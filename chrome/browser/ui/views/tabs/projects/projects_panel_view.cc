// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"

#include <memory>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kProjectPanelWidth = 240;
constexpr gfx::Insets kRegionInteriorMargins = gfx::Insets::VH(12, 12);
}  // namespace

ProjectsPanelView::ProjectsPanelView(actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kRegionInteriorMargins)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));
  SetBackground(views::CreateSolidBackground(ui::kColorFrameActive));

  // The vertical tab strip contains ScrollViews that paint to a layer. This
  // view must also paint to a layer to ensure it overlays those components.
  SetPaintToLayer();

  controls_view_ = AddChildView(std::make_unique<ProjectsPanelControlsView>(
      root_action_item_.get(), action_view_controller_.get()));

  tab_groups_view_ = AddChildView(std::make_unique<ProjectsPanelTabGroupsView>(
      root_action_item_.get(), action_view_controller_.get()));

  SetVisible(false);
  SetPreferredSize(gfx::Size(kProjectPanelWidth, 0));
  SetProperty(views::kElementIdentifierKey, kProjectsPanelViewElementId);
}

ProjectsPanelView::~ProjectsPanelView() = default;

bool ProjectsPanelView::IsPositionInWindowCaption(const gfx::Point& point) {
  gfx::Point point_in_target = point;
  views::View::ConvertPointToTarget(this, controls_view_, &point_in_target);
  if (controls_view_->HitTestPoint(point_in_target)) {
    return controls_view_->IsPositionInWindowCaption(point_in_target);
  }

  return false;
}

void ProjectsPanelView::OnProjectsPanelStateChanged(
    ProjectsPanelStateController* state_controller) {
  TooltipTextChanged();
  SetVisible(state_controller->IsProjectsPanelVisible());
}

BEGIN_METADATA(ProjectsPanelView)
END_METADATA
