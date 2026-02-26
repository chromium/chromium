// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"

namespace actions {
class ActionItem;
}  // namespace actions

namespace ui {
class OSExchangeData;
}

namespace views {
class ActionViewController;
class Label;
class LabelButton;
}  // namespace views

class ProjectsPanelNoTabGroupsView;
class ProjectsPanelTabGroupsItemView;

// Contains the Tab Groups inside the Projects Panel.
class ProjectsPanelTabGroupsView : public views::View,
                                   public views::DragController {
  METADATA_HEADER(ProjectsPanelTabGroupsView, views::View)

 public:
  using TabGroupMovedCallback =
      base::RepeatingCallback<void(const base::Uuid&, int)>;

  ProjectsPanelTabGroupsView(
      actions::ActionItem* root_action_item,
      views::ActionViewController* action_view_controller,
      ProjectsPanelTabGroupsItemView::TabGroupPressedCallback
          tab_group_button_callback = base::DoNothing(),
      ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback
          more_button_callback = base::DoNothing(),
      TabGroupMovedCallback tab_group_moved_callback = base::DoNothing(),
      base::RepeatingClosure create_new_tab_group_callback = base::DoNothing());
  ProjectsPanelTabGroupsView(const ProjectsPanelTabGroupsView&) = delete;
  ProjectsPanelTabGroupsView& operator=(const ProjectsPanelTabGroupsView&) =
      delete;
  ~ProjectsPanelTabGroupsView() override;

  // Sets the tab groups shown in the list.
  void SetTabGroups(const std::vector<tab_groups::SavedTabGroup>& tab_groups);

  // views::View:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;
  void PaintChildren(const views::PaintInfo& paint_info) override;

  // views::DragController:
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override;
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  std::optional<gfx::Rect> GetDropIndicatorBoundsForTesting() const;

  const std::vector<ProjectsPanelTabGroupsItemView*> item_views_for_testing() {
    return item_views_;
  }

  ProjectsPanelNoTabGroupsView* no_tab_groups_view_for_testing() {
    return no_tab_groups_view_;
  }

  views::LabelButton* create_new_tab_group_button_for_testing() {
    return create_new_tab_group_button_;
  }

  static void disable_animations_for_testing();

 private:
  // Determines where the drop will occur.
  struct DropLocation {
    bool Equals(const DropLocation& other) const {
      return index == other.index && operation == other.operation;
    }

    // The index the group should move to.
    std::optional<size_t> index;

    ui::mojom::DragOperation operation = ui::mojom::DragOperation::kNone;
  };

  struct DropInfo {
    // Coordinates of the drag (in terms of this view).
    int x = 0;
    int y = 0;

    DropLocation location;
  };

  void OnDrop(size_t drop_index,
              const ui::DropTargetEvent& event,
              ui::mojom::DragOperation& output_drag_op,
              std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Calculates the location for the drop in `location`.
  void CalculateDropLocation(const ui::DropTargetEvent& event,
                             DropLocation* location);

  // Returns the bounds of the drop indicator, if one should be shown.
  std::optional<gfx::Rect> GetDropIndicatorBounds() const;

  ProjectsPanelTabGroupsItemView::TabGroupPressedCallback
      tab_group_button_callback_;
  ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback
      more_button_callback_;
  TabGroupMovedCallback tab_group_moved_callback_;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::LabelButton> create_new_tab_group_button_ = nullptr;
  raw_ptr<ProjectsPanelNoTabGroupsView> no_tab_groups_view_ = nullptr;
  std::vector<ProjectsPanelTabGroupsItemView*> item_views_;

  // Used to track drops on the view.
  std::unique_ptr<DropInfo> drop_info_;

  base::WeakPtrFactory<ProjectsPanelTabGroupsView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
