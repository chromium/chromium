// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_resource_request_blocked_reason.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/process_state.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/grit/components_resources.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#endif

using base::JSONWriter;
using content::kUnreachableWebDataURL;
using content::RenderFrame;
using content::RenderFrameObserver;
using content::RenderThread;
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

#if BUILDFLAG(IS_ANDROID)
bool IsOfflineContentOnNetErrorFeatureEnabled() {
  return  base::FeatureList::IsEnabled(features::kOfflineContentOnNetError);
}
#else   // BUILDFLAG(IS_ANDROID)
bool IsOfflineContentOnNetErrorFeatureEnabled() {
  return false;
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
bool IsAutoFetchFeatureEnabled() {
  return  base::FeatureList::IsEnabled(features::kOfflineAutoFetch);
}
#else   // BUILDFLAG(IS_ANDROID)
bool IsAutoFetchFeatureEnabled() {
  return false;
}
#endif  // BUILDFLAG(IS_ANDROID)

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

  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<chrome::mojom::NetworkDiagnosticsClient>(
          base::BindRepeating(
              &NetErrorHelper::OnNetworkDiagnosticsClientRequest,
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

void NetErrorHelper::PrepareErrorPage(
    const error_page::Error& error,
    bool is_failed_post,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  core_->PrepareErrorPage(GetFrameType(render_frame()), error, is_failed_post,
                          std::move(alternative_error_page_info), error_html);
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
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  error_html->clear();
  int resource_id = IDR_NET_ERROR_HTML;
  LocalizedError::PageState page_state;
  // If the user is viewing an offline web app then a default page is shown
  // rather than the dino.

  if (alternative_error_page_info &&
      alternative_error_page_info->alternative_error_page_params
          .FindBool(error_page::kOverrideErrorPage)
          .value_or(false)) {
    base::UmaHistogramSparse("Net.ErrorPageCounts.WebAppAlternativeErrorPage",
                             -error.reason());
    resource_id = alternative_error_page_info->resource_id;
    page_state = LocalizedError::GetPageStateForOverriddenErrorPage(
        std::move(alternative_error_page_info->alternative_error_page_params),
        error.reason(), error.domain(), error.url(),
        RenderThread::Get()->GetLocale());
  } else {
    if (alternative_error_page_info) {
      error_page_params_ =
          alternative_error_page_info->alternative_error_page_params.Clone();
    } else {
      error_page_params_.clear();
    }
    page_state = LocalizedError::GetPageState(
        error.reason(), error.domain(), error.url(), is_failed_post,
        error.resolve_error_info().is_secure_network_error,
        error.stale_copy_in_cache(), can_show_network_diagnostics_dialog,
        IsIncognitoProcess(), IsOfflineContentOnNetErrorFeatureEnabled(),
        IsAutoFetchFeatureEnabled(), IsRunningInForcedAppMode(),
        RenderThread::Get()->GetLocale(),
        IsExtensionExtendedErrorCode(error.extended_reason()),
        &error_page_params_);
  }
  std::string extracted_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          resource_id);
  std::string_view template_html(extracted_string.data(),
                                 extracted_string.size());
  DCHECK(!template_html.empty()) << "unable to load template.";
  *error_html = webui::GetLocalizedHtml(template_html, page_state.strings);
  return page_state;
}

void NetErrorHelper::EnablePageHelperFunctions() {
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
      IsIncognitoProcess(), IsOfflineContentOnNetErrorFeatureEnabled(),
      IsAutoFetchFeatureEnabled(), IsRunningInForcedAppMode(),
      RenderThread::Get()->GetLocale(),
      IsExtensionExtendedErrorCode(error.extended_reason()),
      &error_page_params_);

  std::string json;
  JSONWriter::Write(page_state.strings, &json);

  std::string js = "if (window.updateForDnsProbe) "
                   "updateForDnsProbe(" + json + ");";
  std::u16string js16;
  if (base::UTF8ToUTF16(js.c_str(), js.length(), &js16)) {
    render_frame()->ExecuteJavaScript(js16);
  } else {
    NOTREACHED_IN_MIGRATION();
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
    NOTREACHED_IN_MIGRATION();
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

void NetErrorHelper::PortalSignin() {
#if BUILDFLAG(IS_CHROMEOS)
  GetRemoteNetErrorPageSupport()->ShowPortalSignin();
#endif
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
#if BUILDFLAG(IS_ANDROID)
  if (!offline_content_json.empty()) {
    std::string isShownParam(list_visible_by_prefs ? "true" : "false");
    render_frame()->ExecuteJavaScript(base::UTF8ToUTF16(
        base::StrCat({"offlineContentAvailable(", isShownParam, ", ",
                      offline_content_json, ");"})));
  }
#endif
}

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

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
