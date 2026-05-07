// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"

#include <algorithm>
#include <compare>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_flow_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_intro_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_progress_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"
#include "chrome/browser/web_applications/model/dialog_image_info.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace web_app {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kInstallDialogFlowViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kLearnMoreButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kCancelButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallFlowDialogDelegate,
                                      kInstallButton);

std::ostream& operator<<(std::ostream& os, InstallOsType type) {
  switch (type) {
    case InstallOsType::kWin:
      return os << "kWin";
    case InstallOsType::kMac:
      return os << "kMac";
    case InstallOsType::kCros:
      return os << "kCros";
    case InstallOsType::kOther:
      return os << "kOther";
  }
  return os << "Unknown";
}

namespace {

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;

int64_t ToLong(web_app::WebAppInstallStatus web_app_install_status) {
  return static_cast<int64_t>(web_app_install_status);
}
#endif

// The defaulted <=> compares fields in declaration order: not needing
// upscaling beats needing it, and within each group the smaller distance to
// the target wins.
struct BitmapSizeRank {
  bool needs_upscaling;
  SquareSizePx distance_to_target;

  auto operator<=>(const BitmapSizeRank&) const = default;
};

// Selects a bitmap for each supported scale factor, preferring one that
// doesn't require upscaling, and resizes it to the target pixel size.
gfx::ImageSkia CreateResizedIconImage(const UnorderedSizeToBitmap& bitmaps,
                                      int target_size_dp) {
  gfx::ImageSkia resized_image;
  if (bitmaps.empty()) {
    return resized_image;
  }

  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    const int target_size = base::saturated_cast<int>(target_size_dp * scale);

    const auto it = std::ranges::min_element(
        bitmaps, {}, [target_size](const auto& bitmap) -> BitmapSizeRank {
          const SquareSizePx size = bitmap.first;
          return {.needs_upscaling = size < target_size,
                  .distance_to_target = std::abs(size - target_size)};
        });
    SkBitmap bitmap = it->second;
    if (bitmap.width() != target_size) {
      bitmap = skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_BEST, target_size, target_size);
    }
    resized_image.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }
  return resized_image;
}

// Creates a scoped highlight on the corresponding page action icon, if any.
// Returns nullopt if not found.
std::optional<std::variant<views::Button::ScopedAnchorHighlight,
                           page_actions::ScopedPageActionActivity>>
NewPageActionHighlight(content::WebContents& web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(&web_contents);
  if (!tab) {
    return std::nullopt;
  }

  if (IsPageActionMigrated(PageActionIconType::kPwaInstall)) {
    tabs::TabFeatures* tab_features = tab->GetTabFeatures();
    CHECK(tab_features);

    return tab_features->page_action_controller()->AddActivity(
        kActionInstallPwa);
  }

  // TODO(crbug.com/425953501): We shouldn't be using this. Once
  // `ToolbarButtonProvider` is migrated to `BrowserWindowInterface`, we can
  // use that directly.
  Browser* browser =
      tab->GetBrowserWindowInterface()->GetBrowserForMigrationOnly();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return std::nullopt;
  }

  ToolbarButtonProvider* toolbar_button_provider =
      browser_view->toolbar_button_provider();
  if (!toolbar_button_provider) {
    return std::nullopt;
  }

  views::Button* install_icon =
      toolbar_button_provider->GetPageActionView(kActionInstallPwa);

  if (install_icon) {
    // TODO(crbug.com/40841129): move this to dialog->SetHighlightedElement.
    return install_icon->AddAnchorHighlight();
  }

  return std::nullopt;
}

constexpr int kMinBoundsForInstallDialog = 50;

}  // namespace

WebAppInstallFlowDialogDelegate::WebAppInstallFlowDialogDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    WebAppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    InstallDialogType dialog_type,
    InstallOsType os_type,
    std::unique_ptr<ProgressDelay> progress_delay)
    : WebAppModalDialogDelegate(web_contents),
      os_type_(os_type),
      install_info_(std::move(install_info)),
      install_tracker_(std::move(install_tracker)),
      callback_(std::move(callback)),
      iph_state_(std::move(iph_state)),
      prefs_(prefs),
      tracker_(tracker),
      dialog_type_(dialog_type),
      page_action_highlight_(NewPageActionHighlight(CHECK_DEREF(web_contents))),
      progress_delay_(std::move(progress_delay)) {
  CHECK(install_info_);
  CHECK(install_tracker_);
  CHECK(prefs_);
  CHECK(progress_delay_);
}

WebAppInstallFlowDialogDelegate::~WebAppInstallFlowDialogDelegate() = default;

bool WebAppInstallFlowDialogDelegate::AdvanceToNextStepOrClose() {
  CHECK(dialog_model());

  // Update install dialog step.
  switch (current_step_) {
    case InstallDialogStep::kInstallDialog:
      if (os_type_ == InstallOsType::kOther) {
        current_step_ = InstallDialogStep::kProgress;
      } else {
        current_step_ = InstallDialogStep::kInstallerOptions;
      }
      break;

    case InstallDialogStep::kInstallerOptions:
      current_step_ = InstallDialogStep::kProgress;
      break;

    case InstallDialogStep::kProgress:
      current_step_ = InstallDialogStep::kSuccessful;
      break;

    case InstallDialogStep::kSuccessful:
      base::UmaHistogramEnumeration(
          "WebApp.InstallConfirmation.CloseReason",
          views::Widget::ClosedReason::kAcceptButtonClicked);
      if (reparent_closure_) {
        std::move(reparent_closure_).Run();
      }
      return true;
  }

  // Actions based on the new current_step_
  switch (current_step_) {
    case InstallDialogStep::kInstallDialog:
      break;

    case InstallDialogStep::kInstallerOptions: {
      ui::DialogModel::Button* ok_button =
          dialog_model()->GetButtonByUniqueId(kInstallButton);
      if (ok_button) {
        dialog_model()->SetButtonLabel(ok_button,
                                       l10n_util::GetStringUTF16(IDS_INSTALL));
      }
      break;
    }

    case InstallDialogStep::kProgress:
      // Trigger the installation.
      // TODO(crbug.com/508383640): Clean up metrics usage for new install flow.
      OnAccept();

      progress_delay_->Start(
          base::BindRepeating(&WebAppInstallFlowDialogDelegate::OnProgress,
                              weak_ptr_factory_.GetWeakPtr()));

      // Hide buttons on progress step.
      dialog_model()->SetVisible(kLearnMoreButtonId, false);
      dialog_model()->SetVisible(kInstallButton, false);
      dialog_model()->SetVisible(kCancelButtonId, false);
      break;

    case InstallDialogStep::kSuccessful: {
      ui::DialogModel::Button* ok_button =
          dialog_model()->GetButtonByUniqueId(kInstallButton);
      if (ok_button) {
        dialog_model()->SetButtonLabel(
            ok_button,
            l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_SUCCESS_OPEN_APP));
      }
      ui::DialogModel::Button* cancel_button =
          dialog_model()->GetButtonByUniqueId(kCancelButtonId);
      if (cancel_button) {
        dialog_model()->SetButtonLabel(cancel_button,
                                       l10n_util::GetStringUTF16(IDS_CLOSE));
      }
      dialog_model()->SetVisible(kInstallButton, true);
      dialog_model()->SetVisible(kCancelButtonId, true);
      break;
    }
  }

  if (flow_view_) {
    flow_view_->UpdateStepVisibility(current_step_);
  }

  UpdateDialogTitleAndHeader(current_step_);

  return false;
}

void WebAppInstallFlowDialogDelegate::OnLearnMoreButtonClicked() {
  web_contents()->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kInstallDialogFlowLearnMoreURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false),
      base::DoNothing());
}

void WebAppInstallFlowDialogDelegate::OnAccept() {
  if (options_view_) {
    // Pin to Shelf and Pin to Taskbar cannot both be true at the same time
    // because they're only set in different OS configurations, so this should
    // be fine.
    install_info_->add_to_quick_launch_bar =
        options_view_->IsPinToShelfChecked() ||
        options_view_->IsPinToTaskBarChecked();
    install_info_->add_to_desktop =
        options_view_->IsAddDesktopShortcutChecked();
  }

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

  // The password manager PWA installation tutorial requires the
  // `kInstalledPWAEventId` event to be fired from the detailed install dialog.
  // See `kPasswordManagerTutorialMetricPrefix` in
  // `MaybeRegisterChromeTutorials()` for more information.
  if (dialog_type_ == InstallDialogType::kDetailed) {
    auto* element_tracker = ui::ElementTracker::GetElementTracker();
    auto* element_framework = ui::ElementTracker::GetFrameworkDelegate();
    CHECK(element_tracker);
    auto* ok_button = element_tracker->GetElementInAnyContext(kInstallButton);
    if (ok_button && element_framework) {
      element_framework->NotifyCustomEvent(
          ok_button, WebAppInstallDialogDelegate::kInstalledPWAEventId);
    }
  }

  CHECK(callback_);
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kAccepted);
  received_user_response_ = true;

  std::move(callback_).Run(
      true, std::move(install_info_),
      base::BindOnce(&WebAppInstallFlowDialogDelegate::OnInstallResult,
                     AsWeakPtr()));
}

void WebAppInstallFlowDialogDelegate::OnCancel() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kCancelled);
  received_user_response_ = true;
  base::UmaHistogramEnumeration(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked);
  MeasureIphOnDialogClose();
}

void WebAppInstallFlowDialogDelegate::OnClose() {
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  received_user_response_ = true;
  base::UmaHistogramEnumeration(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked);
  MeasureIphOnDialogClose();
}

void WebAppInstallFlowDialogDelegate::OnDestroyed() {
  if (received_user_response_) {
    return;
  }
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  base::UmaHistogramEnumeration("WebApp.InstallConfirmation.CloseReason",
                                views::Widget::ClosedReason::kUnspecified);
  MeasureIphOnDialogClose();
}

void WebAppInstallFlowDialogDelegate::OnTextFieldChangedMaybeUpdateButton(
    const std::u16string& text_field_contents) {
  text_field_contents_ = text_field_contents;
  if (!dialog_model() || !dialog_model()->host()) {
    return;
  }

  ui::DialogModel::Button* ok_button = nullptr;
  if (dialog_model()->HasField(kInstallButton)) {
    ok_button = dialog_model()->GetButtonByUniqueId(kInstallButton);
  }

  if (ok_button) {
    dialog_model()->SetButtonEnabled(ok_button,
                                     /*enabled=*/!text_field_contents.empty());
  }
}

bool WebAppInstallFlowDialogDelegate::
    IsWidgetCurrentSizeSmallerThanPreferredSize(views::Widget* widget) {
  const gfx::Size& current_size = widget->GetSize();
  const gfx::Size& preferred_size =
      widget->GetContentsView()->GetPreferredSize();
  int min_width = preferred_size.width() - kMinBoundsForInstallDialog;
  int min_height = preferred_size.height() - kMinBoundsForInstallDialog;
  return current_size.width() < min_width || current_size.height() < min_height;
}

void WebAppInstallFlowDialogDelegate::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (IsWidgetCurrentSizeSmallerThanPreferredSize(widget)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppInstallFlowDialogDelegate::CloseDialogAsIgnored,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void WebAppInstallFlowDialogDelegate::CloseDialogAsIgnored() {
  if (!dialog_model() || !dialog_model()->host()) {
    return;
  }
  CHECK(install_tracker_);
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kIgnored);
  dialog_model()->host()->Close();
}

void WebAppInstallFlowDialogDelegate::MeasureIphOnDialogClose() {
  MeasureCancelUserActionsForInstallDialog();
  if (iph_state_ == PwaInProductHelpState::kShown && install_info_) {
    webapps::AppId app_id =
        GenerateAppIdFromManifestId(install_info_->manifest_id());
    WebAppPrefGuardrails::GetForDesktopInstallIph(prefs_).RecordIgnore(
        app_id, base::Time::Now());
  }

  if (install_info_ && callback_) {
    // If |install_info_| is populated, then the dialog was not accepted.
#if BUILDFLAG(IS_CHROMEOS)
    const webapps::AppId app_id =
        web_app::GenerateAppIdFromManifestId(install_info_->manifest_id());
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::AppDiscovery_Browser_AppInstallDialogResult()
            .SetWebAppInstallStatus(
                ToLong(web_app::WebAppInstallStatus::kCancelled))
            .SetAppId(app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)
    std::move(callback_).Run(false, std::move(install_info_),
                             base::DoNothing());
  }
}

void WebAppInstallFlowDialogDelegate::
    MeasureAcceptUserActionsForInstallDialog() {
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

void WebAppInstallFlowDialogDelegate::
    MeasureCancelUserActionsForInstallDialog() {
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

// Updates dialog title based on current step.
void WebAppInstallFlowDialogDelegate::UpdateDialogTitleAndHeader(
    InstallDialogStep step) {
  std::u16string title;
  switch (step) {
    case InstallDialogStep::kInstallDialog:
      NOTREACHED();
    case InstallDialogStep::kInstallerOptions:
      title = l10n_util::GetStringUTF16(dialog_type() == InstallDialogType::kDiy
                                            ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                            : IDS_INSTALL_PWA_DIALOG_TITLE);
      break;
    case InstallDialogStep::kProgress:
      title = l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_INSTALLING);
      break;
    case InstallDialogStep::kSuccessful:
      title = l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_SUCCESS_TITLE);
      break;
  }

  CHECK(dialog_model() && dialog_model()->host());
  auto* host =
      static_cast<views::BubbleDialogModelHost*>(dialog_model()->host());
  host->SetTitle(title);
  host->SetAccessibleTitle(title);
  // Clear the subtitle for all subsequent steps.
  host->SetSubtitle(std::u16string());

  // Clear the header for all subsequent steps.
  if (host->GetBubbleFrameView()) {
    host->GetBubbleFrameView()->SetHeaderView(nullptr);
  }
}

void WebAppInstallFlowDialogDelegate::UpdateProgressAndMaybeAdvance() {
  if (current_step_ != InstallDialogStep::kProgress) {
    return;
  }
  double percent =
      timer_percentage_.value_or(1.0) * 0.9 + (install_success_ ? 0.1 : 0);
  if (progress_view_) {
    progress_view_->SetProgressValue(percent);
  }

  if (!timer_percentage_.has_value() && install_success_) {
    AdvanceToNextStepOrClose();
  }
}

void WebAppInstallFlowDialogDelegate::OnInstallResult(
    bool success,
    base::OnceClosure reparent_closure) {
  if (!success) {
    CloseDialogAsIgnored();
    return;
  }
  install_success_ = true;
  reparent_closure_ = std::move(reparent_closure);
  IntentPickerTabHelper* helper =
      IntentPickerTabHelper::FromWebContents(web_contents());
  if (helper) {
    helper->MaybeShowIntentPickerIcon();
  }
  UpdateProgressAndMaybeAdvance();
}

void WebAppInstallFlowDialogDelegate::OnProgress(
    std::optional<double> percent) {
  timer_percentage_ = percent;
  UpdateProgressAndMaybeAdvance();
}

// Builds and shows an install dialog flow according to the install_type.
void WebAppInstallFlowDialogDelegate::Show(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    WebAppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state,
    base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
    bool show_initiating_origin,
    InstallDialogType install_type,
    InstallOsType os_type,
    std::unique_ptr<ProgressDelay> progress_delay) {
  auto* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* prefs = profile->GetPrefs();
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  DialogImageInfo dialog_image_info =
      install_info->GetIconBitmapsForSecureSurfaces();

  // For Install Options view, we need a larger icon image.
  gfx::ImageSkia icon_image_80 =
      CreateResizedIconImage(dialog_image_info.bitmaps, kLargeImageSize);

  gfx::ImageSkia icon_image_32(
      std::make_unique<WebAppInfoImageSource>(
          kIconSize, std::move(dialog_image_info.bitmaps)),
      gfx::Size(kIconSize, kIconSize));

  std::u16string title = install_info->title.value();
  GURL start_url = install_info->start_url();
  std::u16string description = install_info->description.value();
  auto delegate = std::make_unique<WebAppInstallFlowDialogDelegate>(
      web_contents, std::move(install_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker, install_type,
      os_type, std::move(progress_delay));
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  absl::flat_hash_map<InstallDialogStep, std::unique_ptr<views::View>>
      install_step_to_view;

  // kInstallDialog
  install_step_to_view[InstallDialogStep::kInstallDialog] =
      WebAppInstallIntroView::Create(
          install_type, icon_image_32, title, start_url,
          dialog_image_info.is_maskable, description, screenshot_fetcher,
          base::BindRepeating(&WebAppInstallFlowDialogDelegate::
                                  OnTextFieldChangedMaybeUpdateButton,
                              delegate_weak_ptr));

  // kInstallerOptions
  auto options_view = WebAppInstallOptionsView::Create(
      os_type, title, icon_image_32, icon_image_80,
      dialog_image_info.is_maskable, start_url);
  delegate->options_view_ = options_view->GetWeakPtr();
  install_step_to_view[InstallDialogStep::kInstallerOptions] =
      std::move(options_view);

  // kProgress
  auto progress_view = std::make_unique<WebAppInstallProgressView>();
  auto progress_view_weak_ptr = progress_view->GetWeakPtr();
  install_step_to_view[InstallDialogStep::kProgress] = std::move(progress_view);

  // kSuccessful
  auto successful_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();
  successful_view->AddChildView(WebAppIconNameAndOriginView::Create(
      icon_image_32, title, start_url, dialog_image_info.is_maskable));

  install_step_to_view[InstallDialogStep::kSuccessful] =
      std::move(successful_view);

  auto flow_view =
      std::make_unique<WebAppInstallFlowView>(std::move(install_step_to_view));
  flow_view->SetProperty(views::kElementIdentifierKey,
                         kInstallDialogFlowViewId);
  auto flow_view_weak_ptr = flow_view->GetWeakPtr();
  delegate->SetFlowView(flow_view_weak_ptr);
  delegate->SetProgressView(progress_view_weak_ptr);

  views::View* focusable_view =
      flow_view->GetViewForStep(InstallDialogStep::kInstallDialog);

  auto dialog_model_builder = ui::DialogModel::Builder(std::move(delegate));
  dialog_model_builder.SetInternalName("WebAppInstallFlowDialog")
      .SetAccessibleTitle(
          l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                        ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                        : IDS_INSTALL_PWA_DIALOG_TITLE))
      .SetTitle(
          l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                        ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                        : IDS_INSTALL_PWA_DIALOG_TITLE))
      // TODO(b/473080055): Use a translated string. Should use the correct
      // subtitle if DIY vs simple and detailed like the title above.
      .SetSubtitle(l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_FLOW_SUBTITLE))
      .AddExtraButton(
          base::BindRepeating(
              [](base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate,
                 const ui::Event&) {
                if (delegate) {
                  delegate->OnLearnMoreButtonClicked();
                }
              },
              delegate_weak_ptr),
          ui::DialogModel::Button::Params()
              .SetLabel(
                  l10n_util::GetStringUTF16(IDS_LEARN_MORE_MAYBE_TITLE_CASE))
              .SetId(kLearnMoreButtonId))
      .AddOkButton(
          base::BindRepeating(
              [](base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate) {
                return delegate ? delegate->AdvanceToNextStepOrClose() : true;
              },
              delegate_weak_ptr),
          ui::DialogModel::Button::Params()
              .SetLabel(os_type == InstallOsType::kOther
                            ? l10n_util::GetStringUTF16(IDS_INSTALL)
                            : l10n_util::GetStringUTF16(
                                  IDS_WEB_APP_INSTALL_FLOW_NEXT))
              .SetId(WebAppInstallFlowDialogDelegate::kInstallButton))
      .AddCancelButton(
          base::BindOnce(&WebAppInstallFlowDialogDelegate::OnCancel,
                         delegate_weak_ptr),
          ui::DialogModel::Button::Params().SetId(kCancelButtonId))
      .SetCloseActionCallback(base::BindOnce(
          &WebAppInstallFlowDialogDelegate::OnClose, delegate_weak_ptr))
      .SetDialogDestroyingCallback(base::BindOnce(
          &WebAppInstallFlowDialogDelegate::OnDestroyed, delegate_weak_ptr))
      .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
      .AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::move(flow_view),
              views::BubbleDialogModelHost::FieldType::kControl,
              focusable_view),
          WebAppInstallFlowDialogDelegate::kInstallDialogFlowViewId);

  if (install_type == InstallDialogType::kDiy) {
    dialog_model_builder.SetInitiallyFocusedField(kInstallDialogFlowViewId);
  }
  if (install_type != InstallDialogType::kDetailed) {
    dialog_model_builder.SetBannerImage(ui::ImageModel::FromImageSkia(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_WEB_APP_INSTALL_ILLUSTRATION)));
  }

  if (show_initiating_origin) {
    url::Origin initiating_origin =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    std::u16string origin_url = url_formatter::FormatOriginForSecurityDisplay(
        initiating_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    dialog_model_builder.SetSubtitle(l10n_util::GetStringFUTF16(
        IDS_INSTALL_PWA_DIALOG_ORIGIN_LABEL, origin_url));
  }

  auto dialog = views::BubbleDialogModelHost::CreateModal(
      dialog_model_builder.Build(), ui::mojom::ModalType::kChild);

  views::Widget* widget = constrained_window::ShowWebModalDialogViews(
      dialog.release(), web_contents);

  if (IsWidgetCurrentSizeSmallerThanPreferredSize(widget)) {
    delegate_weak_ptr->CloseDialogAsIgnored();
    return;
  }
  delegate_weak_ptr->OnWidgetShownStartTracking(widget);
}

}  // namespace web_app
