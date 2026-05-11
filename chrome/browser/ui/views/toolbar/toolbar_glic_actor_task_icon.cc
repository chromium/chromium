// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon.h"

#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/glic/glic_actor_task_icon.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace glic {

ToolbarGlicActorTaskIcon::ToolbarGlicActorTaskIcon(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback pressed_callback)
    : GlicActorTaskIcon<ToolbarButton>(browser_window_interface,
                                       pressed_callback) {}

ToolbarGlicActorTaskIcon::~ToolbarGlicActorTaskIcon() = default;

void ToolbarGlicActorTaskIcon::AddedToWidget() {
  split_rounded_edge_radius_ = GetRoundedCornerRadius();
  SetLeftRightCornerRadii(kSplitLeftEdgeRadius, GetSplitRoundedEdgeRadius());

  SetDefaultBackgroundColorId(kColorToolbarGlicButtonBackgroundDefault);
  UpdateIconsWithStandardColors(
      glic::GlicVectorIconManager::GetVectorIcon(IDR_ACTOR_AUTO_BROWSE_ICON));
  GlicActorTaskIcon<ToolbarButton>::AddedToWidget();
}

void ToolbarGlicActorTaskIcon::SetForegroundFrameActiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}

void ToolbarGlicActorTaskIcon::SetForegroundFrameInactiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}

void ToolbarGlicActorTaskIcon::SetBackgroundFrameActiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}

void ToolbarGlicActorTaskIcon::SetBackgroundFrameInactiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}

void ToolbarGlicActorTaskIcon::UpdateColors() {
  ToolbarButton::UpdateColorsAndInsets();
}

void ToolbarGlicActorTaskIcon::SetIsShowingNudge(bool is_showing) {
  GlicActorTaskIcon<ToolbarButton>::SetIsShowingNudge(is_showing);
  if (is_showing) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetCloseButtonFocusBehavior(FocusBehavior::ALWAYS);
  } else {
    SetFocusBehavior(FocusBehavior::NEVER);
    SetCloseButtonFocusBehavior(FocusBehavior::NEVER);
  }
}

void ToolbarGlicActorTaskIcon::SetLeftRightCornerRadii(int left, int right) {
  left_corner_radius_ = left;
  right_corner_radius_ = right;
}

float ToolbarGlicActorTaskIcon::GetCornerRadiusFor(
    ToolbarButton::Edge edge) const {
  return edge == ToolbarButton::Edge::kLeft
             ? left_corner_radius_.value_or(GetRoundedCornerRadius())
             : GetRoundedCornerRadius();
}

int ToolbarGlicActorTaskIcon::GetSplitRoundedEdgeRadius() {
  return split_rounded_edge_radius_;
}

BEGIN_METADATA(ToolbarGlicActorTaskIcon)
END_METADATA
}  // namespace glic
