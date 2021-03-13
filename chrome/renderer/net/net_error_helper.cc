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
#include "chrome/common/chrome_resource_request_blocked_reason.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/grit/components_resources.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
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
using error_page::LocalizedError;

namespace {

NetErrorHelperCore::FrameType GetFrameType(RenderFrame* render_frame) {
  if (render_frame->IsMainFrame())
    return NetErrorHelperCore::MAIN_FRAME;
  return NetErrorHelperCore::SUB_FRAME;
}

bool IsExtensionExtendedErrorCode(int extended_error_code) {
  return extended_error_code ==
         static_cast<int>(ChromeResourceRequestBlockedReason::kExtension);
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

bool IsRunningInForcedAppMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceAppMode);
}

}  // namespace

NetErrorHelper::NetErrorHelper(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<NetErrorHelper>(render_frame) {
  // TODO(mmenke): Consider only creating a NetErrorHelperCore for main frames.
  // subframes don't need any of the NetErrorHelperCore's extra logic.
  core_ = std::make_unique<NetErrorHelperCore>(this);

  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&NetErrorHelper::OnNetworkDiagnosticsClientRequest,
                          base::Unretained(this)));
}

NetErrorHelper::~NetErrorHelper() = default;

void NetErrorHelper::ButtonPressed(NetErrorHelperCore::Button button) {
  core_->ExecuteButtonPress(button);
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

void NetErrorHelper::DidCommitProvisionalLoad(ui::PageTransition transition) {
  // Invalidate weak pointers from the old error page controller. If loading a
  // new error page, the controller has not yet been attached, so this won't
  // affect it.
  weak_controller_delegate_factory_.InvalidateWeakPtrs();

  core_->OnCommitLoad(GetFrameType(render_frame()),
                      render_frame()->GetWebFrame()->GetDocument().Url());
}

void NetErrorHelper::DidFinishLoad() {
  core_->OnFinishLoad(GetFrameType(render_frame()));
}

void NetErrorHelper::OnDestruct() {
  delete this;
}

void NetErrorHelper::PrepareErrorPage(const error_page::Error& error,
                                      bool is_failed_post,
                                      std::string* error_html) {
  core_->PrepareErrorPage(GetFrameType(render_frame()), error, is_failed_post,
                          error_html);
}

std::unique_ptr<network::ResourceRequest> NetErrorHelper::CreatePostRequest(
    const GURL& url) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->destination = network::mojom::RequestDestination::kEmpty;
  resource_request->resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  resource_request->site_for_cookies = frame->GetDocument().SiteForCookies();
  // The security origin of the error page should exist and be opaque.
  DCHECK(!frame->GetDocument().GetSecurityOrigin().IsNull());
  DCHECK(frame->GetDocument().GetSecurityOrigin().IsOpaque());
  // All requests coming from a renderer process have to use |request_initiator|
  // that matches the |request_initiator_origin_lock| set by the browser when
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

chrome::mojom::NetErrorPageSupport*
NetErrorHelper::GetRemoteNetErrorPageSupport() {
  if (!remote_net_error_page_support_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &remote_net_error_page_support_);
  }
  return remote_net_error_page_support_.get();
}

LocalizedError::PageState NetErrorHelper::GenerateLocalizedErrorPage(
    const error_page::Error& error,
    bool is_failed_post,
    bool can_show_network_diagnostics_dialog,
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
      error.resolve_error_info().is_secure_network_error,
      error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
      ChromeRenderThreadObserver::is_incognito_process(),
      IsOfflineContentOnNetErrorFeatureEnabled(), IsAutoFetchFeatureEnabled(),
      IsRunningInForcedAppMode(), RenderThread::Get()->GetLocale(),
      IsExtensionExtendedErrorCode(error.extended_reason()));
  DCHECK(!template_html.empty()) << "unable to load template.";
  // "t" is the id of the template's root node.
  *error_html =
      webui::GetTemplatesHtml(template_html, &page_state.strings, "t");
  return page_state;
}

void NetErrorHelper::EnablePageHelperFunctions() {
  security_interstitials::SecurityInterstitialPageController::Install(
      render_frame());
  NetErrorPageController::Install(
      render_frame(), weak_controller_delegate_factory_.GetWeakPtr());
}

LocalizedError::PageState NetErrorHelper::UpdateErrorPage(
    const error_page::Error& error,
    bool is_failed_post,
    bool can_show_network_diagnostics_dialog) {
  LocalizedError::PageState page_state = LocalizedError::GetPageState(
      error.reason(), error.domain(), error.url(), is_failed_post,
      error.resolve_error_info().is_secure_network_error,
      error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
      ChromeRenderThreadObserver::is_incognito_process(),
      IsOfflineContentOnNetErrorFeatureEnabled(), IsAutoFetchFeatureEnabled(),
      IsRunningInForcedAppMode(), RenderThread::Get()->GetLocale(),
      IsExtensionExtendedErrorCode(error.extended_reason()));

  std::string json;
  JSONWriter::Write(page_state.strings, &json);

  std::string js = "if (window.updateForDnsProbe) "
                   "updateForDnsProbe(" + json + ");";
  std::u16string js16;
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
  std::u16string js16;
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

void NetErrorHelper::ReloadFrame() {
  render_frame()->GetWebFrame()->StartReload(blink::WebFrameLoadType::kReload);
}

void NetErrorHelper::DiagnoseError(const GURL& page_url) {
  GetRemoteNetworkDiagnostics()->RunNetworkDiagnostics(page_url);
}

void NetErrorHelper::DownloadPageLater() {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  GetRemoteNetErrorPageSupport()->DownloadPageLater();
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
}

void NetErrorHelper::SetIsShowingDownloadButton(bool show) {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  GetRemoteNetErrorPageSupport()->SetIsShowingDownloadButtonInErrorPage(show);
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
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

void NetErrorHelper::OnNetworkDiagnosticsClientRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::NetworkDiagnosticsClient>
        receiver) {
  network_diagnostics_client_receivers_.Add(this, std::move(receiver));
}

void NetErrorHelper::SetCanShowNetworkDiagnosticsDialog(bool can_show) {
  core_->OnSetCanShowNetworkDiagnosticsDialog(can_show);
}
