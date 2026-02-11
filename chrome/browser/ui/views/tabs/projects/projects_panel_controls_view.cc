// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"

#include <memory>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

ProjectsPanelControlsView::ProjectsPanelControlsView(
    actions::ActionItem* root_action_item,
    views::ActionViewController* action_view_controller) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  std::unique_ptr<TopContainerButton> container_button =
      std::make_unique<TopContainerButton>();
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionToggleProjectsPanel, root_action_item);
  CHECK(action_item);

  action_view_controller->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  projects_button_ = AddChildView(std::move(container_button));
  projects_button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  projects_button_->SetProperty(views::kElementIdentifierKey,
                                kProjectsPanelButtonElementId);
  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelControlsViewElementId);
}

ProjectsPanelControlsView::~ProjectsPanelControlsView() = default;

views::ProposedLayout ProjectsPanelControlsView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  gfx::Size host_size =
      gfx::Size(size_bounds.width().is_bounded() ? size_bounds.width().value()
                                                 : parent()->width(),
                GetLayoutConstant(
                    LayoutConstant::kVerticalTabStripTopButtonContainerHeight));

  CHECK(projects_button_);

  const gfx::Size projects_button_pref_size =
      projects_button_->GetPreferredSize();

  int current_x = host_size.width();
  int current_y = host_size.height();

  // Calculate bounds to right-align the button horizontally and center it
  // vertically within the available space.
  gfx::Rect projects_button_bounds(
      current_x - projects_button_pref_size.width(),
      current_y -
          (GetLayoutConstant(
               LayoutConstant::kVerticalTabStripTopButtonContainerHeight) +
           projects_button_pref_size.height()) /
              2,
      projects_button_pref_size.width(), projects_button_pref_size.height());
  layout.child_layouts.emplace_back(
      projects_button_.get(), projects_button_->GetVisible(),
      projects_button_bounds, views::SizeBounds(projects_button_pref_size));

  layout.host_size = host_size;

  return layout;
}

bool ProjectsPanelControlsView::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (projects_button_ && IsHitInView(projects_button_, point)) {
    return false;
  }

  return true;
}

BEGIN_METADATA(ProjectsPanelControlsView)
END_METADATA
