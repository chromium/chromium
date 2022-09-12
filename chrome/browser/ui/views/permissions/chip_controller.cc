// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip_controller.h"

#include <memory>
#include <string>
#include <utility>
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"

#include "components/permissions/permission_prompt.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"

class BubbleButtonController : public views::ButtonController {
 public:
  BubbleButtonController(
      views::Button* button,
      BubbleOwnerDelegate* bubble_owner,
      std::unique_ptr<views::ButtonControllerDelegate> delegate)
      : views::ButtonController(button, std::move(delegate)),
        bubble_owner_(bubble_owner) {}

  // TODO(crbug.com/1270699): Add keyboard support.
  void OnMouseEntered(const ui::MouseEvent& event) override {
    if (bubble_owner_->IsBubbleShowing() || bubble_owner_->IsAnimating()) {
      return;
    }
    bubble_owner_->RestartTimersOnMouseHover();
  }

 private:
  raw_ptr<BubbleOwnerDelegate> bubble_owner_ = nullptr;
};

ChipController::ChipController(Browser* browser, OmniboxChipButton* chip_view)
    : chip_(chip_view), browser_(browser) {
  chip_->SetVisible(false);
}

ChipController::~ChipController() = default;

void ChipController::OnPermissionRequestManagerDestructed() {
  if (permission_prompt_model_) {
    permission_prompt_model_->ResetDelegate();
  }
}

bool ChipController::IsBubbleShowing() {
  return chip_ != nullptr && GetPromptBubbleWidget() != nullptr;
}

bool ChipController::IsAnimating() const {
  return chip_->is_animating();
}

void ChipController::RestartTimersOnMouseHover() {
  if (!permission_prompt_model_ || IsBubbleShowing() || IsAnimating()) {
    return;
  }
  if (chip_->is_fully_collapsed()) {
    StartDismissTimer();
  } else {
    StartCollapseTimer();
  }
}

void ChipController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(GetPromptBubbleWidget(), widget);
  ResetTimers();
  if (widget->closed_reason() == views::Widget::ClosedReason::kEscKeyPressed ||
      widget->closed_reason() ==
          views::Widget::ClosedReason::kCloseButtonClicked) {
    OnPromptBubbleDismissed();
  }

  widget->RemoveObserver(this);

  // If permission request is still active after the prompt was closed,
  // collapse the chip.
  CollapseChip(/*allow_restart=*/false);
}

void ChipController::ShowPermissionPrompt(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  ResetTimers();
  permission_prompt_model_ =
      std::make_unique<PermissionPromptChipModel>(delegate);

  chip_->SetText(permission_prompt_model_->GetPermissionMessage());
  chip_->SetTheme(permission_prompt_model_->GetChipTheme());
  chip_->SetChipIcon(permission_prompt_model_->GetAllowedIcon());
  chip_->SetButtonController(std::make_unique<BubbleButtonController>(
      chip_.get(), this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          chip_.get())));
  chip_->SetCallback(base::BindRepeating(&ChipController::OnChipButtonPressed,
                                         base::Unretained(this)));
  chip_shown_time_ = base::TimeTicks::Now();
  chip_->SetVisible(true);

  ObservePromptBubble();

  AnnouncePermissionRequestForAccessibility(l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_REQUESTED_SCREENREADER_ANNOUNCEMENT));

  if (permission_prompt_model_ && permission_prompt_model_->ShouldExpand() &&
      (permission_prompt_model_->ShouldBubbleStartOpen() ||
       (!permission_prompt_model_->WasRequestAlreadyDisplayed()))) {
    AnimateExpand(base::BindRepeating(&ChipController::OnExpandAnimationEnded,
                                      base::Unretained(this)));
  } else {
    StartDismissTimer();
  }
}

void ChipController::FinalizeChip() {
  FinalizePermissionPromptChip();
}

void ChipController::FinalizePermissionPromptChip() {
  chip_->ResetAnimation();
  chip_->SetChipIcon(gfx::kNoneIcon);
  chip_->SetVisible(false);

  views::Widget* const bubble_widget = GetPromptBubbleWidget();
  if (bubble_widget) {
    bubble_widget->RemoveObserver(this);
    bubble_widget->Close();
  }

  ResetTimers();
  permission_prompt_model_.reset();

  LocationBarView* lbv = GetLocationBarView();
  if (lbv) {
    lbv->InvalidateLayout();
  }
}

bool ChipController::should_start_open_for_testing() {
  CHECK_IS_TEST();
  return permission_prompt_model_->ShouldBubbleStartOpen();
}

bool ChipController::should_expand_for_testing() {
  CHECK_IS_TEST();
  return permission_prompt_model_->ShouldExpand();
}

void ChipController::AnimateExpand(
    base::RepeatingCallback<void()> expand_anmiation_ended_callback) {
  chip_->SetExpandAnimationEndedCallback(
      std::move(expand_anmiation_ended_callback));
  chip_->ResetAnimation();
  chip_->AnimateExpand();
  chip_->SetVisible(true);
}

void ChipController::AnnouncePermissionRequestForAccessibility(
    const std::u16string& text) {
#if BUILDFLAG(IS_MAC)
  chip_->GetViewAccessibility().OverrideName(text);
  chip_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
#else

  chip_->GetViewAccessibility().AnnounceText(text);
#endif
}

void ChipController::CollapseChip(bool allow_restart) {
  if (allow_restart && chip_->IsMouseHovered()) {
    StartCollapseTimer();
  } else {
    AnimateCollapse();
    chip_->SetChipIcon(permission_prompt_model_
                           ? permission_prompt_model_->GetBlockedIcon()
                           : gfx::kNoneIcon);
    chip_->SetTheme(OmniboxChipTheme::kLowVisibility);
    StartDismissTimer();
  }
}

void ChipController::OpenPermissionPromptBubble() {
  DCHECK(!IsBubbleShowing());
  if (!permission_prompt_model_ ||
      !permission_prompt_model_->GetDelegate().has_value()) {
    return;
  }

  // prevent chip from collapsing while prompt bubble is open
  ResetTimers();

  if (permission_prompt_model_->GetPromptStyle() ==
      PermissionPromptStyle::kChip) {
    // Loud prompt bubble
    raw_ptr<PermissionPromptBubbleView> prompt_bubble =
        new PermissionPromptBubbleView(
            browser_,
            permission_prompt_model_->GetDelegate().value()->GetWeakPtr(),
            chip_shown_time_, PermissionPromptStyle::kChip);
    prompt_bubble_tracker_.SetView(prompt_bubble);
    prompt_bubble->Show();
  } else if (permission_prompt_model_->GetPromptStyle() ==
             PermissionPromptStyle::kQuietChip) {
    // Quiet prompt bubble
    LocationBarView* lbv = GetLocationBarView();
    content::WebContents* web_contents = lbv->GetContentSettingWebContents();

    if (web_contents) {
      std::unique_ptr<ContentSettingQuietRequestBubbleModel>
          content_setting_bubble_model =
              std::make_unique<ContentSettingQuietRequestBubbleModel>(
                  lbv->GetContentSettingBubbleModelDelegate(), web_contents);
      ContentSettingBubbleContents* quiet_request_bubble =
          new ContentSettingBubbleContents(
              std::move(content_setting_bubble_model), web_contents, lbv,
              views::BubbleBorder::TOP_LEFT);
      quiet_request_bubble->set_close_on_deactivate(false);
      views::Widget* bubble_widget =
          views::BubbleDialogDelegateView::CreateBubble(quiet_request_bubble);
      quiet_request_bubble->set_close_on_deactivate(false);
      prompt_bubble_tracker_.SetView(quiet_request_bubble);
      bubble_widget->Show();
    }
  }
  chip_->SetVisibilityChangedCallback(base::BindRepeating(
      &ChipController::OnChipVisibilityChanged, base::Unretained(this)));

  // It is possible that a Chip get finalized while the permission prompt
  // bubble was displayed.
  if (permission_prompt_model_ && IsBubbleShowing()) {
    GetPromptBubbleWidget()->AddObserver(this);
    permission_prompt_model_->GetDelegate().value()->SetBubbleShown();
  }
}

void ChipController::ClosePermissionPromptBubbleWithReason(
    views::Widget::ClosedReason reason) {
  DCHECK(IsBubbleShowing());
  GetPromptBubbleWidget()->CloseWithReason(reason);
}

void ChipController::RecordChipButtonPressed(const char* recordKey) {
  base::UmaHistogramMediumTimes(recordKey,
                                base::TimeTicks::Now() - chip_shown_time_);
}

void ChipController::ObservePromptBubble() {
  views::Widget* promptBubbleWidget = GetPromptBubbleWidget();
  if (promptBubbleWidget) {
    promptBubbleWidget->AddObserver(this);
  }
}

void ChipController::OnPromptBubbleDismissed() {
  DCHECK(permission_prompt_model_);
  if (!permission_prompt_model_)
    return;

  permission_prompt_model_->SetShouldDismiss(true);
  if (permission_prompt_model_->GetDelegate().has_value()) {
    permission_prompt_model_->GetDelegate().value()->SetDismissOnTabClose();
    // If the permission prompt bubble is closed, we count it as "Dismissed",
    // hence it should record the time when the bubble is closed and not when
    // the permission request is finalized.
    permission_prompt_model_->GetDelegate().value()->SetDecisionTime();
  }
}

void ChipController::OnPromptExpired() {
  AnnouncePermissionRequestForAccessibility(l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_EXPIRED_SCREENREADER_ANNOUNCEMENT));
  if (permission_prompt_model_ &&
      permission_prompt_model_->GetDelegate().has_value()) {
    if (permission_prompt_model_->ShouldDismiss()) {
      permission_prompt_model_->GetDelegate().value()->Dismiss();
    } else {
      permission_prompt_model_->GetDelegate().value()->Ignore();
    }
  }
}

void ChipController::OnChipButtonPressed() {
  if (permission_prompt_model_ &&
      (!IsBubbleShowing() ||
       permission_prompt_model_->ShouldBubbleStartOpen())) {
    // Only record if its the first interaction.
    if (permission_prompt_model_->GetPromptStyle() ==
        PermissionPromptStyle::kChip) {
      RecordChipButtonPressed("Permissions.Chip.TimeToInteraction");
    } else if (permission_prompt_model_->GetPromptStyle() ==
               PermissionPromptStyle::kQuietChip) {
      RecordChipButtonPressed("Permissions.QuietChip.TimeToInteraction");
    }
  }

  if (IsBubbleShowing()) {
    // A mouse click on chip while a permission prompt is open should dismiss
    // the prompt and collapse the chip
    ClosePermissionPromptBubbleWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
  } else {
    OpenPermissionPromptBubble();
  }
}

void ChipController::OnExpandAnimationEnded() {
  if (IsBubbleShowing() || !IsPermissionPromptChipVisible() ||
      !permission_prompt_model_)
    return;

  if (permission_prompt_model_->ShouldBubbleStartOpen()) {
    OpenPermissionPromptBubble();
  } else {
    StartCollapseTimer();
  }
}

void ChipController::OnChipVisibilityChanged() {
  auto* prompt_bubble = GetPromptBubbleWidget();
  if (!chip_->GetVisible() && prompt_bubble) {
    // In case if the prompt bubble isn't closed on focus loss, manually close
    // it when chip is hidden.
    prompt_bubble->Close();
  }
}

void ChipController::StartCollapseTimer() {
  constexpr auto kDelayBeforeCollapsingChip = base::Seconds(12);
  collapse_timer_.Start(
      FROM_HERE, kDelayBeforeCollapsingChip,
      base::BindOnce(&ChipController::CollapseChip, base::Unretained(this),
                     /*allow_restart=*/true));
}

void ChipController::StartDismissTimer() {
  if (!permission_prompt_model_)
    return;

  if (permission_prompt_model_->ShouldExpand()) {
    if (base::FeatureList::IsEnabled(
            permissions::features::kPermissionChipAutoDismiss)) {
      auto delay = base::Milliseconds(
          permissions::features::kPermissionChipAutoDismissDelay.Get());
      dismiss_timer_.Start(FROM_HERE, delay, this,
                           &ChipController::OnPromptExpired);
    }
  } else {
    // Abusive origins do not support expand animation, hence the dismiss timer
    // should be longer.
    dismiss_timer_.Start(FROM_HERE, base::Seconds(18), this,
                         &ChipController::OnPromptExpired);
  }
}

void ChipController::ResetTimers() {
  collapse_timer_.AbandonAndStop();
  dismiss_timer_.AbandonAndStop();
}

LocationBarView* ChipController::GetLocationBarView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  return browser_view ? browser_view->GetLocationBarView() : nullptr;
}

views::Widget* ChipController::GetPromptBubbleWidget() {
  return prompt_bubble_tracker_.view()
             ? prompt_bubble_tracker_.view()->GetWidget()
             : nullptr;
}
