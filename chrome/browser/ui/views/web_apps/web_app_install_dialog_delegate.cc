// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list_internal.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
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
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
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

std::u16string NormalizeSuggestedAppTitle(const std::u16string& title) {
  std::u16string normalized = title;
  if (base::StartsWith(normalized, u"https://")) {
    normalized = normalized.substr(8);
  }
  if (base::StartsWith(normalized, u"http://")) {
    normalized = normalized.substr(7);
  }
  return normalized;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallDialogDelegate,
                                      kDiyAppsDialogOkButtonId);

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
  CHECK(install_tracker_);
  CHECK(prefs_);
}

WebAppInstallDialogDelegate::~WebAppInstallDialogDelegate() {
  // TODO(crbug.com/40841129): move this to dialog->SetHighlightedButton.
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
      install_icon->SetHighlighted(false);
    }
  }
}

void WebAppInstallDialogDelegate::StartObservingForPictureInPictureOcclusion(
    views::Widget* install_dialog_widget) {
  occlusion_observation_.Observe(install_dialog_widget);
}

void WebAppInstallDialogDelegate::OnAccept() {
  MeasureAcceptUserActionsForInstallDialog();
  if (iph_state_ == PwaInProductHelpState::kShown) {
    webapps::AppId app_id =
        GenerateAppIdFromManifestId(install_info_->manifest_id());
    WebAppPrefGuardrails::GetForDesktopInstallIph(prefs_).RecordAccept(app_id);
    tracker_->NotifyEvent(feature_engagement::events::kDesktopPwaInstalled);
  }

#if BUILDFLAG(IS_CHROMEOS)
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(install_info_->manifest_id());
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_AppInstallDialogResult()
          .SetWebAppInstallStatus(
              ToLong(web_app::WebAppInstallStatus::kAccepted))
          .SetAppId(app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)

  // DIY apps get their name from the DIY install dialog and are always set to
  // open in a new window.
  if (dialog_type_ == InstallDialogType::kDiy) {
    CHECK(!text_field_contents_.empty());
    install_info_->title = text_field_contents_;
    install_info_->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
  }

  CHECK(callback_);
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kAccepted);
  received_user_response_ = true;
  base::UmaHistogramEnumeration(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked);
  std::move(callback_).Run(true, std::move(install_info_));
}

void WebAppInstallDialogDelegate::OnCancel() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kCancelled);
  received_user_response_ = true;
  base::UmaHistogramEnumeration(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked);
  MeasureIphOnDialogClose();
}

void WebAppInstallDialogDelegate::OnClose() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  received_user_response_ = true;

  // This could be hit by triggering the Esc key as well, unfortunately there is
  // no way to listen to that without observing the low level widget.
  base::UmaHistogramEnumeration(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked);
  MeasureIphOnDialogClose();
}

void WebAppInstallDialogDelegate::OnDestroyed() {
  // Only performs histogram measurement and other actions if the dialog was
  // destroyed without user action, like a change in visibility or navigation to
  // a different tab or destruction of the native widget that contains the
  // dialog this delegate is assigned to.
  if (received_user_response_) {
    return;
  }

  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  base::UmaHistogramEnumeration("WebApp.InstallConfirmation.CloseReason",
                                views::Widget::ClosedReason::kUnspecified);
  MeasureIphOnDialogClose();
}

void WebAppInstallDialogDelegate::OnTextFieldChangedMaybeUpdateButton(
    const std::u16string& text_field_contents) {
  text_field_contents_ = text_field_contents;
  ui::DialogModel::Button* ok_button =
      dialog_model()->GetButtonByUniqueId(kDiyAppsDialogOkButtonId);
  CHECK(ok_button);
  dialog_model()->SetButtonEnabled(ok_button,
                                   /*enabled=*/!text_field_contents.empty());
}

void WebAppInstallDialogDelegate::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    CloseDialogAsIgnored();
  }
}

void WebAppInstallDialogDelegate::WebContentsDestroyed() {
  CloseDialogAsIgnored();
}

void WebAppInstallDialogDelegate::PrimaryPageChanged(content::Page& page) {
  CloseDialogAsIgnored();
}

void WebAppInstallDialogDelegate::OnOcclusionStateChanged(bool occluded) {
  // If a picture-in-picture window is occluding the dialog, froce it to close
  // to prevent spoofing.
  if (occluded) {
    PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
  }
}

void WebAppInstallDialogDelegate::CloseDialogAsIgnored() {
  if (!dialog_model() || !dialog_model()->host()) {
    return;
  }
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  dialog_model()->host()->Close();
}

void WebAppInstallDialogDelegate::MeasureIphOnDialogClose() {
  if (callback_.is_null()) {
    return;
  }
  MeasureCancelUserActionsForInstallDialog();
  if (iph_state_ == PwaInProductHelpState::kShown && install_info_) {
    webapps::AppId app_id =
        GenerateAppIdFromManifestId(install_info_->manifest_id());
    WebAppPrefGuardrails::GetForDesktopInstallIph(prefs_).RecordIgnore(
        app_id, base::Time::Now());
  }

  // If |install_info_| is populated, then the dialog was not accepted.
  if (install_info_) {
#if BUILDFLAG(IS_CHROMEOS)
    const webapps::AppId app_id =
        web_app::GenerateAppIdFromManifestId(install_info_->manifest_id());
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::AppDiscovery_Browser_AppInstallDialogResult()
            .SetWebAppInstallStatus(
                ToLong(web_app::WebAppInstallStatus::kCancelled))
            .SetAppId(app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)
    std::move(callback_).Run(false, std::move(install_info_));
  }
}

void WebAppInstallDialogDelegate::MeasureAcceptUserActionsForInstallDialog() {
  const char* accept_dialog_metric_name = nullptr;
  switch (dialog_type_) {
    case InstallDialogType::kDetailed:
      accept_dialog_metric_name = "WebAppDetailedInstallAccepted";
      break;
    case InstallDialogType::kSimple:
      accept_dialog_metric_name = "WebAppInstallAccepted";
      break;
    case InstallDialogType::kDiy:
      accept_dialog_metric_name = "WebAppDiyInstallAccepted";
      break;
  }
  base::RecordAction(base::UserMetricsAction(accept_dialog_metric_name));
}

void WebAppInstallDialogDelegate::MeasureCancelUserActionsForInstallDialog() {
  const char* cancel_dialog_metric_name = nullptr;
  switch (dialog_type_) {
    case InstallDialogType::kDetailed:
      cancel_dialog_metric_name = "WebAppDetailedInstallCancelled";
      break;
    case InstallDialogType::kSimple:
      cancel_dialog_metric_name = "WebAppInstallCancelled";
      break;
    case InstallDialogType::kDiy:
      cancel_dialog_metric_name = "WebAppDiyInstallCancelled";
      break;
  }
  base::RecordAction(base::UserMetricsAction(cancel_dialog_metric_name));
}

}  // namespace web_app
