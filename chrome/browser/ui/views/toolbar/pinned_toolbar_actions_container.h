// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

class Browser;
class BrowserView;

// Container for pinned actions shown in the toolbar.
// TODO(b/303064829): Handle pop-out behavior including separator in this class
// as well.
// TODO(b/299463183): Handle highlighting of pinned/popped-out buttons.
class PinnedToolbarActionsContainer
    : public ToolbarIconContainerView,
      public PinnedToolbarActionsModel::Observer {
 public:
  class PinnedActionToolbarButton : public ToolbarButton {
   public:
    PinnedActionToolbarButton(Browser* browser, actions::ActionId action_id);
    ~PinnedActionToolbarButton() override;

    actions::ActionId GetActionId();

    void ButtonPressed();

    bool IsActive();

    void AddHighlight();
    void ResetHighlight();

   private:
    void ActionItemChanged();

    raw_ptr<actions::ActionItem> action_item_ = nullptr;
    base::CallbackListSubscription action_changed_subscription_;
    // Used to ensure the button remains highlighted while active.
    absl::optional<Button::ScopedAnchorHighlight> anchor_higlight_;
  };

  explicit PinnedToolbarActionsContainer(BrowserView* browser_view);
  PinnedToolbarActionsContainer(const PinnedToolbarActionsContainer&) = delete;
  PinnedToolbarActionsContainer& operator=(
      const PinnedToolbarActionsContainer&) = delete;
  ~PinnedToolbarActionsContainer() override;

  void UpdateActionState(actions::ActionId id, bool is_active);

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

  // PinnedToolbarActionsModel::Observer:
  void OnActionAdded(const actions::ActionId& id) override;
  void OnActionRemoved(const actions::ActionId& id) override;
  void OnActionMoved(const actions::ActionId& id,
                     int from_index,
                     int to_index) override;
  void OnActionsChanged() override {}

 private:
  friend class PinnedSidePanelInteractiveTest;
  friend class PinnedToolbarActionsContainerTest;

  void CreatePinnedActionButtons();
  actions::ActionItem* GetActionItemFor(const actions::ActionId& id);
  PinnedActionToolbarButton* AddPopOutButtonFor(const actions::ActionId& id);
  void RemovePoppedOutButtonFor(const actions::ActionId& id);
  void AddPinnedActionButtonFor(const actions::ActionId& id);
  void RemovePinnedActionButtonFor(const actions::ActionId& id);
  PinnedActionToolbarButton* GetPinnedButtonFor(const actions::ActionId& id);
  PinnedActionToolbarButton* GetPoppedOutButtonFor(const actions::ActionId& id);
  void ReorderViews();
  SidePanelCoordinator* GetSidePanelCoordinator();

  const raw_ptr<BrowserView> browser_view_;

  std::vector<PinnedActionToolbarButton*> pinned_buttons_;
  std::vector<PinnedActionToolbarButton*> popped_out_buttons_;
  raw_ptr<PinnedToolbarActionsModel> model_;

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_H_
