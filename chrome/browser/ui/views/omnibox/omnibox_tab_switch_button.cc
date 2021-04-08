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
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_impl_macros.h"

// static
bool OmniboxTabSwitchButton::calculated_widths_ = false;
int OmniboxTabSwitchButton::icon_only_width_;
int OmniboxTabSwitchButton::short_text_width_;
int OmniboxTabSwitchButton::full_text_width_;

OmniboxTabSwitchButton::OmniboxTabSwitchButton(
    PressedCallback callback,
    OmniboxPopupContentsView* popup_contents_view,
    OmniboxResultView* result_view,
    const std::u16string& hint,
    const std::u16string& hint_short,
    const gfx::VectorIcon& icon)
    : MdTextButton(std::move(callback),
                   std::u16string(),
                   views::style::CONTEXT_BUTTON_MD),
      popup_contents_view_(popup_contents_view),
      result_view_(result_view),
      hint_(hint),
      hint_short_(hint_short) {
  views::InstallPillHighlightPathGenerator(this);
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
      views::Emphasis::kMaximum, GetPreferredSize()));
  SetTooltipText(hint_);
  SetElideBehavior(gfx::FADE_TAIL);

  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetHasFocusPredicate([](View* view) {
    auto* button = static_cast<OmniboxTabSwitchButton*>(view);
    return button->IsSelected();
  });
}

OmniboxTabSwitchButton::~OmniboxTabSwitchButton() = default;

void OmniboxTabSwitchButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  MdTextButton::OnBoundsChanged(previous_bounds);

  std::u16string text = hint_;
  if (width() <= icon_only_width_)
    text = std::u16string();
  else if (width() <= short_text_width_)
    text = hint_short_;
  SetText(text);
}

void OmniboxTabSwitchButton::StateChanged(ButtonState old_state) {
  MdTextButton::StateChanged(old_state);
  if (GetState() == STATE_NORMAL && old_state == STATE_PRESSED) {
    SetMouseAndGestureHandler(parent());
    if (popup_contents_view_->model()->selected_line_state() ==
        OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH)
      popup_contents_view_->UnselectButton();
  }
}

void OmniboxTabSwitchButton::OnThemeChanged() {
  views::MdTextButton::OnThemeChanged();
  SetBgColorOverride(GetOmniboxColor(GetThemeProvider(),
                                     OmniboxPart::RESULTS_BACKGROUND,
                                     OmniboxPartState::NORMAL));
}

void OmniboxTabSwitchButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACC_TAB_SWITCH_BUTTON));
  // Although this appears visually as a button, expose as a list box option so
  // that it matches the other options within its list box container.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsSelected());
}

// static
views::FlexRule OmniboxTabSwitchButton::GetFlexRule() {
  // The rule below snaps between full, short, icon-only, and zero widths.
  return base::BindRepeating(
      [](const views::View* view, const views::SizeBounds& maximum_size) {
        gfx::Size preferred_size = view->GetPreferredSize();
        int width;
        if (!maximum_size.width().is_bounded()) {
          // Until width is bounded, return 0 to allocate flex excess correctly.
          width = 0;
        } else if (full_text_width_ <= maximum_size.width()) {
          width = full_text_width_;
        } else if (short_text_width_ <= maximum_size.width()) {
          width = short_text_width_;
        } else if (icon_only_width_ < maximum_size.width()) {
          width = icon_only_width_;
        } else {
          // Available width is too small to fit even the icon only. So don't
          // show button at all.
          width = 0;
        }
        return gfx::Size(width, preferred_size.height());
      });
}

void OmniboxTabSwitchButton::UpdateBackground() {
  focus_ring()->SchedulePaint();
}

bool OmniboxTabSwitchButton::IsSelected() const {
  // Is this result selected and is button selected?
  return result_view_->GetMatchSelected() &&
         popup_contents_view_->model()->selected_line_state() ==
             OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH;
}

BEGIN_METADATA(OmniboxTabSwitchButton, views::MdTextButton)
END_METADATA
