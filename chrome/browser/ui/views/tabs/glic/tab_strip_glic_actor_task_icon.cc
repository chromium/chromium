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

constexpr int kActorNudgeLabelMargin = 6;

TabStripGlicActorTaskIcon::TabStripGlicActorTaskIcon(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback pressed_callback)
    : TabStripNudgeButton(browser_window_interface,
                          std::move(pressed_callback),
                          views::Button::PressedCallback(),
                          std::u16string(),
                          kGlicActorTaskIconElementId,
                          Edge::kNone,
                          GetTaskIcon(),
                          /*show_close_button=*/false),
      browser_window_interface_(browser_window_interface) {
  SetProperty(views::kElementIdentifierKey, kGlicActorTaskIconElementId);

  // Explicitly overwrite the horizontal margins. The underlying
  // TabStripNudgeButton calculates defaults that account for a close button,
  // which is not present here.
  label()->SetProperty(views::kMarginsKey,
                       gfx::Insets().set_left_right(kActorNudgeLabelMargin,
                                                    kActorNudgeLabelMargin));

  SetTaskIconToDefault();

  // The task icon will only ever be shown with the GlicButton, so can always
  // set the corner radii for split button styling.
  SetLeftRightCornerRadii(kSplitButtonFlatEdgeRadius,
                          kSplitButtonRoundedEdgeRadius);
  TabStripControlButton::SetInkdropHoverColorId(
      kColorTabBackgroundInactiveHoverFrameActive);

  UpdateColors();

  SetFocusBehavior(FocusBehavior::ALWAYS);

  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
}

gfx::Size TabStripGlicActorTaskIcon::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int full_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();

  const int height = TabStripControlButton::CalculatePreferredSize(
                         views::SizeBounds(full_width, available_size.height()))
                         .height();
  int icon_only_width = height;
  int width = 0;
  switch (animation_mode_) {
    case AnimationMode::kEntry:
      width = std::lerp(0, icon_only_width, GetWidthFactor());
      break;
    case AnimationMode::kNudge:
      int min_width = 0;
      min_width = icon_only_width;
      width = std::lerp(min_width, full_width, GetWidthFactor());
      break;
  }
  return gfx::Size(width, height);
}

void TabStripGlicActorTaskIcon::SetAnimationMode(AnimationMode mode) {
  if (animation_mode_ != mode) {
    animation_mode_ = mode;
    PreferredSizeChanged();
  }
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

void TabStripGlicActorTaskIcon::SetPressedColor(bool is_pressed) {
  SetHighlighted(is_pressed);
  UpdateColors();
}

void TabStripGlicActorTaskIcon::NotifyClick(const ui::Event& event) {
  // TabStripControlButton manipulates the ink drop in its NotifyClick(), so
  // if we're using the ink drop to show the button's pressed state, skip
  // TabStripControlButton::NotifyClick() and just call the base
  // NotifyClick().
  LabelButton::NotifyClick(event);
}

void TabStripGlicActorTaskIcon::SetTaskIconToDefault() {
  SetText(std::u16string());
  SetTooltipText(l10n_util::GetStringUTF16(IDS_ACTOR_TASK_INDICATOR_TOOLTIP));
  SetDefaultColors();
}

void TabStripGlicActorTaskIcon::ShowNudgeLabel(
    const std::u16string nudge_label) {
  SetText(nudge_label);
  SetTooltipText(nudge_label);
}

void TabStripGlicActorTaskIcon::RefreshBackground() {
  UpdateColors();
}

void TabStripGlicActorTaskIcon::AddedToWidget() {
  TabStripNudgeButton::AddedToWidget();
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  window_did_become_active_subscription_ =
      browser_window_interface_->RegisterDidBecomeActive(base::BindRepeating(
          &TabStripGlicActorTaskIcon::OnBrowserWindowDidBecomeActive,
          base::Unretained(this)));
  window_did_become_inactive_subscription_ =
      browser_window_interface_->RegisterDidBecomeInactive(base::BindRepeating(
          &TabStripGlicActorTaskIcon::OnBrowserWindowDidBecomeInactive,
          base::Unretained(this)));

  UpdateInkdropHoverColor(browser_window_interface_->IsActive());
}

void TabStripGlicActorTaskIcon::RemovedFromWidget() {
  window_did_become_active_subscription_ = {};
  window_did_become_inactive_subscription_ = {};
  TabStripNudgeButton::RemovedFromWidget();
}

void TabStripGlicActorTaskIcon::OnBrowserWindowDidBecomeActive(
    BrowserWindowInterface* bwi) {
  UpdateInkdropHoverColor(true);
}

void TabStripGlicActorTaskIcon::OnBrowserWindowDidBecomeInactive(
    BrowserWindowInterface* bwi) {
  UpdateInkdropHoverColor(false);
}

void TabStripGlicActorTaskIcon::UpdateInkdropHoverColor(bool is_frame_active) {
  SetInkdropHoverColorId(is_frame_active
                             ? kColorTabBackgroundInactiveHoverFrameActive
                             : kColorTabBackgroundInactiveHoverFrameInactive);
  UpdateColors();
}

gfx::Rect TabStripGlicActorTaskIcon::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

bool TabStripGlicActorTaskIcon::ShouldUseGlicButtonAltIconBackgroundColor() {
  // LINT.IfChange(ShouldUseGlicButtonAltIconBackgroundColor)
  return base::FeatureList::IsEnabled(
             kGlicActorTaskIconUseGlicButtonAltIconBackgroundColor) &&
         base::FeatureList::IsEnabled(features::kGlicEntrypointVariations) &&
         features::kGlicEntrypointVariationsAltIcon.Get();
  // LINT.ThenChange(//chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.cc:ShouldUseAltIcon)
}

TabStripGlicActorTaskIcon::~TabStripGlicActorTaskIcon() = default;

BEGIN_METADATA(TabStripGlicActorTaskIcon)
END_METADATA

}  // namespace glic
