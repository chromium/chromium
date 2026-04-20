// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_actor_task_icon.h"

#include <string>

#include "base/feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/glic/glic_actor_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace glic {

BASE_FEATURE(kGlicActorTaskIconUseGlicButtonAltIconBackgroundColor,
             base::FEATURE_ENABLED_BY_DEFAULT);

const gfx::VectorIcon& GetTaskIcon() {
  return glic::GlicVectorIconManager::GetVectorIcon(IDR_ACTOR_AUTO_BROWSE_ICON);
}

TabStripGlicActorTaskIcon::TabStripGlicActorTaskIcon(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback pressed_callback)
    : GlicActorTaskIcon<TabStripNudgeButton>(browser_window_interface,
                                             /* TabStripNudgeButton args */
                                             browser_window_interface,
                                             std::move(pressed_callback),
                                             views::Button::PressedCallback(),
                                             std::u16string(),
                                             kGlicActorTaskIconElementId,
                                             Edge::kNone,
                                             GetTaskIcon(),
                                             /*show_close_button=*/false),
      browser_window_interface_(browser_window_interface) {
  SetTaskIconToDefault();

  // The task icon will only ever be shown with the GlicButton, so can always
  // set the corner radii for split button styling.
  SetLeftRightCornerRadii(kSplitButtonFlatEdgeRadius,
                          kSplitButtonRoundedEdgeRadius);
  TabStripControlButton::SetInkdropHoverColorId(
      kColorTabBackgroundInactiveHoverFrameActive);

  UpdateColors();
}

bool TabStripGlicActorTaskIcon::GetIsShowingNudge() const {
  return is_showing_nudge_;
}

void TabStripGlicActorTaskIcon::SetIsShowingNudge(bool is_showing) {
  if (!is_showing) {
    SetText(std::u16string());
  }
  is_showing_nudge_ = is_showing;
  PreferredSizeChanged();
}

void TabStripGlicActorTaskIcon::SetDefaultColors() {
  if (ShouldUseGlicButtonAltIconBackgroundColor()) {
    SetForegroundFrameActiveColorId(ui::kColorSysOnSurface);
    SetBackgroundFrameActiveColorId(ui::kColorSysBase);
    SetTextColor(STATE_DISABLED, ui::kColorLabelForegroundDisabled);
  } else {
    SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  }
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);
}

void TabStripGlicActorTaskIcon::NotifyClick(const ui::Event& event) {
  // TabStripControlButton manipulates the ink drop in its NotifyClick(), so
  // if we're using the ink drop to show the button's pressed state, skip
  // TabStripControlButton::NotifyClick() and just call the base
  // NotifyClick().
  LabelButton::NotifyClick(event);
}

gfx::Rect TabStripGlicActorTaskIcon::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

bool TabStripGlicActorTaskIcon::ShouldUseGlicButtonAltIconBackgroundColor() {
  return base::FeatureList::IsEnabled(
      kGlicActorTaskIconUseGlicButtonAltIconBackgroundColor);
}

TabStripGlicActorTaskIcon::~TabStripGlicActorTaskIcon() = default;

BEGIN_METADATA(TabStripGlicActorTaskIcon)
END_METADATA

}  // namespace glic
