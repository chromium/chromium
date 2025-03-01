// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace glic {

GlicButton::GlicButton(TabStripController* tab_strip_controller,
                       PressedCallback pressed_callback,
                       PressedCallback close_pressed_callback,
                       base::RepeatingClosure hovered_callback,
                       const gfx::VectorIcon& icon,
                       const std::u16string& tooltip)
    : TabStripNudgeButton(tab_strip_controller,
                          std::move(pressed_callback),
                          std::move(close_pressed_callback),
                          tooltip,
                          kGlicNudgeButtonElementId,
                          Edge::kNone,
                          icon),
      tab_strip_controller_(tab_strip_controller),
      hovered_callback_(std::move(hovered_callback)) {
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);

  SetTooltipText(tooltip);
  GetViewAccessibility().SetName(tooltip);

  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);

  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  UpdateColors();

  SetVisible(true);

  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
}

GlicButton::~GlicButton() = default;

void GlicButton::SetShowState(bool show) {
  show_state_ = show;

  // Don't update visibility while the nudge is showing.
  if (is_showing_nudge_) {
    return;
  }
  SetVisible(show_state_);

  PreferredSizeChanged();
}

void GlicButton::SetIcon(const gfx::VectorIcon& icon) {
  SetVectorIcon(icon);
}

void GlicButton::SetIsShowingNudge(bool is_showing) {
  is_showing_nudge_ = is_showing;
  PreferredSizeChanged();
}

gfx::Size GlicButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int full_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();

  const int height = TabStripControlButton::CalculatePreferredSize(
                         views::SizeBounds(full_width, available_size.height()))
                         .height();
  // Set collapsed size to a square.
  const int collapsed_width = height;
  const int width = std::lerp(collapsed_width, full_width, GetWidthFactor());

  return gfx::Size(width, height);
}

void GlicButton::StateChanged(ButtonState old_state) {
  TabStripNudgeButton::StateChanged(old_state);
  if (old_state == STATE_NORMAL && GetState() == STATE_HOVERED &&
      hovered_callback_) {
    hovered_callback_.Run();
  }
}

void GlicButton::SetDropToAttachIndicator(bool indicate) {
  if (indicate) {
    SetBackgroundFrameActiveColorId(ui::kColorSysStateHeaderHover);
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  }
}

gfx::Rect GlicButton::GetBoundsWithInset() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

BEGIN_METADATA(GlicButton)
END_METADATA

}  // namespace glic
