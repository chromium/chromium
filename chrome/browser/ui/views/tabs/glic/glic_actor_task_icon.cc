// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic/glic_actor_task_icon.h"

#include <string>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
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
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#endif

namespace glic {

const gfx::VectorIcon& GetTaskIcon() {
#if BUILDFLAG(ENABLE_GLIC)
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    return glic::GlicVectorIconManager::GetVectorIcon(
        IDR_ACTOR_AUTO_BROWSE_ICON);
  }
#endif
  return gfx::VectorIcon::EmptyIcon();
}

constexpr int kActorNudgeLabelMargin = 6;

GlicActorTaskIcon::GlicActorTaskIcon(
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

  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    // The task icon will only ever be shown with the GlicButton, so can always
    // set the corner radii for split button styling.
    SetLeftRightCornerRadii(kSplitButtonFlatEdgeRadius,
                            kSplitButtonRoundedEdgeRadius);
    TabStripControlButton::SetInkdropHoverColorId(
        kColorTabBackgroundInactiveHoverFrameActive);
  }
  UpdateColors();

  SetFocusBehavior(FocusBehavior::ALWAYS);

  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
}

gfx::Size GlicActorTaskIcon::CalculatePreferredSize(
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
      if (base::FeatureList::IsEnabled(
              features::kGlicActorUiGlobalTaskIndicator)) {
        min_width = icon_only_width;
      }
      width = std::lerp(min_width, full_width, GetWidthFactor());
      break;
  }
  return gfx::Size(width, height);
}

void GlicActorTaskIcon::SetAnimationMode(AnimationMode mode) {
  if (animation_mode_ != mode) {
    animation_mode_ = mode;
    PreferredSizeChanged();
  }
}

void GlicActorTaskIcon::SetIsShowingNudge(bool is_showing) {
  if (!is_showing) {
    SetText(std::u16string());
  }
  is_showing_nudge_ = is_showing;
  PreferredSizeChanged();
}

void GlicActorTaskIcon::SetDefaultColors() {
  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);
}

// TODO(crbug.com/470120703): Remove this method when GlobalTaskIndicator is
// enabled by default.
// NOTE: This method is only used for the nudge and has a misleading name.
void GlicActorTaskIcon::HighlightTaskIcon() {
  SetBackgroundFrameActiveColorId(kColorTabBackgroundInactiveHoverFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorTabBackgroundInactiveHoverFrameInactive);
}

void GlicActorTaskIcon::SetPressedColor(bool is_pressed) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicActorUiGlobalTaskIndicator)) {
    return;
  }

  SetHighlighted(is_pressed);
  UpdateColors();
}

void GlicActorTaskIcon::NotifyClick(const ui::Event& event) {
  // TabStripControlButton manipulates the ink drop in its NotifyClick(), so
  // if we're using the ink drop to show the button's pressed state, skip
  // TabStripControlButton::NotifyClick() and just call the base
  // NotifyClick().
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    LabelButton::NotifyClick(event);
  } else {
    TabStripNudgeButton::NotifyClick(event);
  }
}

void GlicActorTaskIcon::SetTaskIconToDefault() {
  SetText(std::u16string());
  SetTooltipText(l10n_util::GetStringUTF16(IDS_ACTOR_TASK_INDICATOR_TOOLTIP));
  SetDefaultColors();
}

void GlicActorTaskIcon::ShowNudgeLabel(const std::u16string nudge_label) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicActorUiGlobalTaskIndicator)) {
    HighlightTaskIcon();
  }
  SetText(nudge_label);
  SetTooltipText(nudge_label);
}

void GlicActorTaskIcon::RefreshBackground() {
  UpdateColors();
}

void GlicActorTaskIcon::AddedToWidget() {
  TabStripNudgeButton::AddedToWidget();
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  window_did_become_active_subscription_ =
      browser_window_interface_->RegisterDidBecomeActive(base::BindRepeating(
          &GlicActorTaskIcon::OnBrowserWindowDidBecomeActive,
          base::Unretained(this)));
  window_did_become_inactive_subscription_ =
      browser_window_interface_->RegisterDidBecomeInactive(base::BindRepeating(
          &GlicActorTaskIcon::OnBrowserWindowDidBecomeInactive,
          base::Unretained(this)));

  UpdateInkdropHoverColor(browser_window_interface_->IsActive());
}

void GlicActorTaskIcon::RemovedFromWidget() {
  window_did_become_active_subscription_ = {};
  window_did_become_inactive_subscription_ = {};
  TabStripNudgeButton::RemovedFromWidget();
}

void GlicActorTaskIcon::OnBrowserWindowDidBecomeActive(
    BrowserWindowInterface* bwi) {
  UpdateInkdropHoverColor(true);
}

void GlicActorTaskIcon::OnBrowserWindowDidBecomeInactive(
    BrowserWindowInterface* bwi) {
  UpdateInkdropHoverColor(false);
}

void GlicActorTaskIcon::UpdateInkdropHoverColor(bool is_frame_active) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUiGlobalTaskIndicator)) {
    SetInkdropHoverColorId(is_frame_active
                               ? kColorTabBackgroundInactiveHoverFrameActive
                               : kColorTabBackgroundInactiveHoverFrameInactive);
    UpdateColors();
  }
}

gfx::Rect GlicActorTaskIcon::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

GlicActorTaskIcon::~GlicActorTaskIcon() = default;

BEGIN_METADATA(GlicActorTaskIcon)
END_METADATA

}  // namespace glic
