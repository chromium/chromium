// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_chip.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "components/permissions/permission_request.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {
bool IsCameraPermission(permissions::PermissionRequestType type) {
  return type ==
         permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA;
}

bool IsCameraOrMicPermission(permissions::PermissionRequestType type) {
  return IsCameraPermission(type) ||
         type == permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_MIC;
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

PermissionChip::PermissionChip(Browser* browser)
    : views::AnimationDelegateViews(this), browser_(browser) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetVisible(false);

  chip_button_ =
      AddChildView(std::make_unique<views::MdTextButton>(base::BindRepeating(
          &PermissionChip::ChipButtonPressed, base::Unretained(this))));
  chip_button_->SetProminent(true);
  chip_button_->SetCornerRadius(GetIconSize());
  chip_button_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  chip_button_->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  chip_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Equalizing padding on the left, right and between icon and label.
  chip_button_->SetImageLabelSpacing(
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left());
  chip_button_->SetCustomPadding(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING),
                  GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left()));

  chip_button_->SetButtonController(std::make_unique<BubbleButtonController>(
      chip_button_, this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          chip_button_)));

  constexpr auto kAnimationDuration = base::TimeDelta::FromMilliseconds(350);
  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetSlideDuration(kAnimationDuration);
}

PermissionChip::~PermissionChip() {
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
  CHECK(!IsInObserverList());
}

void PermissionChip::Show(permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;

  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();

  // TODO(olesiamarukhno): Add combined camera & microphone permission and
  // update delegate to contain only one request at a time.
  DCHECK(requests.size() == 1u || requests.size() == 2u);
  if (requests.size() == 2) {
    DCHECK(IsCameraOrMicPermission(requests[0]->GetPermissionRequestType()));
    DCHECK(IsCameraOrMicPermission(requests[1]->GetPermissionRequestType()));
    DCHECK_NE(requests[0]->GetPermissionRequestType(),
              requests[1]->GetPermissionRequestType());
  }

  chip_button_->SetText(GetPermissionMessage());
  UpdatePermissionIconAndTextColor();

  SetVisible(true);
  // TODO(olesiamarukhno): Add tests for animation logic.
  animation_->Reset();
  if (!delegate_->WasCurrentRequestAlreadyDisplayed())
    animation_->Show();
  requested_time_ = base::TimeTicks::Now();
  PreferredSizeChanged();
}

void PermissionChip::Hide() {
  SetVisible(false);
  timer_.AbandonAndStop();
  delegate_ = nullptr;
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
  already_recorded_interaction_ = false;
  PreferredSizeChanged();
}

gfx::Size PermissionChip::CalculatePreferredSize() const {
  const int fixed_width = GetIconSize() + chip_button_->GetInsets().width();
  const int collapsable_width =
      chip_button_->GetPreferredSize().width() - fixed_width;
  const int width =
      std::round(collapsable_width * animation_->GetCurrentValue()) +
      fixed_width;
  return gfx::Size(width, GetHeightForWidth(width));
}

void PermissionChip::OnMouseEntered(const ui::MouseEvent& event) {
  // Restart the timer after user hovers the view.
  StartCollapseTimer();
}

void PermissionChip::OnThemeChanged() {
  View::OnThemeChanged();
  UpdatePermissionIconAndTextColor();
}

void PermissionChip::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, animation_.get());
  if (animation->GetCurrentValue() == 1.0)
    StartCollapseTimer();
}

void PermissionChip::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, animation_.get());
  PreferredSizeChanged();
}

void PermissionChip::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, prompt_bubble_->GetWidget());
  widget->RemoveObserver(this);
  prompt_bubble_ = nullptr;
  animation_->Hide();
}

bool PermissionChip::IsBubbleShowing() const {
  return prompt_bubble_ != nullptr;
}

void PermissionChip::ChipButtonPressed() {
  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!prompt_bubble_);

  prompt_bubble_ =
      new PermissionPromptBubbleView(browser_, delegate_, requested_time_);
  prompt_bubble_->Show();
  prompt_bubble_->GetWidget()->AddObserver(this);
  // Restart the timer after user clicks on the chip to open the bubble.
  StartCollapseTimer();
  if (!already_recorded_interaction_) {
    base::UmaHistogramLongTimes("Permissions.Chip.TimeToInteraction",
                                base::TimeTicks::Now() - requested_time_);
    already_recorded_interaction_ = true;
  }
}

void PermissionChip::Collapse() {
  if (IsMouseHovered() || prompt_bubble_) {
    StartCollapseTimer();
  } else {
    animation_->Hide();
  }
}

void PermissionChip::StartCollapseTimer() {
  constexpr auto kDelayBeforeCollapsingChip =
      base::TimeDelta::FromMilliseconds(8000);
  timer_.Start(FROM_HERE, kDelayBeforeCollapsingChip, this,
               &PermissionChip::Collapse);
}

int PermissionChip::GetIconSize() const {
  return GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
}

void PermissionChip::UpdatePermissionIconAndTextColor() {
  if (!delegate_)
    return;

  // Set label and icon color to be the same color.
  SkColor enabled_text_color =
      views::style::GetColor(*chip_button_, views::style::CONTEXT_BUTTON_MD,
                             views::style::STYLE_DIALOG_BUTTON_DEFAULT);

  chip_button_->SetEnabledTextColors(enabled_text_color);
  chip_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(GetPermissionIconId(), enabled_text_color,
                                     GetIconSize()));
}

const gfx::VectorIcon& PermissionChip::GetPermissionIconId() {
  auto requests = delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetIconId();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return IsCameraPermission(requests[0]->GetPermissionRequestType())
             ? requests[0]->GetIconId()
             : requests[1]->GetIconId();
}

base::string16 PermissionChip::GetPermissionMessage() {
  auto requests = delegate_->Requests();

  return requests.size() == 1
             ? requests[0]->GetChipText().value()
             : l10n_util::GetStringUTF16(
                   IDS_MEDIA_CAPTURE_VIDEO_AND_AUDIO_PERMISSION_CHIP);
}
