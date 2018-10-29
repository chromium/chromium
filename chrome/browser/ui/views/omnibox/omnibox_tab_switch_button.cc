// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_mask.h"

bool OmniboxTabSwitchButton::calculated_widths_ = false;
size_t OmniboxTabSwitchButton::icon_only_width_;
size_t OmniboxTabSwitchButton::short_text_width_;
size_t OmniboxTabSwitchButton::full_text_width_;

OmniboxTabSwitchButton::OmniboxTabSwitchButton(OmniboxPopupContentsView* model,
                                               OmniboxResultView* result_view,
                                               const base::string16& hint,
                                               const base::string16& hint_short,
                                               const gfx::VectorIcon& icon)
    : MdTextButton(result_view, views::style::CONTEXT_BUTTON_MD),
      model_(model),
      result_view_(result_view),
      initialized_(false),
      animation_(new gfx::SlideAnimation(this)),
      hint_(hint),
      hint_short_(hint_short) {
  SetBgColorOverride(GetBackgroundColor());
  SetImage(STATE_NORMAL, gfx::CreateVectorIcon(
                             icon, GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
                             gfx::kChromeIconGrey));
  SetImageLabelSpacing(8);
  if (!calculated_widths_) {
    icon_only_width_ = MdTextButton::CalculatePreferredSize().width();
    SetText(hint_short_);
    short_text_width_ = MdTextButton::CalculatePreferredSize().width();
    SetText(hint_);
    full_text_width_ = MdTextButton::CalculatePreferredSize().width();
    calculated_widths_ = true;
  } else {
    SetText(hint_);
  }
  SetTooltipText(hint_);
  set_corner_radius(CalculatePreferredSize().height() / 2.f);
  animation_->SetSlideDuration(500);
  SetElideBehavior(gfx::FADE_TAIL);

  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetHasFocusPredicate([](View* view) {
    auto* button = static_cast<OmniboxTabSwitchButton*>(view);
    return button->IsSelected();
  });
}

OmniboxTabSwitchButton::~OmniboxTabSwitchButton() = default;

gfx::Size OmniboxTabSwitchButton::CalculatePreferredSize() const {
  gfx::Size size = MdTextButton::CalculatePreferredSize();
  size.set_height(kButtonHeight);
  int current_width = animation_->CurrentValueBetween(
      static_cast<int>(start_width_), static_cast<int>(goal_width_));
  size.set_width(current_width);
  return size;
}

void OmniboxTabSwitchButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  MdTextButton::OnBoundsChanged(previous_bounds);
  focus_ring()->SetPath(GetFocusRingPath());
}

std::unique_ptr<views::InkDropMask> OmniboxTabSwitchButton::CreateInkDropMask()
    const {
  return std::make_unique<views::RoundRectInkDropMask>(
      size(), gfx::Insets(), CalculatePreferredSize().height() / 2.f);
}

void OmniboxTabSwitchButton::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation != animation_.get()) {
    MdTextButton::AnimationProgressed(animation);
    return;
  }

  // If done shrinking, correct text.
  if (animation_->GetCurrentValue() == 1 && goal_width_ < start_width_)
    SetText(goal_text_);
  result_view_->Layout();
  result_view_->SchedulePaint();
}

void OmniboxTabSwitchButton::StateChanged(ButtonState old_state) {
  if (state() == STATE_NORMAL) {
    // If used to be pressed, transfer ownership.
    if (old_state == STATE_PRESSED) {
      SetBgColorOverride(GetBackgroundColor());
      SetMouseHandler(parent());
      if (model_->IsButtonSelected())
        model_->UnselectButton();
    // Otherwise was hovered. Update color if not selected.
    } else if (!model_->IsButtonSelected()) {
      SetBgColorOverride(GetBackgroundColor());
    }
  }
  if (state() == STATE_HOVERED) {
    if (old_state == STATE_NORMAL) {
      SetBgColorOverride(GetBackgroundColor());
    }
  }
  if (state() == STATE_PRESSED)
    SetPressed();
  MdTextButton::StateChanged(old_state);
}

void OmniboxTabSwitchButton::UpdateBackground() {
  focus_ring()->SchedulePaint();
  SetBgColorOverride(GetBackgroundColor());
}

void OmniboxTabSwitchButton::ProvideWidthHint(size_t parent_width) {
  size_t preferred_width = CalculateGoalWidth(parent_width, &goal_text_);
  if (!initialized_) {
    initialized_ = true;
    goal_width_ = preferred_width;
    animation_->Reset(1);
    SetText(goal_text_);
    return;
  }
  if (preferred_width != goal_width_) {
    goal_width_ = preferred_width;
    start_width_ = width();
    // If growing/showing, set text-to-be and grow into it.
    if (goal_width_ > start_width_)
      SetText(goal_text_);
    animation_->Reset(0);
    animation_->Show();
  }
}

void OmniboxTabSwitchButton::ProvideFocusHint() {
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void OmniboxTabSwitchButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACC_TAB_SWITCH_BUTTON));
  // Although this appears visually as a button, expose as a list box option so
  // that it matches the other options within its list box container.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsSelected());
}

bool OmniboxTabSwitchButton::IsSelected() const {
  // Is this result selected and is button selected?
  return result_view_->IsSelected() && model_->IsButtonSelected();
}

SkPath OmniboxTabSwitchButton::GetFocusRingPath() const {
  SkPath path;
  path.addRRect(SkRRect::MakeRectXY(RectToSkRect(GetLocalBounds()),
                                    height() / 2.f, height() / 2.f));
  return path;
}

SkColor OmniboxTabSwitchButton::GetBackgroundColor() const {
  return GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND,
                         result_view_->GetTint(),
                         state() == STATE_HOVERED ? OmniboxPartState::HOVERED
                                                  : OmniboxPartState::NORMAL);
}

void OmniboxTabSwitchButton::SetPressed() {
  SetBgColorOverride(color_utils::AlphaBlend(
      GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND, result_view_->GetTint(),
                      OmniboxPartState::SELECTED),
      SK_ColorBLACK, 0.8 * 255));
}

size_t OmniboxTabSwitchButton::CalculateGoalWidth(size_t parent_width,
                                                  base::string16* goal_text) {
  if (full_text_width_ * 5 <= parent_width) {
    *goal_text = hint_;
    return full_text_width_;
  }
  if (short_text_width_ * 5 <= parent_width) {
    *goal_text = hint_short_;
    return short_text_width_;
  }
  *goal_text = base::string16();
  if (icon_only_width_ * 5 <= parent_width)
    return icon_only_width_;
  return 0;
}
