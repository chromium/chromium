// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/drag_controller.h"

class Browser;
class BrowserView;

// Container for pinned actions shown in the toolbar.
// TODO(crbug.com/1514477): Re-enable animation after the race condition issue
// is addressed.
class PinnedToolbarActionsContainer
    : public views::View,
      public PinnedToolbarActionsModel::Observer,
      public views::DragController,
      public ToolbarController::PinnedActionsDelegate {
  METADATA_HEADER(PinnedToolbarActionsContainer, views::View)

 public:
  class PinnedActionToolbarButton : public ToolbarButton,
                                    public ui::SimpleMenuModel::Delegate {
    METADATA_HEADER(PinnedActionToolbarButton, ToolbarButton)

   public:
    PinnedActionToolbarButton(Browser* browser,
                              actions::ActionId action_id,
                              PinnedToolbarActionsContainer* container);
    ~PinnedActionToolbarButton() override;

    actions::ActionId GetActionId();

    void ButtonPressed();
    void AddHighlight();
    void ResetHighlight();
    void SetIconVisibility(bool visible);
    void SetPinned(bool pinned);

    bool IsActive();
    bool IsInvokingAction();

    // Button:
    gfx::Size CalculatePreferredSize() const override;
    void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

    void UpdatePinnedStateForContextMenu();

    // ui::SimpleMenuModel::Delegate:
    bool IsItemForCommandIdDynamic(int command_id) const override;
    std::u16string GetLabelForCommandId(int command_id) const override;
    void ExecuteCommand(int command_id, int event_flags) override;
    bool IsCommandIdEnabled(int command_id) const override;

   private:
    void ActionItemChanged();
    std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

    raw_ptr<Browser> browser_;
    raw_ptr<actions::ActionItem> action_item_ = nullptr;
    base::CallbackListSubscription action_changed_subscription_;
    // Used to ensure the button remains highlighted while active.
    absl::optional<Button::ScopedAnchorHighlight> anchor_higlight_;
    bool pinned_ = false;
    bool invoking_action_ = false;
    raw_ptr<PinnedToolbarActionsContainer> container_;
  };

  explicit PinnedToolbarActionsContainer(BrowserView* browser_view);
  PinnedToolbarActionsContainer(const PinnedToolbarActionsContainer&) = delete;
  PinnedToolbarActionsContainer& operator=(
      const PinnedToolbarActionsContainer&) = delete;
  ~PinnedToolbarActionsContainer() override;

  void UpdateActionState(actions::ActionId id, bool is_active);
  void UpdateDividerFlexSpecification();

  void UpdateAllIcons();

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
  void OnActionAdded(const actions::ActionId& id) override;
  void OnActionRemoved(const actions::ActionId& id) override;
  void OnActionMoved(const actions::ActionId& id,
                     int from_index,
                     int to_index) override;
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

  bool IsActionPinned(const actions::ActionId& id);

 private:
  friend class PinnedSidePanelInteractiveTest;
  friend class PinnedToolbarActionsContainerTest;

  // A struct representing the position and action being dragged.
  struct DropInfo;

  PinnedActionToolbarButton* AddPopOutButtonFor(const actions::ActionId& id);
  void RemovePoppedOutButtonFor(const actions::ActionId& id);
  void AddPinnedActionButtonFor(const actions::ActionId& id);
  void RemovePinnedActionButtonFor(const actions::ActionId& id);
  PinnedActionToolbarButton* GetPinnedButtonFor(const actions::ActionId& id);
  PinnedActionToolbarButton* GetPoppedOutButtonFor(const actions::ActionId& id);

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

  std::vector<PinnedActionToolbarButton*> pinned_buttons_;
  std::vector<PinnedActionToolbarButton*> popped_out_buttons_;
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
