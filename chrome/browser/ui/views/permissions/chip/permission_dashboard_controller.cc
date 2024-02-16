// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"

#include <string>

#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
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
}  // namespace

PermissionDashboardController::PermissionDashboardController(
    Browser* browser,
    LocationBarView* location_bar_view,
    PermissionDashboardView* permission_dashboard_view)
    : browser_(browser),
      location_bar_view_(location_bar_view),
      permission_dashboard_view_(permission_dashboard_view) {
  request_chip_controller_ = std::make_unique<ChipController>(
      browser, permission_dashboard_view_->GetRequestChip(),
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
    bool force_hide) {
  indicator_model->Update(force_hide ? nullptr
                                     : location_bar_view_->GetWebContents());

  PermissionChipView* indicator_chip =
      permission_dashboard_view_->GetIndicatorChip();

  if (!indicator_model->is_visible()) {
    if (!indicator_chip->GetVisible()) {
      return false;
    }

    if (is_verbose_) {
      Collapse(/*hide=*/true);
    } else {
      HideIndicators();
    }

    return true;
  }

  permission_dashboard_view_->SetVisible(true);

  indicator_chip->SetChipIcon(indicator_model->icon());
  indicator_chip->SetTheme(indicator_model->is_blocked()
                               ? PermissionChipTheme::kBlockedActivityIndicator
                               : PermissionChipTheme::kInUseActivityIndicator);
  indicator_chip->GetViewAccessibility().OverrideIsIgnored(false);
  indicator_chip->SetTooltipText(indicator_model->get_tooltip());

  if (request_chip_controller_->is_confirmation_showing()) {
    request_chip_controller_->ResetPermissionPromptChip();
  }

  indicator_chip->ResetAnimation();

  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          location_bar_view_->GetWebContents()->GetPrimaryMainFrame());

  if (ShouldExpandChipIndicator(content_settings)) {
    indicator_chip->SetMessage(GetIndicatorTitle(indicator_model));
    indicator_chip->AnimateExpand(
        GetAnimationDuration(kExpandAnimationDuration));
    // An alert role is required in order to fire the alert event.
    indicator_chip->SetAccessibleRole(ax::mojom::Role::kAlert);
  } else {
    UpdateIndicatorsVisibilityFlags(location_bar_view_);
  }
  indicator_chip->SetVisible(true);

  return true;
}

void PermissionDashboardController::OnChipVisibilityChanged(bool is_visible) {}

void PermissionDashboardController::OnExpandAnimationEnded() {
  is_verbose_ = true;

  UpdateIndicatorsVisibilityFlags(location_bar_view_);

  StartCollapseTimer();
}

void PermissionDashboardController::OnCollapseAnimationEnded() {
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

bool PermissionDashboardController::SuppressVerboseIndicator() {
  if (collapse_timer_.IsRunning()) {
    collapse_timer_.FireNow();
    return true;
  }

  return false;
}

void PermissionDashboardController::StartCollapseTimer() {
  collapse_timer_.Start(FROM_HERE, kCollapseDelay,
                        base::BindOnce(&PermissionDashboardController::Collapse,
                                       weak_factory_.GetWeakPtr(),
                                       /*hide=*/false));
}

void PermissionDashboardController::Collapse(bool hide) {
  if (hide) {
    UpdateIndicatorsVisibilityFlags(location_bar_view_);
  }
  permission_dashboard_view_->GetIndicatorChip()->AnimateCollapse(
      GetAnimationDuration(kCollapseAnimationDuration));
}

void PermissionDashboardController::HideIndicators() {
  collapse_timer_.AbandonAndStop();

  permission_dashboard_view_->GetIndicatorChip()
      ->GetViewAccessibility()
      .OverrideIsIgnored(true);
  permission_dashboard_view_->GetIndicatorChip()->SetVisible(false);
  permission_dashboard_view_->GetDividerView()->SetVisible(false);
  if (permission_dashboard_view_->GetRequestChip()->GetVisible()) {
    // After the indicator view is gone, remove the divider padding if the
    // request chip is visible.
    permission_dashboard_view_->GetRequestChip()->UpdateForDividerVisibility(
        false);
  } else {
    permission_dashboard_view_->SetVisible(false);
  }

  UpdateIndicatorsVisibilityFlags(location_bar_view_);
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

  auto initialized_callback = base::DoNothing();

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          permission_dashboard_view_, gfx::Rect(),
          permission_dashboard_view_->GetWidget()->GetNativeWindow(), contents,
          entry->GetVirtualURL(), std::move(initialized_callback),
          base::BindOnce(&PermissionDashboardController::OnPageInfoBubbleClosed,
                         weak_factory_.GetWeakPtr()));
  bubble->GetWidget()->Show();
  page_info_bubble_tracker_.SetView(bubble);
}

void PermissionDashboardController::OnPageInfoBubbleClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {}

void PermissionDashboardController::OnIndicatorsChipButtonPressed() {
  ShowPageInfoDialog();
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

    NOTREACHED();
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

  NOTREACHED();
  return std::u16string();
}
