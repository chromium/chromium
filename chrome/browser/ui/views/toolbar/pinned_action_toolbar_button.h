// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"

class Browser;
class PinnedToolbarActionsContainer;

class PinnedActionToolbarButton : public ToolbarButton,
                                  public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(PinnedActionToolbarButton, ToolbarButton)

 public:
  PinnedActionToolbarButton(Browser* browser,
                            actions::ActionId action_id,
                            PinnedToolbarActionsContainer* container);
  ~PinnedActionToolbarButton() override;

  actions::ActionId GetActionId() { return action_id_; }

  void AddHighlight();
  void ResetHighlight();
  void SetPinned(bool pinned);
  bool IsActive();
  base::AutoReset<bool> SetNeedsDelayedDestruction(
      bool needs_delayed_destruction);
  void SetIconVisibility(bool is_visible);
  bool NeedsDelayedDestruction() { return needs_delayed_destruction_; }
  void SetIsPinnable(bool is_pinnable) { is_pinnable_ = is_pinnable; }
  void SetShouldShowEphemerallyInToolbar(bool should_show_in_toolbar) {
    should_show_in_toolbar_ = should_show_in_toolbar;
  }
  bool ShouldShowEphemerallyInToolbar() { return should_show_in_toolbar_; }
  bool IsIconVisible() { return is_icon_visible_; }
  bool IsPinned() { return pinned_; }

  // View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // ToolbarButton:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

  void UpdatePinnedStateForContextMenu();

  // ui::SimpleMenuModel::Delegate:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  void OnAnchorCountChanged(size_t anchor_count);

  raw_ptr<Browser> browser_;
  actions::ActionId action_id_;
  base::CallbackListSubscription action_changed_subscription_;
  base::CallbackListSubscription action_count_changed_subscription_;
  // Used to ensure the button remains highlighted while active.
  std::optional<Button::ScopedAnchorHighlight> anchor_higlight_;
  bool pinned_ = false;
  bool needs_delayed_destruction_ = false;
  bool is_pinnable_ = false;
  bool is_icon_visible_ = true;
  // Set when a button should be shown in the toolbar regardless of whether it
  // is pinned or active. This is used in cases like when the recent download
  // button should be visible after a download.
  bool should_show_in_toolbar_ = false;
  raw_ptr<PinnedToolbarActionsContainer> container_;
};

class PinnedActionToolbarButtonActionViewInterface
    : public ToolbarButtonActionViewInterface {
 public:
  explicit PinnedActionToolbarButtonActionViewInterface(
      PinnedActionToolbarButton* action_view);
  ~PinnedActionToolbarButtonActionViewInterface() override = default;

  // ToolbarButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override;
  void InvokeActionImpl(actions::ActionItem* action_item) override;
  void OnViewChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<PinnedActionToolbarButton> action_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_H_
