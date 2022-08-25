// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_chip.h"
#include <cstddef>
#include <memory>

#include "base/check.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_util.h"
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

namespace {
const gfx::VectorIcon& GetBlockedPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(delegate->Requests().size() > 0);

  auto requests = delegate->Requests();
  if (requests.size() == 1)
    return requests[0]->GetBlockedIconForChip();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return permissions::RequestType::kCameraStream == requests[0]->request_type()
             ? requests[0]->GetBlockedIconForChip()
             : requests[1]->GetBlockedIconForChip();
}

const gfx::VectorIcon& GetPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(delegate->Requests().size() > 0);
  auto requests = delegate->Requests();
  if (requests.size() == 1)
    return requests[0]->GetIconForChip();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return permissions::RequestType::kCameraStream == requests[0]->request_type()
             ? requests[0]->GetIconForChip()
             : requests[1]->GetIconForChip();
}

std::u16string GetQuietPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(delegate->Requests()[0]->GetQuietChipText().has_value());

  return delegate->Requests()[0]->GetQuietChipText().value();
}

std::u16string GetPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  auto requests = delegate->Requests();

  return requests.size() == 1
             ? requests[0]->GetRequestChipText().value()
             : l10n_util::GetStringUTF16(
                   IDS_MEDIA_CAPTURE_VIDEO_AND_AUDIO_PERMISSION_CHIP);
}

bool ShouldPermissionBubbleExpand(
    permissions::PermissionPrompt::Delegate* delegate,
    PermissionPromptStyle prompt_style) {
  DCHECK(delegate);
  if (PermissionPromptStyle::kQuietChip == prompt_style) {
    return !permissions::PermissionUiSelector::ShouldSuppressAnimation(
        delegate->ReasonForUsingQuietUi());
  }

  return true;
}
}  // namespace

void PermissionChip::OpenBubble() {
  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!IsBubbleShowing());

  if (!permission_prompt_delegate_.value()) {
    return;
  }

  // Prevent the chip from being collapsed if the permission prompt bubble is
  // opened.
  ResetTimers();

  if (prompt_style_ == PermissionPromptStyle::kChip) {
    raw_ptr<PermissionPromptBubbleView> prompt_bubble =
        new PermissionPromptBubbleView(
            browser_, permission_prompt_delegate_.value()->GetWeakPtr(),
            chip_shown_time_, PermissionPromptStyle::kChip);
    prompt_bubble_tracker_.SetView(prompt_bubble);
    prompt_bubble->Show();
  } else if (prompt_style_ == PermissionPromptStyle::kQuietChip) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

    LocationBarView* lbv =
        browser_view ? browser_view->GetLocationBarView() : nullptr;
    BrowserView::GetBrowserViewForBrowser(browser_)->GetLocationBarView();
    content::WebContents* web_contents = lbv->GetContentSettingWebContents();

    if (web_contents) {
      std::unique_ptr<ContentSettingQuietRequestBubbleModel>
          content_setting_bubble_model =
              std::make_unique<ContentSettingQuietRequestBubbleModel>(
                  lbv->GetContentSettingBubbleModelDelegate(), web_contents);
      raw_ptr<ContentSettingBubbleContents> quiet_request_bubble =
          new ContentSettingBubbleContents(
              std::move(content_setting_bubble_model), web_contents, lbv,
              views::BubbleBorder::TOP_LEFT);
      views::Widget* bubble_widget =
          views::BubbleDialogDelegateView::CreateBubble(quiet_request_bubble);

      quiet_request_bubble->set_close_on_deactivate(false);
      prompt_bubble_tracker_.SetView(quiet_request_bubble);
      bubble_widget->Show();
    }
  }

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
  if (GetVisible() && !permission_prompt_delegate_.has_value())
    return;
  SetVisible(true);
  Show();
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
  if (!permission_prompt_delegate_.value()) {
    return;
  }
  if (!should_bubble_start_open_) {
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
  chip_button_->SetChipIcon(*blocked_icon_);
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

void PermissionChip::SetupChip(const std::u16string& text,
                               OmniboxChipTheme visibility,
                               const gfx::VectorIcon& icon) {
  chip_shown_time_ = base::TimeTicks::Now();
  chip_button_->SetText(text);
  chip_button_->SetTheme(visibility);
  chip_button_->SetChipIcon(icon);
  chip_button_->SetButtonController(std::make_unique<BubbleButtonController>(
      chip_button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          chip_button_)));
  chip_button_->SetExpandAnimationEndedCallback(base::BindRepeating(
      &PermissionChip::ExpandAnimationEnded, weak_factory_.GetWeakPtr()));
}

void PermissionChip::ShowQuietChip(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* permission_prompt_delegate) {
  DCHECK(permission_prompt_delegate);

  permission_prompt_delegate_ = permission_prompt_delegate;
  browser_ = browser;
  prompt_style_ = PermissionPromptStyle::kQuietChip;
  blocked_icon_ = &GetBlockedPermissionIconId(*permission_prompt_delegate_);
  should_bubble_start_open_ = false;
  should_expand_ = ShouldPermissionBubbleExpand(
      permission_prompt_delegate_.value(), prompt_style_);

  SetupChip(GetQuietPermissionMessage(*permission_prompt_delegate_),
            OmniboxChipTheme::kLowVisibility, *blocked_icon_);
  SetVisible(true);
  Show();
  AnnounceChip();
}

void PermissionChip::ShowLoudChip(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* permission_prompt_delegate) {
  DCHECK(permission_prompt_delegate);

  permission_prompt_delegate_ = permission_prompt_delegate;
  browser_ = browser;
  prompt_style_ = PermissionPromptStyle::kChip;
  should_bubble_start_open_ =
      permissions::PermissionUtil::ShouldPermissionBubbleStartOpen(
          permission_prompt_delegate_.value());
  blocked_icon_ = &GetBlockedPermissionIconId(*permission_prompt_delegate_);
  should_expand_ = true;
  SetupChip(GetPermissionMessage(*permission_prompt_delegate_),
            OmniboxChipTheme::kNormalVisibility,
            GetPermissionIconId(*permission_prompt_delegate_));
  SetVisible(true);
  Show();
  AnnounceChip();
}

void PermissionChip::Finalize() {
  Hide();

  permissions::PermissionPrompt::Delegate* delegate = nullptr;
  if (permission_prompt_delegate_.has_value()) {
    delegate = permission_prompt_delegate_.value();
    permission_prompt_delegate_.reset();
  }

  chip_button_->Finalize();

  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_EXPIRED_SCREENREADER_ANNOUNCEMENT));

  if (delegate) {
    if (should_dismiss_) {
      delegate->Dismiss();
    } else {
      delegate->Ignore();
    }
  }

  views::Widget* const bubble_widget = GetPromptBubbleWidget();
  if (bubble_widget) {
    bubble_widget->RemoveObserver(this);
    bubble_widget->Close();
  }
  prompt_bubble_tracker_.SetView(nullptr);

  CHECK(!IsInObserverList());
  ResetTimers();
}

bool PermissionChip::IsActive() {
  return GetVisible() && permission_prompt_delegate_.value();
}

void PermissionChip::Show() {
  // TODO(olesiamarukhno): Add tests for animation logic.
  chip_button_->ResetAnimation();
  if (should_expand_ && (should_bubble_start_open_ ||
                         (permission_prompt_delegate_.value() &&
                          !permission_prompt_delegate_.value()
                               ->WasCurrentRequestAlreadyDisplayed()))) {
    chip_button_->AnimateExpand();
  } else {
    StartDismissTimer();
  }
  PreferredSizeChanged();
}

void PermissionChip::ExpandAnimationEnded() {
  if (IsBubbleShowing() || !GetVisible() ||
      !permission_prompt_delegate_.value())
    return;

  if (should_bubble_start_open_) {
    OpenBubble();
  } else {
    StartCollapseTimer();
  }
}

void PermissionChip::ChipButtonPressed() {
  if (!IsBubbleShowing() || should_bubble_start_open_) {
    // Only record if its the first interaction.
    if (prompt_style_ == PermissionPromptStyle::kChip) {
      RecordChipButtonPressed("Permissions.Chip.TimeToInteraction");
    } else if (prompt_style_ == PermissionPromptStyle::kQuietChip) {
      RecordChipButtonPressed("Permissions.QuietChip.TimeToInteraction");
    }
  }

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

void PermissionChip::RecordChipButtonPressed(const char* recordKey) {
  base::UmaHistogramMediumTimes(recordKey,
                                base::TimeTicks::Now() - chip_shown_time_);
}

BEGIN_METADATA(PermissionChip, views::View)
END_METADATA
