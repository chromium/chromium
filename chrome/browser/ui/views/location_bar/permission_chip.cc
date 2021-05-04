// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_chip.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace {
bool IsCameraPermission(permissions::RequestType type) {
  return type == permissions::RequestType::kCameraStream;
}

bool IsCameraOrMicPermission(permissions::RequestType type) {
  return IsCameraPermission(type) ||
         type == permissions::RequestType::kMicStream;
}
}  // namespace

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

  bool OnMousePressed(const ui::MouseEvent& event) override {
    suppress_button_release_ = bubble_owner_->IsBubbleShowing();
    return views::ButtonController::OnMousePressed(event);
  }

  bool IsTriggerableEvent(const ui::Event& event) override {
    // TODO(olesiamarukhno): There is the same logic in IconLabelBubbleView,
    // this class should be reused in the future to avoid duplication.
    if (event.IsMouseEvent())
      return !bubble_owner_->IsBubbleShowing() && !suppress_button_release_;

    return views::ButtonController::IsTriggerableEvent(event);
  }

 private:
  bool suppress_button_release_ = false;
  BubbleOwnerDelegate* bubble_owner_ = nullptr;
};

PermissionChip::PermissionChip(Browser* browser) : browser_(browser) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetVisible(false);

  chip_button_ =
      AddChildView(std::make_unique<OmniboxChipButton>(base::BindRepeating(
          &PermissionChip::ChipButtonPressed, base::Unretained(this))));

  chip_button_->SetButtonController(std::make_unique<BubbleButtonController>(
      chip_button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          chip_button_)));

  chip_button_->SetExpandAnimationEndedCallback(base::BindRepeating(
      &PermissionChip::ExpandAnimationEnded, base::Unretained(this)));

  chip_button_->SetTheme(OmniboxChipButton::Theme::kBlue);
  chip_button_->SetProminent(true);
}

PermissionChip::~PermissionChip() {
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
  CHECK(!IsInObserverList());
}

void PermissionChip::DisplayRequest(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
  requested_time_ = base::TimeTicks::Now();

  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();

  // TODO(olesiamarukhno): Add combined camera & microphone permission and
  // update delegate to contain only one request at a time.
  DCHECK(requests.size() == 1u || requests.size() == 2u);
  if (requests.size() == 2) {
    DCHECK(IsCameraOrMicPermission(requests[0]->GetRequestType()));
    DCHECK(IsCameraOrMicPermission(requests[1]->GetRequestType()));
    DCHECK_NE(requests[0]->GetRequestType(), requests[1]->GetRequestType());
  }

  chip_button_->SetText(GetPermissionMessage());
  chip_button_->SetIcon(&GetPermissionIconId());

  Show(ShouldBubbleStartOpen());

  if (!ShouldBubbleStartOpen()) {
    GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
        IDS_PERMISSIONS_REQUESTED_SCREENREADER_ANNOUNCEMENT));
  }
}

void PermissionChip::FinalizeRequest() {
  SetVisible(false);
  chip_button_->ResetAnimation();
  collapse_timer_.AbandonAndStop();
  dismiss_timer_.AbandonAndStop();
  delegate_ = nullptr;
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
  already_recorded_interaction_ = false;
  PreferredSizeChanged();
}

void PermissionChip::Reshow() {
  if (GetVisible())
    return;
  Show(/*always_open_bubble=*/false);
}

void PermissionChip::Hide() {
  SetVisible(false);
}

bool PermissionChip::GetActiveRequest() const {
  return !!delegate_;
}

void PermissionChip::OnMouseEntered(const ui::MouseEvent& event) {
  if (!chip_button_->is_animating())
    RestartTimersOnInteraction();
}

void PermissionChip::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, prompt_bubble_->GetWidget());
  widget->RemoveObserver(this);
  prompt_bubble_ = nullptr;
  // If permission request is still active after the prompt was closed,
  // collapse the chip.
  if (delegate_)
    Collapse(/*allow_restart=*/false);
}

void PermissionChip::OpenBubble() {
  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!prompt_bubble_);

  prompt_bubble_ = new PermissionPromptBubbleView(
      browser_, delegate_, requested_time_, PermissionPromptStyle::kChip);
  prompt_bubble_->Show();
  prompt_bubble_->GetWidget()->AddObserver(this);
}

bool PermissionChip::IsBubbleShowing() const {
  return prompt_bubble_ != nullptr;
}

bool PermissionChip::ShouldBubbleStartOpen() const {
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipGestureSensitive)) {
    auto requests = delegate_->Requests();
    const bool has_gesture =
        std::any_of(requests.begin(), requests.end(), [](auto* request) {
          return request->GetGestureType() ==
                 permissions::PermissionRequestGestureType::GESTURE;
        });
    if (has_gesture)
      return true;
  }
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipRequestTypeSensitive)) {
    // Notifications and geolocation are targeted here because they are usually
    // not necessary for the website to function correctly, so they can safely
    // be given less prominence.
    auto requests = delegate_->Requests();
    const bool is_geolocation_or_notifications =
        std::any_of(requests.begin(), requests.end(), [](auto* request) {
          auto request_type = request->GetRequestType();
          return request_type == permissions::RequestType::kNotifications ||
                 request_type == permissions::RequestType::kGeolocation;
        });
    if (!is_geolocation_or_notifications)
      return true;
  }
  return false;
}

void PermissionChip::Show(bool always_open_bubble) {
  SetVisible(true);
  // TODO(olesiamarukhno): Add tests for animation logic.
  chip_button_->ResetAnimation();
  if (!delegate_->WasCurrentRequestAlreadyDisplayed() || always_open_bubble)
    chip_button_->AnimateExpand();
  PreferredSizeChanged();
}

void PermissionChip::ExpandAnimationEnded() {
  StartCollapseTimer();
  if (ShouldBubbleStartOpen())
    OpenBubble();
}

void PermissionChip::ChipButtonPressed() {
  OpenBubble();
  RestartTimersOnInteraction();
  if (!already_recorded_interaction_) {
    base::UmaHistogramLongTimes("Permissions.Chip.TimeToInteraction",
                                base::TimeTicks::Now() - requested_time_);
    already_recorded_interaction_ = true;
  }
}

void PermissionChip::RestartTimersOnInteraction() {
  if (is_fully_collapsed()) {
    StartDismissTimer();
  } else {
    StartCollapseTimer();
  }
}

void PermissionChip::StartCollapseTimer() {
  constexpr auto kDelayBeforeCollapsingChip = base::TimeDelta::FromSeconds(12);
  collapse_timer_.Start(
      FROM_HERE, kDelayBeforeCollapsingChip,
      base::BindOnce(&PermissionChip::Collapse, base::Unretained(this),
                     /*allow_restart=*/true));
}

void PermissionChip::Collapse(bool allow_restart) {
  if (allow_restart && (IsMouseHovered() || prompt_bubble_)) {
    StartCollapseTimer();
  } else {
    chip_button_->AnimateCollapse();
    StartDismissTimer();
  }
}

void PermissionChip::StartDismissTimer() {
  constexpr auto kDelayBeforeDismissingRequest =
      base::TimeDelta::FromSeconds(6);
  dismiss_timer_.Start(FROM_HERE, kDelayBeforeDismissingRequest, this,
                       &PermissionChip::Dismiss);
}

void PermissionChip::Dismiss() {
  delegate_->Closing();
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_EXPIRED_SCREENREADER_ANNOUNCEMENT));
}

const gfx::VectorIcon& PermissionChip::GetPermissionIconId() const {
  auto requests = delegate_->Requests();
  if (requests.size() == 1)
    return permissions::GetIconId(requests[0]->GetRequestType());

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return IsCameraPermission(requests[0]->GetRequestType())
             ? permissions::GetIconId(requests[0]->GetRequestType())
             : permissions::GetIconId(requests[1]->GetRequestType());
}

std::u16string PermissionChip::GetPermissionMessage() const {
  if (!delegate_)
    return std::u16string();
  auto requests = delegate_->Requests();

  return requests.size() == 1
             ? requests[0]->GetChipText().value()
             : l10n_util::GetStringUTF16(
                   IDS_MEDIA_CAPTURE_VIDEO_AND_AUDIO_PERMISSION_CHIP);
}

BEGIN_METADATA(PermissionChip, views::View)
ADD_READONLY_PROPERTY_METADATA(bool, ActiveRequest)
ADD_READONLY_PROPERTY_METADATA(std::u16string, PermissionMessage)
END_METADATA
