// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/metrics/structured/event_logging_features.h"
// TODO(crbug/1125897): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;

int64_t ToLong(web_app::WebAppInstallStatus web_app_install_status) {
  return static_cast<int64_t>(web_app_install_status);
}
#endif

}  // namespace

WebAppInstallDialogDelegate::WebAppInstallDialogDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    InstallDialogType dialog_type)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      install_info_(std::move(web_app_info)),
      install_tracker_(std::move(install_tracker)),
      callback_(std::move(callback)),
      iph_state_(std::move(iph_state)),
      prefs_(prefs),
      tracker_(tracker),
      dialog_type_(dialog_type) {
  CHECK(install_info_);
  CHECK(install_info_->manifest_id.is_valid());
  CHECK(install_tracker_);
  CHECK(prefs_);
}

WebAppInstallDialogDelegate::~WebAppInstallDialogDelegate() {
  // TODO(crbug.com/1327363): move this to dialog->SetHighlightedButton.
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  if (browser_view && browser_view->toolbar_button_provider()) {
    PageActionIconView* install_icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kPwaInstall);
    if (install_icon) {
      // Dehighlight the install icon when this dialog is closed.
      browser_view->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kPwaInstall)
          ->SetHighlighted(false);
    }
  }
}

void WebAppInstallDialogDelegate::OnAccept() {
  const char* metric_to_measure = (dialog_type_ == InstallDialogType::kDetailed)
                                      ? "WebAppDetailedInstallAccepted"
                                      : "WebAppInstallAccepted";
  base::RecordAction(base::UserMetricsAction(metric_to_measure));
  if (iph_state_ == PwaInProductHelpState::kShown) {
    webapps::AppId app_id =
        GenerateAppIdFromManifestId(install_info_->manifest_id);
    WebAppPrefGuardrails::GetForDesktopInstallIph(prefs_).RecordAccept(app_id);
    tracker_->NotifyEvent(feature_engagement::events::kDesktopPwaInstalled);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(metrics::structured::kAppDiscoveryLogging)) {
    const webapps::AppId app_id =
        web_app::GenerateAppIdFromManifestId(install_info_->manifest_id);
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::AppDiscovery_Browser_AppInstallDialogResult()
            .SetWebAppInstallStatus(
                ToLong(web_app::WebAppInstallStatus::kAccepted))
            .SetAppId(app_id));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  CHECK(callback_);
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kAccepted);
  std::move(callback_).Run(true, std::move(install_info_));
}

void WebAppInstallDialogDelegate::OnCancel() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kCancelled);
  MeasureIphOnDialogClose();
}

void WebAppInstallDialogDelegate::OnClose() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  MeasureIphOnDialogClose();
}

void WebAppInstallDialogDelegate::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    CloseDialogAsIgnored();
  }
}

void WebAppInstallDialogDelegate::WebContentsDestroyed() {
  CloseDialogAsIgnored();
}

void WebAppInstallDialogDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Close dialog when navigating to a different domain.
  if (!url::IsSameOriginWith(
          navigation_handle->GetPreviousPrimaryMainFrameURL(),
          navigation_handle->GetURL())) {
    CloseDialogAsIgnored();
  }
}

void WebAppInstallDialogDelegate::CloseDialogAsIgnored() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  if (dialog_model() && dialog_model()->host()) {
    dialog_model()->host()->Close();
  }
}

void WebAppInstallDialogDelegate::MeasureIphOnDialogClose() {
  if (callback_.is_null()) {
    return;
  }

  const char* metric_to_measure = (dialog_type_ == InstallDialogType::kDetailed)
                                      ? "WebAppDetailedInstallCancelled"
                                      : "WebAppInstallCancelled";
  base::RecordAction(base::UserMetricsAction(metric_to_measure));

  if (iph_state_ == PwaInProductHelpState::kShown && install_info_) {
    webapps::AppId app_id =
        GenerateAppIdFromManifestId(install_info_->manifest_id);
    WebAppPrefGuardrails::GetForDesktopInstallIph(prefs_).RecordIgnore(
        app_id, base::Time::Now());
  }

  // If |install_info_| is populated, then the dialog was not accepted.
  if (install_info_) {
#if BUILDFLAG(IS_CHROMEOS)
    if (base::FeatureList::IsEnabled(
            metrics::structured::kAppDiscoveryLogging)) {
      const webapps::AppId app_id =
          web_app::GenerateAppIdFromManifestId(install_info_->manifest_id);
      metrics::structured::StructuredMetricsClient::Record(
          cros_events::AppDiscovery_Browser_AppInstallDialogResult()
              .SetWebAppInstallStatus(
                  ToLong(web_app::WebAppInstallStatus::kCancelled))
              .SetAppId(app_id));
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    std::move(callback_).Run(false, std::move(install_info_));
  }
}

}  // namespace web_app
