// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/ssl/ssl_certificate_error_page_controller.h"
#include "chrome/renderer/supervised_user/supervised_user_error_page_controller.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common/interfaces/interstitial_commands.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/resource_fetcher.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
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
using OfflineContentOnNetErrorFeatureState =
    LocalizedError::OfflineContentOnNetErrorFeatureState;

namespace {

// Number of seconds to wait for the navigation correction service to return
// suggestions.  If it takes too long, just use the local error page.
const int kNavigationCorrectionFetchTimeoutSec = 3;

NetErrorHelperCore::PageType GetLoadingPageType(
    blink::WebDocumentLoader* document_loader) {
  GURL url = document_loader->GetRequest().Url();
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
OfflineContentOnNetErrorFeatureState GetOfflineContentOnNetErrorFeatureState() {
  if (!base::FeatureList::IsEnabled(features::kNewNetErrorPageUI))
    return OfflineContentOnNetErrorFeatureState::kDisabled;
  const std::string alternate_ui_name = base::GetFieldTrialParamValueByFeature(
      features::kNewNetErrorPageUI,
      features::kNewNetErrorPageUIAlternateParameterName);
  if (alternate_ui_name == features::kNewNetErrorPageUIAlternateContentList) {
    return OfflineContentOnNetErrorFeatureState::kEnabledList;
  }
  return OfflineContentOnNetErrorFeatureState::kEnabledSummary;
}
#else   // OS_ANDROID
OfflineContentOnNetErrorFeatureState GetOfflineContentOnNetErrorFeatureState() {
  return OfflineContentOnNetErrorFeatureState::kDisabled;
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
      content::RenderFrameObserverTracker<NetErrorHelper>(render_frame),
      weak_controller_delegate_factory_(this),
      weak_ssl_error_controller_delegate_factory_(this),
      weak_supervised_user_error_controller_delegate_factory_(this) {
  RenderThread::Get()->AddObserver(this);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool auto_reload_enabled =
      command_line->HasSwitch(switches::kEnableOfflineAutoReload);
  bool auto_reload_visible_only =
      command_line->HasSwitch(switches::kEnableOfflineAutoReloadVisibleOnly);
  // TODO(mmenke): Consider only creating a NetErrorHelperCore for main frames.
  // subframes don't need any of the NetErrorHelperCore's extra logic.
  core_.reset(new NetErrorHelperCore(this,
                                     auto_reload_enabled,
                                     auto_reload_visible_only,
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

void NetErrorHelper::SendCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  security_interstitials::mojom::InterstitialCommandsAssociatedPtr interface;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&interface);
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED: {
      interface->DontProceed();
      break;
    }
    case security_interstitials::CMD_PROCEED: {
      interface->Proceed();
      break;
    }
    case security_interstitials::CMD_SHOW_MORE_SECTION: {
      interface->ShowMoreSection();
      break;
    }
    case security_interstitials::CMD_OPEN_HELP_CENTER: {
      interface->OpenHelpCenter();
      break;
    }
    case security_interstitials::CMD_OPEN_DIAGNOSTIC: {
      interface->OpenDiagnostic();
      break;
    }
    case security_interstitials::CMD_RELOAD: {
      interface->Reload();
      break;
    }
    case security_interstitials::CMD_OPEN_DATE_SETTINGS: {
      interface->OpenDateSettings();
      break;
    }
    case security_interstitials::CMD_OPEN_LOGIN: {
      interface->OpenLogin();
      break;
    }
    case security_interstitials::CMD_DO_REPORT: {
      interface->DoReport();
      break;
    }
    case security_interstitials::CMD_DONT_REPORT: {
      interface->DontReport();
      break;
    }
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY: {
      interface->OpenReportingPrivacy();
      break;
    }
    case security_interstitials::CMD_OPEN_WHITEPAPER: {
      interface->OpenWhitepaper();
      break;
    }
    case security_interstitials::CMD_REPORT_PHISHING_ERROR: {
      interface->ReportPhishingError();
      break;
    }
    default: {
      // Other values in the enum are only used by tests so this
      // method should not be called with them.
      NOTREACHED();
    }
  }
}

void NetErrorHelper::GoBack() {
  if (supervised_user_interface_)
    supervised_user_interface_->GoBack();
}

void NetErrorHelper::RequestPermission(
    base::OnceCallback<void(bool)> callback) {
  if (supervised_user_interface_)
    supervised_user_interface_->RequestPermission(std::move(callback));
}

void NetErrorHelper::Feedback() {
  if (supervised_user_interface_)
    supervised_user_interface_->Feedback();
}

void NetErrorHelper::DidStartProvisionalLoad(
    blink::WebDocumentLoader* document_loader,
    bool is_content_initiated) {
  core_->OnStartLoad(GetFrameType(render_frame()),
                     GetLoadingPageType(document_loader));
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
  weak_ssl_error_controller_delegate_factory_.InvalidateWeakPtrs();
  weak_supervised_user_error_controller_delegate_factory_.InvalidateWeakPtrs();

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
                                      bool is_ignoring_cache,
                                      std::string* error_html) {
  core_->PrepareErrorPage(GetFrameType(render_frame()), error, is_failed_post,
                          is_ignoring_cache, error_html);
}

bool NetErrorHelper::ShouldSuppressErrorPage(const GURL& url) {
  return core_->ShouldSuppressErrorPage(GetFrameType(render_frame()), url);
}

chrome::mojom::NetworkDiagnostics*
NetErrorHelper::GetRemoteNetworkDiagnostics() {
  if (!remote_network_diagnostics_) {
    render_frame()->GetRemoteAssociatedInterfaces()
        ->GetInterface(&remote_network_diagnostics_);
  }
  return remote_network_diagnostics_.get();
}

void NetErrorHelper::GenerateLocalizedErrorPage(
    const error_page::Error& error,
    bool is_failed_post,
    bool can_show_network_diagnostics_dialog,
    std::unique_ptr<ErrorPageParams> params,
    bool* reload_button_shown,
    bool* show_saved_copy_button_shown,
    bool* show_cached_copy_button_shown,
    bool* download_button_shown,
    OfflineContentOnNetErrorFeatureState* offline_content_feature_state,
    std::string* error_html) const {
  error_html->clear();

  int resource_id = IDR_NET_ERROR_HTML;
  const base::StringPiece template_html(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  if (template_html.empty()) {
    NOTREACHED() << "unable to load template.";
  } else {
    base::DictionaryValue error_strings;
    *offline_content_feature_state = GetOfflineContentOnNetErrorFeatureState();
    LocalizedError::GetStrings(
        error.reason(), error.domain(), error.url(), is_failed_post,
        error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
        ChromeRenderThreadObserver::is_incognito_process(),
        *offline_content_feature_state, RenderThread::Get()->GetLocale(),
        std::move(params), &error_strings);
    *reload_button_shown = error_strings.Get("reloadButton", nullptr);
    *show_saved_copy_button_shown =
        error_strings.Get("showSavedCopyButton", nullptr);
    *show_cached_copy_button_shown =
        error_strings.Get("cacheButton", nullptr);
    *download_button_shown =
        error_strings.Get("downloadButton", nullptr);
    if (!error_strings.Get("suggestedOfflineContentPresentationMode",
                           nullptr)) {
      *offline_content_feature_state =
          OfflineContentOnNetErrorFeatureState::kDisabled;
    }

    // "t" is the id of the template's root node.
    *error_html = webui::GetTemplatesHtml(template_html, &error_strings, "t");
  }
}

void NetErrorHelper::LoadErrorPage(const std::string& html,
                                   const GURL& failed_url) {
  render_frame()->GetWebFrame()->CommitDataNavigation(
      blink::WebURLRequest(GURL(kUnreachableWebDataURL)), blink::WebData(html),
      blink::WebString::FromUTF8("text/html"),
      blink::WebString::FromUTF8("UTF-8"), failed_url,
      blink::WebFrameLoadType::kReplaceCurrentItem, blink::WebHistoryItem(),
      false /* is_client_redirect */, nullptr, nullptr);
}

void NetErrorHelper::EnablePageHelperFunctions(net::Error net_error) {
  if (net::IsCertificateError(net_error)) {
    SSLCertificateErrorPageController::Install(
        render_frame(),
        weak_ssl_error_controller_delegate_factory_.GetWeakPtr());
    return;
  }
  NetErrorPageController::Install(
      render_frame(), weak_controller_delegate_factory_.GetWeakPtr());

  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &supervised_user_interface_);
  SupervisedUserErrorPageController::Install(
      render_frame(),
      weak_supervised_user_error_controller_delegate_factory_.GetWeakPtr());
}

void NetErrorHelper::UpdateErrorPage(const error_page::Error& error,
                                     bool is_failed_post,
                                     bool can_show_network_diagnostics_dialog) {
  base::DictionaryValue error_strings;
  LocalizedError::GetStrings(
      error.reason(), error.domain(), error.url(), is_failed_post,
      error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
      ChromeRenderThreadObserver::is_incognito_process(),
      GetOfflineContentOnNetErrorFeatureState(),
      RenderThread::Get()->GetLocale(), std::unique_ptr<ErrorPageParams>(),
      &error_strings);

  std::string json;
  JSONWriter::Write(error_strings, &json);

  std::string js = "if (window.updateForDnsProbe) "
                   "updateForDnsProbe(" + json + ");";
  base::string16 js16;
  if (!base::UTF8ToUTF16(js.c_str(), js.length(), &js16)) {
    NOTREACHED();
    return;
  }

  render_frame()->ExecuteJavaScript(js16);
}

void NetErrorHelper::FetchNavigationCorrections(
    const GURL& navigation_correction_url,
    const std::string& navigation_correction_request_body) {
  DCHECK(!correction_fetcher_.get());

  correction_fetcher_ =
      content::ResourceFetcher::Create(navigation_correction_url);
  correction_fetcher_->SetMethod("POST");
  correction_fetcher_->SetBody(navigation_correction_request_body);
  correction_fetcher_->SetHeader("Content-Type", "application/json");

  // Prevent CORB from triggering on this request by setting an Origin header.
  correction_fetcher_->SetHeader("Origin", "null");

  correction_fetcher_->Start(
      render_frame()->GetWebFrame(), blink::mojom::RequestContextType::INTERNAL,
      render_frame()->GetURLLoaderFactory(), GetNetworkTrafficAnnotationTag(),
      base::BindOnce(&NetErrorHelper::OnNavigationCorrectionsFetched,
                     base::Unretained(this)));

  correction_fetcher_->SetTimeout(
      base::TimeDelta::FromSeconds(kNavigationCorrectionFetchTimeoutSec));
}

void NetErrorHelper::CancelFetchNavigationCorrections() {
  correction_fetcher_.reset();
}

void NetErrorHelper::SendTrackingRequest(
    const GURL& tracking_url,
    const std::string& tracking_request_body) {
  // If there's already a pending tracking request, this will cancel it.
  tracking_fetcher_ = content::ResourceFetcher::Create(tracking_url);
  tracking_fetcher_->SetMethod("POST");
  tracking_fetcher_->SetBody(tracking_request_body);
  tracking_fetcher_->SetHeader("Content-Type", "application/json");

  tracking_fetcher_->Start(
      render_frame()->GetWebFrame(), blink::mojom::RequestContextType::INTERNAL,
      render_frame()->GetURLLoaderFactory(), GetNetworkTrafficAnnotationTag(),
      base::BindOnce(&NetErrorHelper::OnTrackingRequestComplete,
                     base::Unretained(this)));
}

void NetErrorHelper::ReloadPage(bool bypass_cache) {
  render_frame()->GetWebFrame()->StartReload(
      bypass_cache ? blink::WebFrameLoadType::kReloadBypassingCache
                   : blink::WebFrameLoadType::kReload);
}

void NetErrorHelper::LoadPageFromCache(const GURL& page_url) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK_NE("POST",
            web_frame->GetDocumentLoader()->GetRequest().HttpMethod().Ascii());

  blink::WebURLRequest request(page_url);
  request.SetCacheMode(blink::mojom::FetchCacheMode::kOnlyIfCached);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::Create(page_url));
  web_frame->StartNavigation(request);
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
    const std::string& offline_content_json) {
#if defined(OS_ANDROID)
  if (!offline_content_json.empty()) {
    render_frame()->ExecuteJavaScript(base::UTF8ToUTF16(base::StrCat(
        {"offlineContentAvailable(", offline_content_json, ");"})));
  }
#endif
}

void NetErrorHelper::OfflineContentSummaryAvailable(
    const std::string& offline_content_summary_json) {
#if defined(OS_ANDROID)
  if (!offline_content_summary_json.empty()) {
    render_frame()->ExecuteJavaScript(
        base::UTF8ToUTF16(base::StrCat({"offlineContentSummaryAvailable(",
                                        offline_content_summary_json, ");"})));
  }
#endif
}

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
    const blink::WebURLResponse& response,
    const std::string& data) {
  // The fetcher may only be deleted after |data| is passed to |core_|.  Move
  // it to a temporary to prevent any potential re-entrancy issues.
  std::unique_ptr<content::ResourceFetcher> fetcher(
      correction_fetcher_.release());
  bool success = (!response.IsNull() && response.HttpStatusCode() == 200);
  core_->OnNavigationCorrectionsFetched(success ? data : "",
                                        base::i18n::IsRTL());
}

void NetErrorHelper::OnTrackingRequestComplete(
    const blink::WebURLResponse& response,
    const std::string& data) {
  tracking_fetcher_.reset();
}

void NetErrorHelper::OnNetworkDiagnosticsClientRequest(
    chrome::mojom::NetworkDiagnosticsClientAssociatedRequest request) {
  network_diagnostics_client_bindings_.AddBinding(this, std::move(request));
}

void NetErrorHelper::OnNavigationCorrectorRequest(
    chrome::mojom::NavigationCorrectorAssociatedRequest request) {
  navigation_corrector_bindings_.AddBinding(this, std::move(request));
}

void NetErrorHelper::SetCanShowNetworkDiagnosticsDialog(bool can_show) {
  core_->OnSetCanShowNetworkDiagnosticsDialog(can_show);
}
