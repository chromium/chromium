// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_chip.h"

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/widget/widget.h"

// ButtonController that NotifyClick from being called when the
// BubbleOwnerDelegate's bubble is showing. Otherwise the bubble will show again
// immediately after being closed via losing focus.
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

PermissionChip::PermissionChip() {
  SetVisible(false);
  SetUseDefaultFillLayout(true);

  chip_button_ =
      AddChildView(std::make_unique<OmniboxChipButton>(base::BindRepeating(
          &PermissionChip::ChipButtonPressed, base::Unretained(this))));
}

PermissionChip::~PermissionChip() {
  Finalize();
}

void PermissionChip::OpenBubble() {
  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!IsBubbleShowing());

  // Prevent the chip from being collapsed if the permission prompt bubble is
  // opened.
  ResetTimers();

  prompt_bubble_tracker_.SetView(permission_chip_delegate_->CreateBubble());
  permission_chip_delegate_->ShowBubble();

  // It is possible that a Chip get finalized while the permission prompt bubble
  // was displayed.
  if (permission_prompt_delegate_.value() && prompt_bubble_tracker_.view()) {
    prompt_bubble_tracker_.view()->GetWidget()->AddObserver(this);
    permission_prompt_delegate_.value()->SetBubbleShown();
  }
}

void PermissionChip::Hide() {
  SetVisible(false);
}

void PermissionChip::Reshow() {
  if (GetVisible() || !IsInitialized())
    return;
  SetVisible(true);
  Show(/*always_open_bubble=*/false);
}

void PermissionChip::Collapse(bool allow_restart) {
  if (allow_restart && IsMouseHovered()) {
    StartCollapseTimer();
  } else {
    chip_button_->AnimateCollapse();
    StartDismissTimer();
    ShowBlockedIcon();
  }
}

void PermissionChip::AnnounceChip() {
  if (!should_start_open_) {
#if BUILDFLAG(IS_MAC)
    GetViewAccessibility().OverrideName(l10n_util::GetStringUTF16(
        IDS_PERMISSIONS_REQUESTED_SCREENREADER_ANNOUNCEMENT));
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
#else
    GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
        IDS_PERMISSIONS_REQUESTED_SCREENREADER_ANNOUNCEMENT));
#endif
  }
}

void PermissionChip::OnPromptBubbleDismissed() {
  should_dismiss_ = true;
  if (permission_prompt_delegate_.value()) {
    permission_prompt_delegate_.value()->SetDismissOnTabClose();
    // If the permission prompt bubble is closed, we count it as "Dismissed",
    // hence it should record the time when the bubble is closed and not when
    // the permission request is finalized.
    permission_prompt_delegate_.value()->SetDecisionTime();
  }
}

void PermissionChip::ShowBlockedIcon() {
  chip_button_->SetShowBlockedIcon(true);
}

void PermissionChip::VisibilityChanged(views::View* /*starting_from*/,
                                       bool is_visible) {
  auto* prompt_bubble = GetPromptBubbleWidget();
  if (!is_visible && prompt_bubble) {
    // In case if the prompt bubble isn't closed on focus loss, manually close
    // it when chip is hidden.
    prompt_bubble->Close();
  }
}

void PermissionChip::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(GetPromptBubbleWidget(), widget);
  if (widget->closed_reason() == views::Widget::ClosedReason::kEscKeyPressed ||
      widget->closed_reason() ==
          views::Widget::ClosedReason::kCloseButtonClicked) {
    OnPromptBubbleDismissed();
  }

  widget->RemoveObserver(this);
  // If permission request is still active after the prompt was closed,
  // collapse the chip.
  Collapse(/*allow_restart=*/false);
  ShowBlockedIcon();
}

bool PermissionChip::IsBubbleShowing() const {
  return prompt_bubble_tracker_.view() != nullptr;
}

bool PermissionChip::IsAnimating() const {
  return chip_button_->is_animating();
}

void PermissionChip::RestartTimersOnMouseHover() {
  if (is_fully_collapsed()) {
    StartDismissTimer();
  } else {
    StartCollapseTimer();
  }
}

views::Widget* PermissionChip::GetPromptBubbleWidgetForTesting() {
  return GetPromptBubbleWidget();
}

views::Widget* PermissionChip::GetPromptBubbleWidget() {
  return prompt_bubble_tracker_.view()
             ? prompt_bubble_tracker_.view()->GetWidget()
             : nullptr;
}

void PermissionChip::SetupChip(
    std::unique_ptr<PermissionChipDelegate> permission_chip_delegate) {
  DCHECK(permission_chip_delegate.get());
  DCHECK(!permission_chip_delegate_.get());

  permission_chip_delegate_ = std::move(permission_chip_delegate);

  permission_prompt_delegate_ =
      permission_chip_delegate_->GetPermissionPromptDelegate();

  should_start_open_ = permission_chip_delegate_->ShouldStartOpen();
  should_expand_ = permission_chip_delegate_->ShouldExpand();

  chip_button_->SetTheme(permission_chip_delegate_->GetTheme());
  chip_button_->SetText(permission_chip_delegate_->GetMessage());
  chip_button_->SetPermissionChipDelegate(permission_chip_delegate_.get());
  chip_button_->SetButtonController(std::make_unique<BubbleButtonController>(
      chip_button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          chip_button_)));
  chip_button_->SetExpandAnimationEndedCallback(base::BindRepeating(
      &PermissionChip::ExpandAnimationEnded, weak_factory_.GetWeakPtr()));

  SetVisible(true);
  Show(should_start_open_);

  AnnounceChip();
}

void PermissionChip::Finalize() {
  Hide();

  if (!IsInitialized()) {
    return;
  }

  permissions::PermissionPrompt::Delegate* delegate =
      permission_prompt_delegate_.value();
  permission_prompt_delegate_.reset();

  chip_button_->Finalize();

  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_EXPIRED_SCREENREADER_ANNOUNCEMENT));

  if (should_dismiss_) {
    delegate->Dismiss();
  } else {
    delegate->Ignore();
  }

  permission_chip_delegate_.reset();

  views::Widget* const bubble_widget = GetPromptBubbleWidget();
  if (bubble_widget) {
    bubble_widget->RemoveObserver(this);
    bubble_widget->Close();
  }
  prompt_bubble_tracker_.SetView(nullptr);

  CHECK(!IsInObserverList());
  ResetTimers();
}

bool PermissionChip::IsInitialized() {
  return permission_prompt_delegate_.has_value() &&
         permission_chip_delegate_.get();
}

bool PermissionChip::IsActive() {
  return GetVisible() && IsInitialized();
}

void PermissionChip::Show(bool always_open_bubble) {
  // TODO(olesiamarukhno): Add tests for animation logic.
  chip_button_->ResetAnimation();
  if (should_expand_ && (!permission_prompt_delegate_.value()
                              ->WasCurrentRequestAlreadyDisplayed() ||
                         always_open_bubble)) {
    chip_button_->AnimateExpand();
  } else {
    StartDismissTimer();
  }
  PreferredSizeChanged();
}

void PermissionChip::ExpandAnimationEnded() {
  if (IsBubbleShowing() || !GetVisible() || !IsInitialized())
    return;

  if (should_start_open_) {
    OpenBubble();
  } else {
    StartCollapseTimer();
  }
}

void PermissionChip::ChipButtonPressed() {
  if (IsBubbleShowing()) {
    // A mouse click on chip while a permission prompt is open should dismiss
    // the prompt and collapse the chip
    prompt_bubble_tracker_.view()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
  } else {
    OpenBubble();
  }
}

void PermissionChip::StartCollapseTimer() {
  constexpr auto kDelayBeforeCollapsingChip = base::Seconds(12);
  collapse_timer_.Start(
      FROM_HERE, kDelayBeforeCollapsingChip,
      base::BindOnce(&PermissionChip::Collapse, base::Unretained(this),
                     /*allow_restart=*/true));
}

void PermissionChip::StartDismissTimer() {
  if (should_expand_) {
    if (base::FeatureList::IsEnabled(
            permissions::features::kPermissionChipAutoDismiss)) {
      auto delay = base::Milliseconds(
          permissions::features::kPermissionChipAutoDismissDelay.Get());
      dismiss_timer_.Start(FROM_HERE, delay, this, &PermissionChip::Finalize);
    }
  } else {
    // Abusive origins do not support expand animation, hence the dismiss timer
    // should be longer.
    dismiss_timer_.Start(FROM_HERE, base::Seconds(18), this,
                         &PermissionChip::Finalize);
  }
}

BEGIN_METADATA(PermissionChip, views::View)
END_METADATA
