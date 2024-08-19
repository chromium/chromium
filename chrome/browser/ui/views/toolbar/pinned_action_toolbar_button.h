// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_H_

#include <memory>
#include <optional>
#include <string>
#include <type_traits>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container_layout.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

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
  void SetIsActionShowingBubble(bool showing_bubble) {
    is_action_showing_bubble_ = showing_bubble;
  }
  void SetShouldShowEphemerallyInToolbar(bool should_show_in_toolbar) {
    should_show_in_toolbar_ = should_show_in_toolbar;
  }
  void SetActionEngaged(bool action_engaged);
  void UpdateIcon() override;
  bool ShouldShowEphemerallyInToolbar();
  bool IsIconVisible() { return is_icon_visible_; }
  bool IsPinned() { return pinned_; }

  bool ShouldSkipExecutionForTesting() { return skip_execution_; }

  using views::LabelButton::image_container_view;
  // View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // ToolbarButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;
  void Layout(PassKey) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

  void UpdatePinnedStateForContextMenu();
  void UpdateStatusIndicator();
  void HideStatusIndicator();
  PinnedToolbarButtonStatusIndicator* GetStatusIndicatorForTesting() {
    return status_indicator_;
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  void OnAnchorCountChanged(size_t anchor_count);

  raw_ptr<Browser> browser_;
  raw_ptr<PinnedToolbarButtonStatusIndicator> status_indicator_;

  actions::ActionId action_id_;
  base::CallbackListSubscription action_changed_subscription_;
  base::CallbackListSubscription action_count_changed_subscription_;
  // Used to ensure the button remains highlighted while active.
  std::optional<Button::ScopedAnchorHighlight> anchor_higlight_;
  bool pinned_ = false;
  bool needs_delayed_destruction_ = false;
  bool is_pinnable_ = false;
  bool is_icon_visible_ = true;
  bool action_engaged_ = false;
  // Set when the action is currently showing an associated bubble.
  bool is_action_showing_bubble_ = false;
  bool skip_execution_ = false;

  // Set when something is currently anchored to the button (bubble dialog,
  // context menu, etc.)
  bool has_anchor_ = false;

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

extern const ui::ClassProperty<
    std::underlying_type_t<PinnedToolbarActionFlexPriority>>* const
    kToolbarButtonFlexPriorityKey;

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_H_
