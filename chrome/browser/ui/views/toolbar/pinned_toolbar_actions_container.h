// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/drag_controller.h"

class BrowserView;

namespace views {
class ActionViewController;
}

// Container for pinned and epheremeral actions shown in the toolbar.
// Pinned actions are tracked by `pinned_buttons_`. Ephemeral actions are
// tracked by `popped_out_buttons_`. Pinned actions are determined by listening
// to PinnedToolbarActionsModel. Ephemeral actions are determined by external
// callers via the methods UpdateActionState() and  UpdateEphemeralAction().
class PinnedToolbarActionsContainer
    : public ToolbarIconContainerView,
      public PinnedToolbarActionsModel::Observer,
      public views::DragController,
      public ToolbarController::PinnedActionsDelegate {
  METADATA_HEADER(PinnedToolbarActionsContainer, ToolbarIconContainerView)

 public:
  explicit PinnedToolbarActionsContainer(BrowserView* browser_view);
  PinnedToolbarActionsContainer(const PinnedToolbarActionsContainer&) = delete;
  PinnedToolbarActionsContainer& operator=(
      const PinnedToolbarActionsContainer&) = delete;
  ~PinnedToolbarActionsContainer() override;

  // TODO(https://crbug.com/363743077): This method is almost but not quite
  // identical to ShowActionEphemerallyInToolbar(). This doesn't make sense and
  // one should be removed.
  void UpdateActionState(actions::ActionId id, bool is_active);
  // Updates whether the button is shown ephemerally in the toolbar (in the
  // popped out region unless also pinned) regardless of whether it is active.
  void ShowActionEphemerallyInToolbar(actions::ActionId id, bool show);

  void MovePinnedActionBy(actions::ActionId action_id, int delta);

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

  // views::View:
  void OnThemeChanged() override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // PinnedToolbarActionsModel::Observer:
  void OnActionAddedLocally(const actions::ActionId& id) override;
  void OnActionRemovedLocally(const actions::ActionId& id) override;
  void OnActionMovedLocally(const actions::ActionId& id,
                            int from_index,
                            int to_index) override {}
  void OnActionsChanged() override;

  // views::DragController:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // ToolbarController::PinnedActionsDelegate:
  actions::ActionItem* GetActionItemFor(const actions::ActionId& id) override;
  bool IsOverflowed(const actions::ActionId& id) override;
  views::View* GetContainerView() override;
  bool ShouldAnyButtonsOverflow(gfx::Size available_size) const override;

  bool IsActionPinned(const actions::ActionId& id);
  bool IsActionPoppedOut(const actions::ActionId& id);
  bool IsActionPinnedOrPoppedOut(const actions::ActionId& id);
  PinnedActionToolbarButton* GetButtonFor(const actions::ActionId& id);

  // Removes the popped out button if it should no longer remain in the toolbar.
  void MaybeRemovePoppedOutButtonFor(const actions::ActionId& id);

 private:
  friend class PinnedSidePanelInteractiveTest;
  friend class PinnedToolbarActionsContainerTest;

  // A struct representing the position and action being dragged.
  struct DropInfo;

  PinnedActionToolbarButton* AddPoppedOutButtonFor(const actions::ActionId& id);
  void AddPinnedActionButtonFor(const actions::ActionId& id);
  void RemovePinnedActionButtonFor(const actions::ActionId& id);
  PinnedActionToolbarButton* GetPinnedButtonFor(const actions::ActionId& id);
  PinnedActionToolbarButton* GetPoppedOutButtonFor(const actions::ActionId& id);
  bool ShouldRemainPoppedOutInToolbar(PinnedActionToolbarButton* button);
  // Returns the size based on the layout manager's default flex specification.
  gfx::Size DefaultFlexRule(const views::SizeBounds& size_bounds);
  // Returns the total width of the `popped_out_buttons_` including margins
  // between them.
  int CalculatePoppedOutButtonsWidth();

  // Sorts child views to display them in the correct order.
  void ReorderViews();

  // Updates the container view to match the current state of the model.
  void UpdateViews();

  void RemoveButton(PinnedActionToolbarButton* button);
  void SetActionButtonIconVisibility(actions::ActionId id, bool visible);

  // Moves the dragged action `action_id`.
  void MovePinnedAction(
      const actions::ActionId& action_id,
      size_t index,
      base::ScopedClosureRunner cleanup,
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Performs clean up after dragging.
  void DragDropCleanup(const actions::ActionId& dragged_action_id);

  // Utility function for going from width to icon counts.
  size_t WidthToIconCount(int x_offset);

  const raw_ptr<BrowserView> browser_view_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::vector<raw_ptr<PinnedActionToolbarButton, VectorExperimental>>
      pinned_buttons_;
  std::vector<raw_ptr<PinnedActionToolbarButton, VectorExperimental>>
      popped_out_buttons_;
  raw_ptr<views::View> toolbar_divider_;
  raw_ptr<PinnedToolbarActionsModel> model_;

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      model_observation_{this};

  // The DropInfo for the current drag-and-drop operation, or a null pointer if
  // there is none.
  std::unique_ptr<DropInfo> drop_info_;

  base::WeakPtrFactory<PinnedToolbarActionsContainer> weak_ptr_factory_{this};

  base::WeakPtrFactory<PinnedToolbarActionsContainer> drop_weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_
