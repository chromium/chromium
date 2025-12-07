// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_actor_task_icon.h"

#include <string>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace glic {

constexpr int kActorNudgeLabelMargin = 6;

GlicActorTaskIcon::GlicActorTaskIcon(TabStripController* tab_strip_controller,
                                     PressedCallback pressed_callback)
    : TabStripNudgeButton(
          tab_strip_controller,
          std::move(pressed_callback),
          views::Button::PressedCallback(),
          std::u16string(),
          kGlicActorTaskIconElementId,
          Edge::kNone,
          base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)
              ? gfx::VectorIcon::EmptyIcon()
              : kScreensaverAutoIcon,
          /*show_close_button=*/false),
      tab_strip_controller_(tab_strip_controller) {
  SetProperty(views::kElementIdentifierKey, kGlicActorTaskIconElementId);

  if (base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)) {
    // Explicitly overwrite the horizontal margins. The underlying
    // TabStripNudgeButton calculates defaults that account for a close button,
    // which is not present here.
    label()->SetProperty(views::kMarginsKey,
                         gfx::Insets().set_left_right(kActorNudgeLabelMargin,
                                                      kActorNudgeLabelMargin));
  }

  SetTaskIconToDefault();
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
  int width = std::lerp(height, full_width, GetWidthFactor());
  if (base::FeatureList::IsEnabled(features::kGlicActorUiNudgeRedesign)) {
    // Task Icon shouldn't show for the redesign, making the default width 0.
    width = std::lerp(0, full_width, GetWidthFactor());
  }

  return gfx::Size(width, height);
}

void GlicActorTaskIcon::SetIsShowingNudge(bool is_showing) {
  if (!is_showing) {
    SetText(std::u16string());
  }
  is_showing_nudge_ = is_showing;
  PreferredSizeChanged();
}

void GlicActorTaskIcon::SetFloatyOpenTooltipText() {
  // TODO(crbug.com/431015299): Replace with finalized strings when ready.
  const std::u16string glic_actor_task_icon_floaty_open_tooltip_text =
      u"Close Gemini in Chrome";
  SetTooltipText(glic_actor_task_icon_floaty_open_tooltip_text);
}

void GlicActorTaskIcon::SetFloatyClosedTooltipText() {
  // TODO(crbug.com/431015299): Replace with finalized strings when ready.
  const std::u16string glic_actor_task_icon_floaty_closed_tooltip_text =
      u"Open Gemini in Chrome";
  SetTooltipText(glic_actor_task_icon_floaty_closed_tooltip_text);
}

void GlicActorTaskIcon::SetDefaultColors() {
  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);
}

void GlicActorTaskIcon::HighlightTaskIcon() {
  SetBackgroundFrameActiveColorId(kColorTabBackgroundInactiveHoverFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorTabBackgroundInactiveHoverFrameInactive);
}

void GlicActorTaskIcon::SetTaskIconToDefault() {
  SetText(std::u16string());
  SetDefaultColors();
  SetFloatyClosedTooltipText();
}

void GlicActorTaskIcon::ShowNudgeLabel(const std::u16string nudge_label) {
  HighlightTaskIcon();
  SetText(nudge_label);
  SetTooltipText(nudge_label);
}

void GlicActorTaskIcon::RefreshBackground() {
  UpdateColors();
}

GlicActorTaskIcon::~GlicActorTaskIcon() = default;

BEGIN_METADATA(GlicActorTaskIcon)
END_METADATA

}  // namespace glic
