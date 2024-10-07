// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"

#include <string>

#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_prompt_chip_model.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/permissions/permission_indicators_tab_data.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace {

// A duration of the expand animation. In other words, how long does it take to
// expand the chip.
constexpr auto kExpandAnimationDuration = base::Milliseconds(350);
// A duration of the collapse animation. In other words, how long does it take
// to collapse/shrink the chip.
constexpr auto kCollapseAnimationDuration = base::Milliseconds(250);
// A delay for the verbose state. In other words the delay that is used between
// expand and collapse animations.
constexpr auto kCollapseDelay = base::Seconds(4);

base::TimeDelta GetAnimationDuration(base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

// This method updates indicators' visibility set in
// `PageSpecificContentSettings`.
void UpdateIndicatorsVisibilityFlags(LocationBarView* location_bar) {
  if (!location_bar->GetWebContents()) {
    return;
  }

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          location_bar->GetWebContents()->GetPrimaryMainFrame());

  if (!pscs) {
    return;
  }

  if (pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kCameraAccessed)) {
    pscs->OnPermissionIndicatorShown(ContentSettingsType::MEDIASTREAM_CAMERA);
  } else {
    pscs->OnPermissionIndicatorHidden(ContentSettingsType::MEDIASTREAM_CAMERA);
  }

  if (pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kMicrophoneAccessed)) {
    pscs->OnPermissionIndicatorShown(ContentSettingsType::MEDIASTREAM_MIC);
  } else {
    pscs->OnPermissionIndicatorHidden(ContentSettingsType::MEDIASTREAM_MIC);
  }
}

// Returns `true` if there is misalignment in Camera & Mic usage and displayed
// indicators.
bool ShouldExpandChipIndicator(
    content_settings::PageSpecificContentSettings* pscs) {
  if (pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kCameraAccessed) &&
      pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kMicrophoneAccessed) &&
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA) &&
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC)) {
    return false;
  }

  if (pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kCameraAccessed) &&
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA)) {
    return false;
  }

  if (pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kMicrophoneAccessed) &&
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC)) {
    return false;
  }

  return true;
}

void RecordIndicators(ContentSettingImageModel* indicator_model,
                      content_settings::PageSpecificContentSettings* pscs,
                      bool clicked) {
  std::set<ContentSettingsType> permissions;
  if (pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kCameraAccessed)) {
    permissions.insert(ContentSettingsType::MEDIASTREAM_CAMERA);
  }
  if (pscs->GetMicrophoneCameraState().Has(
          content_settings::PageSpecificContentSettings::kMicrophoneAccessed)) {
    permissions.insert(ContentSettingsType::MEDIASTREAM_MIC);
  }

  permissions::PermissionUmaUtil::RecordActivityIndicator(
      permissions, indicator_model->is_blocked(),
      indicator_model->blocked_on_system_level(), clicked);
}

// If permission request chip is visible, the indicator's verbose state (the
// expand animation) should be suppressed.
bool SuppressVerboseState(ChipController* request_chip_controller) {
  if (!request_chip_controller->IsPermissionPromptChipVisible()) {
    return false;
  }

  PermissionPromptChipModel* prompt_model =
      request_chip_controller->permission_prompt_model();

  if (!prompt_model) {
    return false;
  }

  ContentSettingsType prompt_type = prompt_model->content_settings_type();

  // If currently displayed permission request chip is not for media
  // permissions, the expand animation should be suppressed.
  if (prompt_type != ContentSettingsType::MEDIASTREAM_CAMERA &&
      prompt_type != ContentSettingsType::MEDIASTREAM_MIC) {
    return true;
  }

  std::optional<permissions::PermissionRequestManager*> prm =
      request_chip_controller->active_permission_request_manager();

  // If there are pending permission requests in `PermissionRequestManager`,
  // then the expand animation should be suppressed.
  return prm.has_value() && prm.value()->has_pending_requests();
}
}  // namespace

PermissionDashboardController::PermissionDashboardController(
    LocationBarView* location_bar_view,
    PermissionDashboardView* permission_dashboard_view)
    : location_bar_view_(location_bar_view),
      permission_dashboard_view_(permission_dashboard_view) {
  request_chip_controller_ = std::make_unique<ChipController>(
      location_bar_view, permission_dashboard_view_->GetRequestChip(),
      permission_dashboard_view_, this);
  observation_.Observe(permission_dashboard_view_->GetIndicatorChip());

  permission_dashboard_view->GetIndicatorChip()->SetCallback(
      base::BindRepeating(
          &PermissionDashboardController::OnIndicatorsChipButtonPressed,
          weak_factory_.GetWeakPtr()));
  permission_dashboard_view->SetVisible(false);
}

PermissionDashboardController::~PermissionDashboardController() = default;

bool PermissionDashboardController::Update(
    ContentSettingImageModel* indicator_model,
    ContentSettingImageView::Delegate* delegate) {
  indicator_model->Update(delegate->ShouldHideContentSettingImage()
                              ? nullptr
                              : location_bar_view_->GetWebContents());

  PermissionChipView* indicator_chip =
      permission_dashboard_view_->GetIndicatorChip();

  if (!indicator_model->is_visible()) {
    if (!indicator_chip->GetVisible()) {
      return false;
    }

    // When `WebContents` is nullptr, `indicator_model->is_visible()` is always
    // false.
    if (!location_bar_view_->GetWebContents()) {
      HideIndicators();
      return true;
    }

    // In case `GetPrimaryMainFrame()` changed, we should immediately hide
    // indicators without the collapse animation.
    bool same_frame = main_frame_id_ == location_bar_view_->GetWebContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetGlobalId();

    if (is_verbose_ && same_frame) {
      // At first show the collapse animation and then hide indicators.
      Collapse(/*hide=*/true);
    } else {
      HideIndicators();
    }

    return true;
  }

  content_setting_image_model_ = indicator_model;
  delegate_ = delegate;
  // Save the currently displayed frame id to avoid unnecessary animation if the
  // main frame gets changed.
  main_frame_id_ = location_bar_view_->GetWebContents()
                       ->GetPrimaryMainFrame()
                       ->GetGlobalId();
  permission_dashboard_view_->SetVisible(true);

  // Always update the icon and the message as they may change based on used
  // permissions.
  indicator_chip->SetChipIcon(indicator_model->icon());
  indicator_chip->SetMessage(GetIndicatorTitle(indicator_model));

  blocked_on_system_level_ = indicator_model->blocked_on_system_level();
  if (indicator_model->is_blocked()) {
    indicator_chip->SetTheme(
        indicator_model->blocked_on_system_level()
            ? PermissionChipTheme::kOnSystemBlockedActivityIndicator
            : PermissionChipTheme::kBlockedActivityIndicator);
  } else {
    indicator_chip->SetTheme(PermissionChipTheme::kInUseActivityIndicator);
  }


  if (request_chip_controller_->is_confirmation_showing()) {
    request_chip_controller_->ResetPermissionPromptChip();
  }

  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          location_bar_view_->GetWebContents()->GetPrimaryMainFrame());

  indicator_chip->SetVisible(true);

  if (ShouldExpandChipIndicator(content_settings)) {
    is_verbose_ = false;
    if (SuppressVerboseState(request_chip_controller())) {
      // Permission request chip is visible it was drawn without a divider.
      // Add the divider between an indicator and the request chip.
      permission_dashboard_view_->UpdateDividerViewVisibility();
    } else {
      // Suppress LHS indicator's verbose animation if it was already displayed.
      // Blocked on the system level is an error case and should always be
      // animated.
      permissions::PermissionIndicatorsTabData* permission_indicators_tab_data =
          location_bar_view_->browser()
              ->tab_strip_model()
              ->GetActiveTab()
              ->tab_features()
              ->permission_indicators_tab_data();
      if (permission_indicators_tab_data &&
          permission_indicators_tab_data->IsVerboseIndicatorAllowed(
              permissions::PermissionIndicatorsTabData::IndicatorsType::
                  kMediaStream)) {
        indicator_chip->ResetAnimation();
        indicator_chip->AnimateExpand(
            GetAnimationDuration(kExpandAnimationDuration));
      }
    }
  }

  UpdateIndicatorsVisibilityFlags(location_bar_view_);

  if (indicator_model->ShouldNotifyAccessibility(
          location_bar_view_->GetWebContents())) {
    indicator_chip->SetTooltipText(indicator_model->get_tooltip());

    std::u16string name = l10n_util::GetStringUTF16(
        indicator_model->AccessibilityAnnouncementStringId());
    permission_dashboard_view_->GetViewAccessibility().SetName(name);

    permission_dashboard_view_->GetViewAccessibility().AnnounceAlert(
        l10n_util::GetStringFUTF16(
            IDS_A11Y_INDICATORS_ANNOUNCEMENT, name,
            l10n_util::GetStringUTF16(IDS_A11Y_OMNIBOX_CHIP_HINT)));

    RecordIndicators(indicator_model, content_settings, /*clicked=*/false);

    indicator_model->AccessibilityWasNotified(
        location_bar_view_->GetWebContents());
  }

  return true;
}

void PermissionDashboardController::OnChipVisibilityChanged(bool is_visible) {}

void PermissionDashboardController::OnExpandAnimationEnded() {
  if (!location_bar_view_->GetWebContents()) {
    HideIndicators();
    return;
  }

  is_verbose_ = true;

  UpdateIndicatorsVisibilityFlags(location_bar_view_);

  StartCollapseTimer();
}

void PermissionDashboardController::OnCollapseAnimationEnded() {
  if (!location_bar_view_->GetWebContents()) {
    HideIndicators();
    return;
  }

  permissions::PermissionIndicatorsTabData* permission_indicators_tab_data =
      location_bar_view_->browser()
          ->tab_strip_model()
          ->GetActiveTab()
          ->tab_features()
          ->permission_indicators_tab_data();

  if (permission_indicators_tab_data) {
    permission_indicators_tab_data->SetVerboseIndicatorDisplayed(
        permissions::PermissionIndicatorsTabData::IndicatorsType::kMediaStream);
  }

  is_verbose_ = false;
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          location_bar_view_->GetWebContents()->GetPrimaryMainFrame());
  if (!content_settings || (!content_settings->IsIndicatorVisible(
                                ContentSettingsType::MEDIASTREAM_CAMERA) &&
                            !content_settings->IsIndicatorVisible(
                                ContentSettingsType::MEDIASTREAM_MIC))) {
    HideIndicators();
  }
}

void PermissionDashboardController::OnMousePressed() {
  should_suppress_reopening_page_info_ = !!page_info_bubble_tracker_.view();
}

bool PermissionDashboardController::SuppressVerboseIndicator() {
  if (collapse_timer_.IsRunning()) {
    collapse_timer_.FireNow();
    return true;
  }

  return false;
}

void PermissionDashboardController::StartCollapseTimer() {
  if (do_no_collapse_for_testing_) {
    return;
  }

  collapse_timer_.Start(FROM_HERE, kCollapseDelay,
                        base::BindOnce(&PermissionDashboardController::Collapse,
                                       weak_factory_.GetWeakPtr(),
                                       /*hide=*/false));
}

void PermissionDashboardController::Collapse(bool hide) {
  if (hide) {
    UpdateIndicatorsVisibilityFlags(location_bar_view_);
  }
  if (!permission_dashboard_view_->GetIndicatorChip()->is_animating()) {
    permission_dashboard_view_->GetIndicatorChip()->AnimateCollapse(
        GetAnimationDuration(kCollapseAnimationDuration));
  }
}

void PermissionDashboardController::HideIndicators() {
  collapse_timer_.AbandonAndStop();
  permission_dashboard_view_->GetIndicatorChip()->ResetAnimation();
  is_verbose_ = false;
  permission_dashboard_view_->GetIndicatorChip()
      ->GetViewAccessibility()
      .SetIsIgnored(true);
  permission_dashboard_view_->GetIndicatorChip()->SetVisible(false);
  content_setting_image_model_ = nullptr;
  delegate_ = nullptr;
  permission_dashboard_view_->GetDividerView()->SetVisible(false);
  if (permission_dashboard_view_->GetRequestChip()->GetVisible()) {
    // After the indicator view is gone, remove the divider padding if the
    // request chip is visible.
    permission_dashboard_view_->GetRequestChip()->UpdateForDividerVisibility(
        false);
  } else {
    permission_dashboard_view_->SetVisible(false);
  }

  // If blocked on the system level, then the indicators will not be shown as
  // blocked in PSCS. Reset them manually.
  if (blocked_on_system_level_ && location_bar_view_->GetWebContents()) {
    content_settings::PageSpecificContentSettings* pscs =
        content_settings::PageSpecificContentSettings::GetForFrame(
            location_bar_view_->GetWebContents()->GetPrimaryMainFrame());
    if (!pscs) {
      return;
    }

    if (pscs->GetMicrophoneCameraState().Has(
            content_settings::PageSpecificContentSettings::kCameraAccessed)) {
      pscs->OnPermissionIndicatorHidden(
          ContentSettingsType::MEDIASTREAM_CAMERA);
      pscs->ResetMediaBlockedState(ContentSettingsType::MEDIASTREAM_CAMERA,
                                   /*update_indicators=*/false);
    }

    if (pscs->GetMicrophoneCameraState().Has(
            content_settings::PageSpecificContentSettings::
                kMicrophoneAccessed)) {
      pscs->OnPermissionIndicatorHidden(ContentSettingsType::MEDIASTREAM_MIC);
      pscs->ResetMediaBlockedState(ContentSettingsType::MEDIASTREAM_MIC,
                                   /*update_indicators=*/false);
    }
  }

  UpdateIndicatorsVisibilityFlags(location_bar_view_);
}

void PermissionDashboardController::ShowBubble() {
  content::WebContents* web_contents = location_bar_view_->GetWebContents();
  if (web_contents && !page_info_bubble_tracker_) {
    views::View* const anchor = permission_dashboard_view_->GetIndicatorChip();
    ContentSettingBubbleContents* bubble_view_ =
        new ContentSettingBubbleContents(
            content_setting_image_model_->CreateBubbleModel(
                delegate_->GetContentSettingBubbleModelDelegate(),
                web_contents),
            web_contents, anchor, views::BubbleBorder::TOP_LEFT);
    bubble_view_->SetHighlightedButton(
        permission_dashboard_view_->GetIndicatorChip());
    views::Widget* bubble_widget =
        views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    bubble_widget->Show();
    delegate_->OnContentSettingImageBubbleShown(
        content_setting_image_model_->image_type());
  }
}

void PermissionDashboardController::ShowPageInfoDialog() {
  content::WebContents* contents = location_bar_view_->GetWebContents();
  if (!contents) {
    return;
  }

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (entry->IsInitialEntry()) {
    return;
  }

  // If PageInfo already opened, close it and return.
  // Under a normal mouse click flow the PageInfo dialog will be closed on a
  // focus lost event. But tests and maybe some UI automation tools have
  // different mouse click event propagation flow. In other words the mouse
  // click listener will be called before the PageInfo dialog receives a focus
  // change event. Hence the dialog will not be closed on time.
  if (page_info_bubble_tracker_) {
    page_info_bubble_tracker_.view()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    return;
  }

  if (should_suppress_reopening_page_info_) {
    // Reset the flag because `OnMousePressed()` is not called if the LHS
    // indicator gets keyboard interaction.
    should_suppress_reopening_page_info_ = false;
    return;
  }

  auto initialized_callback = base::DoNothing();

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          permission_dashboard_view_, gfx::Rect(),
          permission_dashboard_view_->GetWidget()->GetNativeWindow(), contents,
          entry->GetVirtualURL(), std::move(initialized_callback),
          base::BindOnce(&PermissionDashboardController::OnPageInfoBubbleClosed,
                         weak_factory_.GetWeakPtr()),
          /*allow_about_this_site=*/true);
  bubble->GetWidget()->Show();
  page_info_bubble_tracker_.SetView(bubble);
}

void PermissionDashboardController::OnPageInfoBubbleClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {}

void PermissionDashboardController::OnIndicatorsChipButtonPressed() {
  content::WebContents* contents = location_bar_view_->GetWebContents();
  if (!contents) {
    return;
  }

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (entry->IsInitialEntry()) {
    return;
  }

  GURL url = entry->GetVirtualURL();

  if (PageInfo::IsFileOrInternalPage(url) ||
      url.SchemeIs(content_settings::kExtensionScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme)) {
    ShowBubble();
  } else {
    ShowPageInfoDialog();
  }

  if (content_setting_image_model_) {
    content_settings::PageSpecificContentSettings* pscs =
        content_settings::PageSpecificContentSettings::GetForFrame(
            location_bar_view_->GetWebContents()->GetPrimaryMainFrame());
    if (!pscs) {
      return;
    }

    RecordIndicators(content_setting_image_model_, pscs, /*clicked=*/true);
  }
}

std::u16string PermissionDashboardController::GetIndicatorTitle(
    ContentSettingImageModel* model) {
  // Currently PermissionDashboardController supports only Camera and
  // Microphone.
  DCHECK(model->image_type() ==
         ContentSettingImageModel::ImageType::MEDIASTREAM);

  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          location_bar_view_->GetWebContents()->GetPrimaryMainFrame());
  if (!content_settings) {
    return std::u16string();
  }

  content_settings::PageSpecificContentSettings::MicrophoneCameraState state =
      content_settings->GetMicrophoneCameraState();

  if (model->blocked_on_system_level()) {
    if (state.Has(content_settings::PageSpecificContentSettings::
                      kMicrophoneAccessed) &&
        state.Has(
            content_settings::PageSpecificContentSettings::kCameraAccessed)) {
      return l10n_util::GetStringUTF16(IDS_CAMERA_MICROPHONE_CANNOT_ACCESS);
    }

    if (state.Has(
            content_settings::PageSpecificContentSettings::kCameraAccessed)) {
      return l10n_util::GetStringUTF16(IDS_CAMERA_CANNOT_ACCESS);
    }

    if (state.Has(content_settings::PageSpecificContentSettings::
                      kMicrophoneAccessed)) {
      return l10n_util::GetStringUTF16(IDS_MICROPHONE_CANNOT_ACCESS);
    }
  }

  if (model->is_blocked()) {
    if (state.Has(content_settings::PageSpecificContentSettings::
                      kMicrophoneAccessed) &&
        state.Has(
            content_settings::PageSpecificContentSettings::kCameraAccessed)) {
      return l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_NOT_ALLOWED);
    }

    if (state.Has(
            content_settings::PageSpecificContentSettings::kCameraAccessed)) {
      return l10n_util::GetStringUTF16(IDS_CAMERA_NOT_ALLOWED);
    }

    if (state.Has(content_settings::PageSpecificContentSettings::
                      kMicrophoneAccessed)) {
      return l10n_util::GetStringUTF16(IDS_MICROPHONE_NOT_ALLOWED);
    }

    DUMP_WILL_BE_NOTREACHED();
    return std::u16string();
  }

  if (state.Has(
          content_settings::PageSpecificContentSettings::kMicrophoneAccessed) &&
      state.Has(
          content_settings::PageSpecificContentSettings::kCameraAccessed)) {
    return l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_IN_USE);
  }
  if (state.Has(
          content_settings::PageSpecificContentSettings::kCameraAccessed)) {
    return l10n_util::GetStringUTF16(IDS_CAMERA_IN_USE);
  }
  if (state.Has(
          content_settings::PageSpecificContentSettings::kMicrophoneAccessed)) {
    return l10n_util::GetStringUTF16(IDS_MICROPHONE_IN_USE);
  }

  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}
