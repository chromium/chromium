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
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kRegionInteriorMargins = 8;
constexpr int kProjectPanelWidth = 240;
}  // namespace

ProjectsPanelView::ProjectsPanelView(actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetBackground(views::CreateSolidBackground(ui::kColorFrameActive));

  // The vertical tab strip contains ScrollViews that paint to a layer. This
  // view must also paint to a layer to ensure it overlays those components.
  SetPaintToLayer();

  projects_button_ = AddChildButtonFor(kActionToggleProjectsPanel);
  projects_button_->SetProperty(views::kElementIdentifierKey,
                                kProjectsPanelButtonElementId);

  SetVisible(false);

  SetPreferredSize(gfx::Size(kProjectPanelWidth, 0));

  SetProperty(views::kElementIdentifierKey, kProjectsPanelViewElementId);
}

ProjectsPanelView::~ProjectsPanelView() = default;

views::LabelButton* ProjectsPanelView::AddChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<TopContainerButton> container_button =
      std::make_unique<TopContainerButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  TopContainerButton* container_button_ptr =
      AddChildView(std::move(container_button));

  container_button_ptr->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  return container_button_ptr;
}

views::ProposedLayout ProjectsPanelView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  gfx::Size host_size = gfx::Size(
      size_bounds.width().is_bounded() ? size_bounds.width().value()
                                       : parent()->width(),
      GetLayoutConstant(VERTICAL_TAB_STRIP_TOP_BUTTON_CONTAINER_HEIGHT));

  CHECK(projects_button_);

  const gfx::Size projects_button_pref_size =
      projects_button_->GetPreferredSize();

  int current_x = host_size.width();
  int current_y = host_size.height();

  // Calculate bounds to right-align the button horizontally and center it
  // vertically within the available space.
  gfx::Rect projects_button_bounds(
      current_x - projects_button_pref_size.width() - kRegionInteriorMargins,
      current_y -
          (GetLayoutConstant(VERTICAL_TAB_STRIP_TOP_BUTTON_CONTAINER_HEIGHT) +
           projects_button_pref_size.height()) /
              2 +
          kRegionInteriorMargins,
      projects_button_pref_size.width(), projects_button_pref_size.height());
  layout.child_layouts.emplace_back(
      projects_button_.get(), projects_button_->GetVisible(),
      projects_button_bounds, views::SizeBounds(projects_button_pref_size));

  layout.host_size = host_size;

  return layout;
}

bool ProjectsPanelView::IsPositionInWindowCaption(const gfx::Point& point) {
  if (projects_button_ && IsHitInView(projects_button_, point)) {
    return false;
  }

  return true;
}

void ProjectsPanelView::OnProjectsPanelStateChanged(
    ProjectsPanelStateController* state_controller) {
  TooltipTextChanged();
  SetVisible(state_controller->IsProjectsPanelVisible());
}

BEGIN_METADATA(ProjectsPanelView)
END_METADATA
