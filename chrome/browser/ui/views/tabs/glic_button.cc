// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace glic {

GlicButton::GlicButton(TabStripController* tab_strip_controller,
                       PressedCallback callback,
                       const gfx::VectorIcon& icon,
                       const std::u16string& tooltip)
    : TabStripControlButton(tab_strip_controller, std::move(callback), icon),
      tab_strip_controller_(tab_strip_controller) {
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);

  SetTooltipText(tooltip);
  GetViewAccessibility().SetName(tooltip);

  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  UpdateColors();
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
  // Don't show the button while the nudge is showing and restore
  // to original show state when the nudge is no longer showing.
  is_showing_nudge_ = is_showing;

  if (is_showing_nudge_) {
    SetVisible(false);
  } else {
    SetVisible(show_state_);
  }

  PreferredSizeChanged();
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
