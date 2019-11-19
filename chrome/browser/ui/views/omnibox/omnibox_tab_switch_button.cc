// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
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
#include "ui/views/controls/highlight_path_generator.h"

// static
bool OmniboxTabSwitchButton::calculated_widths_ = false;
int OmniboxTabSwitchButton::icon_only_width_;
int OmniboxTabSwitchButton::short_text_width_;
int OmniboxTabSwitchButton::full_text_width_;

OmniboxTabSwitchButton::OmniboxTabSwitchButton(
    OmniboxPopupContentsView* popup_contents_view,
    OmniboxResultView* result_view,
    const base::string16& hint,
    const base::string16& hint_short,
    const gfx::VectorIcon& icon,
    const ui::ThemeProvider* theme_provider)
    : MdTextButton(result_view, views::style::CONTEXT_BUTTON_MD),
      popup_contents_view_(popup_contents_view),
      result_view_(result_view),
      hint_(hint),
      hint_short_(hint_short),
      theme_provider_(theme_provider) {
  views::InstallPillHighlightPathGenerator(this);
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
  SetPreferredSize({full_text_width_, 32});
  SetCornerRadius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, GetPreferredSize()));
  SetTooltipText(hint_);
  SetElideBehavior(gfx::FADE_TAIL);

  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetHasFocusPredicate([](View* view) {
    auto* button = static_cast<OmniboxTabSwitchButton*>(view);
    return button->IsSelected();
  });
}

OmniboxTabSwitchButton::~OmniboxTabSwitchButton() = default;

void OmniboxTabSwitchButton::StateChanged(ButtonState old_state) {
  if (state() == STATE_NORMAL) {
    // If used to be pressed, transfer ownership.
    if (old_state == STATE_PRESSED) {
      SetBgColorOverride(GetBackgroundColor());
      SetMouseHandler(parent());
      if (popup_contents_view_->IsButtonSelected())
        popup_contents_view_->UnselectButton();
    } else if (!popup_contents_view_->IsButtonSelected()) {
      // Otherwise, the button was hovered. Update color if not selected.
      SetBgColorOverride(GetBackgroundColor());
    }
  } else if (state() == STATE_HOVERED) {
    if (old_state == STATE_NORMAL) {
      SetBgColorOverride(GetBackgroundColor());
    }
  } else if (state() == STATE_PRESSED) {
    SetPressed();
  }
  MdTextButton::StateChanged(old_state);
}

void OmniboxTabSwitchButton::UpdateBackground() {
  focus_ring()->SchedulePaint();
  SetBgColorOverride(GetBackgroundColor());
}

void OmniboxTabSwitchButton::ProvideWidthHint(int parent_width) {
  base::string16 text;
  int preferred_width = CalculateGoalWidth(parent_width, &text);
  SetText(text);
  SetPreferredSize({preferred_width, GetPreferredSize().height()});
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
  return result_view_->IsSelected() && popup_contents_view_->IsButtonSelected();
}

SkColor OmniboxTabSwitchButton::GetBackgroundColor() const {
  return GetOmniboxColor(theme_provider_, OmniboxPart::RESULTS_BACKGROUND,
                         state() == STATE_HOVERED ? OmniboxPartState::HOVERED
                                                  : OmniboxPartState::NORMAL);
}

void OmniboxTabSwitchButton::SetPressed() {
  SetBgColorOverride(color_utils::AlphaBlend(
      GetOmniboxColor(theme_provider_, OmniboxPart::RESULTS_BACKGROUND,
                      OmniboxPartState::SELECTED),
      SK_ColorBLACK, 0.8f));
}

int OmniboxTabSwitchButton::CalculateGoalWidth(int parent_width,
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
  return (icon_only_width_ * 5 <= parent_width) ? icon_only_width_ : 0;
}
