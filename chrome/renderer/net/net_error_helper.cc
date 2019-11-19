// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/grit/components_resources.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/common/mojom/interstitial_commands.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#endif

using base::JSONWriter;
using content::DocumentState;
using content::RenderFrame;
using content::RenderFrameObserver;
using content::RenderThread;
using content::kUnreachableWebDataURL;
using error_page::DnsProbeStatus;
using error_page::DnsProbeStatusToString;
using error_page::ErrorPageParams;
using error_page::LocalizedError;

namespace {

// Number of seconds to wait for the navigation correction service to return
// suggestions.  If it takes too long, just use the local error page.
const int kNavigationCorrectionFetchTimeoutSec = 3;

NetErrorHelperCore::PageType GetLoadingPageType(const GURL& url) {
  if (!url.is_valid() || url.spec() != kUnreachableWebDataURL)
    return NetErrorHelperCore::NON_ERROR_PAGE;
  return NetErrorHelperCore::ERROR_PAGE;
}

NetErrorHelperCore::FrameType GetFrameType(RenderFrame* render_frame) {
  if (render_frame->IsMainFrame())
    return NetErrorHelperCore::MAIN_FRAME;
  return NetErrorHelperCore::SUB_FRAME;
}

#if defined(OS_ANDROID)
bool IsOfflineContentOnNetErrorFeatureEnabled() {
  return true;
}
#else   // OS_ANDROID
bool IsOfflineContentOnNetErrorFeatureEnabled() {
  return false;
}
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
bool IsAutoFetchFeatureEnabled() {
  return true;
}
#else   // OS_ANDROID
bool IsAutoFetchFeatureEnabled() {
  return false;
}
#endif  // OS_ANDROID

const net::NetworkTrafficAnnotationTag& GetNetworkTrafficAnnotationTag() {
  static const net::NetworkTrafficAnnotationTag network_traffic_annotation_tag =
      net::DefineNetworkTrafficAnnotation("net_error_helper", R"(
    semantics {
      sender: "NetErrorHelper"
      description:
        "Chrome asks Link Doctor service when a navigating page returns an "
        "error to investigate details about what is wrong."
      trigger:
        "When Chrome navigates to a page, and the page returns an error."
      data:
        "Failed page information including the URL will be sent to the service."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "You can enable or disable this feature via 'Use a web service to help "
        "resolve navigation errors' in Chrome's settings under Advanced. The "
        "feature is enabled by default."
      chrome_policy {
        AlternateErrorPagesEnabled {
          policy_options {mode: MANDATORY}
          AlternateErrorPagesEnabled: false
        }
      }
    })");
  return network_traffic_annotation_tag;
}

}  // namespace

NetErrorHelper::NetErrorHelper(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<NetErrorHelper>(render_frame) {
  RenderThread::Get()->AddObserver(this);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool auto_reload_enabled =
      command_line->HasSwitch(switches::kEnableAutoReload);
  // TODO(mmenke): Consider only creating a NetErrorHelperCore for main frames.
  // subframes don't need any of the NetErrorHelperCore's extra logic.
  core_.reset(new NetErrorHelperCore(this,
                                     auto_reload_enabled,
                                     !render_frame->IsHidden()));

  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::Bind(&NetErrorHelper::OnNetworkDiagnosticsClientRequest,
                 base::Unretained(this)));
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(base::Bind(
      &NetErrorHelper::OnNavigationCorrectorRequest, base::Unretained(this)));
}

NetErrorHelper::~NetErrorHelper() {
  RenderThread::Get()->RemoveObserver(this);
}

void NetErrorHelper::ButtonPressed(NetErrorHelperCore::Button button) {
  core_->ExecuteButtonPress(button);
}

void NetErrorHelper::TrackClick(int tracking_id) {
  core_->TrackClick(tracking_id);
}

void NetErrorHelper::LaunchOfflineItem(const std::string& id,
                                       const std::string& name_space) {
  core_->LaunchOfflineItem(id, name_space);
}

void NetErrorHelper::LaunchDownloadsPage() {
  core_->LaunchDownloadsPage();
}

void NetErrorHelper::SavePageForLater() {
  core_->SavePageForLater();
}

void NetErrorHelper::CancelSavePage() {
  core_->CancelSavePage();
}

void NetErrorHelper::ListVisibilityChanged(bool is_visible) {
  core_->ListVisibilityChanged(is_visible);
}

content::RenderFrame* NetErrorHelper::GetRenderFrame() {
  return render_frame();
}

mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
NetErrorHelper::GetInterface() {
  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
      interface;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&interface);
  return interface;
}

void NetErrorHelper::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  core_->OnStartLoad(GetFrameType(render_frame()), GetLoadingPageType(url));
}

void NetErrorHelper::DidCommitProvisionalLoad(bool is_same_document_navigation,
                                              ui::PageTransition transition) {
  // If this is a "same-document" navigation, it's not a real navigation.  There
  // wasn't a start event for it, either, so just ignore it.
  if (is_same_document_navigation)
    return;

  // Invalidate weak pointers from old error page controllers. If loading a new
  // error page, the controller has not yet been attached, so this won't affect
  // it.
  weak_controller_delegate_factory_.InvalidateWeakPtrs();
  weak_security_interstitial_controller_delegate_factory_.InvalidateWeakPtrs();

  core_->OnCommitLoad(GetFrameType(render_frame()),
                      render_frame()->GetWebFrame()->GetDocument().Url());
}

void NetErrorHelper::DidFinishLoad() {
  core_->OnFinishLoad(GetFrameType(render_frame()));
}

void NetErrorHelper::OnStop() {
  core_->OnStop();
}

void NetErrorHelper::WasShown() {
  core_->OnWasShown();
}

void NetErrorHelper::WasHidden() {
  core_->OnWasHidden();
}

void NetErrorHelper::OnDestruct() {
  delete this;
}

void NetErrorHelper::NetworkStateChanged(bool enabled) {
  core_->NetworkStateChanged(enabled);
}

void NetErrorHelper::PrepareErrorPage(const error_page::Error& error,
                                      bool is_failed_post,
                                      std::string* error_html) {
  core_->PrepareErrorPage(GetFrameType(render_frame()), error, is_failed_post,
                          error_html);
}

bool NetErrorHelper::ShouldSuppressErrorPage(const GURL& url) {
  return core_->ShouldSuppressErrorPage(GetFrameType(render_frame()), url);
}

std::unique_ptr<network::ResourceRequest> NetErrorHelper::CreatePostRequest(
    const GURL& url) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->fetch_request_context_type =
      static_cast<int>(blink::mojom::RequestContextType::INTERNAL);
  resource_request->resource_type =
      static_cast<int>(content::ResourceType::kSubResource);

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  resource_request->site_for_cookies = frame->GetDocument().SiteForCookies();
  // The security origin of the error page should exist and be opaque.
  DCHECK(!frame->GetDocument().GetSecurityOrigin().IsNull());
  DCHECK(frame->GetDocument().GetSecurityOrigin().IsOpaque());
  // All requests coming from a renderer process have to use |request_initiator|
  // that matches the |request_initiator_site_lock| set by the browser when
  // creating URLLoaderFactory exposed to the renderer.
  blink::WebSecurityOrigin origin = frame->GetDocument().GetSecurityOrigin();
  resource_request->request_initiator = static_cast<url::Origin>(origin);
  // Since the page is trying to fetch cross-origin resources (which would
  // be protected by CORB in no-cors mode), we need to ask for CORS.  See also
  // https://crbug.com/932542.
  resource_request->mode = network::mojom::RequestMode::kCors;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                      origin.ToString().Ascii());
  return resource_request;
}

chrome::mojom::NetworkDiagnostics*
NetErrorHelper::GetRemoteNetworkDiagnostics() {
  if (!remote_network_diagnostics_) {
    render_frame()->GetRemoteAssociatedInterfaces()
        ->GetInterface(&remote_network_diagnostics_);
  }
  return remote_network_diagnostics_.get();
}

chrome::mojom::NetworkEasterEgg* NetErrorHelper::GetRemoteNetworkEasterEgg() {
  if (!remote_network_easter_egg_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &remote_network_easter_egg_);
  }
  return remote_network_easter_egg_.get();
}

LocalizedError::PageState NetErrorHelper::GenerateLocalizedErrorPage(
    const error_page::Error& error,
    bool is_failed_post,
    bool can_show_network_diagnostics_dialog,
    std::unique_ptr<ErrorPageParams> params,
    std::string* error_html) const {
  error_html->clear();

  int resource_id = IDR_NET_ERROR_HTML;
  std::string extracted_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          resource_id);
  base::StringPiece template_html(extracted_string.data(),
                                  extracted_string.size());

  LocalizedError::PageState page_state = LocalizedError::GetPageState(
      error.reason(), error.domain(), error.url(), is_failed_post,
      error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
      ChromeRenderThreadObserver::is_incognito_process(),
      IsOfflineContentOnNetErrorFeatureEnabled(), IsAutoFetchFeatureEnabled(),
      RenderThread::Get()->GetLocale(), std::move(params));
  DCHECK(!template_html.empty()) << "unable to load template.";
  // "t" is the id of the template's root node.
  *error_html =
      webui::GetTemplatesHtml(template_html, &page_state.strings, "t");
  return page_state;
}

void NetErrorHelper::LoadErrorPage(const std::string& html,
                                   const GURL& failed_url) {
  render_frame()->LoadHTMLString(html, GURL(kUnreachableWebDataURL), "UTF-8",
                                 failed_url, true /* replace_current_item */);
}

void NetErrorHelper::EnablePageHelperFunctions() {
  security_interstitials::SecurityInterstitialPageController::Install(
      render_frame(),
      weak_security_interstitial_controller_delegate_factory_.GetWeakPtr());
  NetErrorPageController::Install(
      render_frame(), weak_controller_delegate_factory_.GetWeakPtr());
}

LocalizedError::PageState NetErrorHelper::UpdateErrorPage(
    const error_page::Error& error,
    bool is_failed_post,
    bool can_show_network_diagnostics_dialog) {
  LocalizedError::PageState page_state = LocalizedError::GetPageState(
      error.reason(), error.domain(), error.url(), is_failed_post,
      error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
      ChromeRenderThreadObserver::is_incognito_process(),
      IsOfflineContentOnNetErrorFeatureEnabled(), IsAutoFetchFeatureEnabled(),
      RenderThread::Get()->GetLocale(), std::unique_ptr<ErrorPageParams>());

  std::string json;
  JSONWriter::Write(page_state.strings, &json);

  std::string js = "if (window.updateForDnsProbe) "
                   "updateForDnsProbe(" + json + ");";
  base::string16 js16;
  if (base::UTF8ToUTF16(js.c_str(), js.length(), &js16)) {
    render_frame()->ExecuteJavaScript(js16);
  } else {
    NOTREACHED();
  }
  return page_state;
}

void NetErrorHelper::InitializeErrorPageEasterEggHighScore(int high_score) {
  std::string js = base::StringPrintf(
      "if (window.initializeEasterEggHighScore) "
      "initializeEasterEggHighScore(%i);",
      high_score);
  base::string16 js16;
  if (!base::UTF8ToUTF16(js.c_str(), js.length(), &js16)) {
    NOTREACHED();
    return;
  }

  render_frame()->ExecuteJavaScript(js16);
}

void NetErrorHelper::RequestEasterEggHighScore() {
  GetRemoteNetworkEasterEgg()->GetHighScore(base::BindOnce(
      [](NetErrorHelper* helper, uint32_t high_score) {
        helper->core_->OnEasterEggHighScoreReceived(high_score);
      },
      base::Unretained(this)));
}

void NetErrorHelper::UpdateEasterEggHighScore(int high_score) {
  GetRemoteNetworkEasterEgg()->UpdateHighScore(high_score);
}

void NetErrorHelper::ResetEasterEggHighScore() {
  GetRemoteNetworkEasterEgg()->ResetHighScore();
}

void NetErrorHelper::FetchNavigationCorrections(
    const GURL& navigation_correction_url,
    const std::string& navigation_correction_request_body) {
  DCHECK(!correction_loader_.get());

  std::unique_ptr<network::ResourceRequest> resource_request =
      CreatePostRequest(navigation_correction_url);

  correction_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotationTag());
  correction_loader_->AttachStringForUpload(navigation_correction_request_body,
                                            "application/json");
  correction_loader_->DownloadToString(
      render_frame()->GetURLLoaderFactory().get(),
      base::BindOnce(&NetErrorHelper::OnNavigationCorrectionsFetched,
                     base::Unretained(this)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
  correction_loader_->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kNavigationCorrectionFetchTimeoutSec));
}

void NetErrorHelper::CancelFetchNavigationCorrections() {
  correction_loader_.reset();
}

void NetErrorHelper::SendTrackingRequest(
    const GURL& tracking_url,
    const std::string& tracking_request_body) {
  // If there's already a pending tracking request, this will cancel it.
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreatePostRequest(tracking_url);

  tracking_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotationTag());
  tracking_loader_->AttachStringForUpload(tracking_request_body,
                                          "application/json");
  tracking_loader_->DownloadToString(
      render_frame()->GetURLLoaderFactory().get(),
      base::BindOnce(&NetErrorHelper::OnTrackingRequestComplete,
                     base::Unretained(this)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void NetErrorHelper::ReloadFrame() {
  render_frame()->GetWebFrame()->StartReload(blink::WebFrameLoadType::kReload);
}

void NetErrorHelper::DiagnoseError(const GURL& page_url) {
  GetRemoteNetworkDiagnostics()->RunNetworkDiagnostics(page_url);
}

void NetErrorHelper::DownloadPageLater() {
#if defined(OS_ANDROID)
  render_frame()->Send(new ChromeViewHostMsg_DownloadPageLater(
      render_frame()->GetRoutingID()));
#endif  // defined(OS_ANDROID)
}

void NetErrorHelper::SetIsShowingDownloadButton(bool show) {
#if defined(OS_ANDROID)
  render_frame()->Send(
      new ChromeViewHostMsg_SetIsShowingDownloadButtonInErrorPage(
          render_frame()->GetRoutingID(), show));
#endif  // defined(OS_ANDROID)
}

void NetErrorHelper::OfflineContentAvailable(
    bool list_visible_by_prefs,
    const std::string& offline_content_json) {
#if defined(OS_ANDROID)
  if (!offline_content_json.empty()) {
    std::string isShownParam(list_visible_by_prefs ? "true" : "false");
    render_frame()->ExecuteJavaScript(base::UTF8ToUTF16(
        base::StrCat({"offlineContentAvailable(", isShownParam, ", ",
                      offline_content_json, ");"})));
  }
#endif
}

#if defined(OS_ANDROID)
void NetErrorHelper::SetAutoFetchState(
    chrome::mojom::OfflinePageAutoFetcherScheduleResult result) {
  const char* scheduled = "false";
  const char* can_schedule = "false";
  switch (result) {
    case chrome::mojom::OfflinePageAutoFetcherScheduleResult::kAlreadyScheduled:
    case chrome::mojom::OfflinePageAutoFetcherScheduleResult::kScheduled:
      scheduled = "true";
      can_schedule = "true";
      break;
    case chrome::mojom::OfflinePageAutoFetcherScheduleResult::kOtherError:
      break;
    case chrome::mojom::OfflinePageAutoFetcherScheduleResult::kNotEnoughQuota:
      can_schedule = "true";
      break;
  }
  render_frame()->ExecuteJavaScript(base::UTF8ToUTF16(base::StrCat(
      {"setAutoFetchState(", scheduled, ", ", can_schedule, ");"})));
}
#endif  // defined(OS_ANDROID)

void NetErrorHelper::DNSProbeStatus(int32_t status_num) {
  DCHECK(status_num >= 0 && status_num < error_page::DNS_PROBE_MAX);

  DVLOG(1) << "Received status " << DnsProbeStatusToString(status_num);

  core_->OnNetErrorInfo(static_cast<DnsProbeStatus>(status_num));
}

void NetErrorHelper::SetNavigationCorrectionInfo(
    const GURL& navigation_correction_url,
    const std::string& language,
    const std::string& country_code,
    const std::string& api_key,
    const GURL& search_url) {
  core_->OnSetNavigationCorrectionInfo(navigation_correction_url, language,
                                       country_code, api_key, search_url);
}

void NetErrorHelper::OnNavigationCorrectionsFetched(
    std::unique_ptr<std::string> response_body) {
  bool success = response_body.get() != nullptr;
  correction_loader_.reset();
  core_->OnNavigationCorrectionsFetched(success ? *response_body : "",
                                        base::i18n::IsRTL());
}

void NetErrorHelper::OnTrackingRequestComplete(
    std::unique_ptr<std::string> response_body) {
  tracking_loader_.reset();
}

void NetErrorHelper::OnNetworkDiagnosticsClientRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::NetworkDiagnosticsClient>
        receiver) {
  network_diagnostics_client_receivers_.Add(this, std::move(receiver));
}

void NetErrorHelper::OnNavigationCorrectorRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::NavigationCorrector>
        receiver) {
  navigation_corrector_receivers_.Add(this, std::move(receiver));
}

void NetErrorHelper::SetCanShowNetworkDiagnosticsDialog(bool can_show) {
  core_->OnSetCanShowNetworkDiagnosticsDialog(can_show);
}
