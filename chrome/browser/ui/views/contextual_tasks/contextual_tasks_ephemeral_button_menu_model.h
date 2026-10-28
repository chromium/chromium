// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_MENU_MODEL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

class BrowserWindowInterface;
class PinnedActionToolbarButtonMenuModel;

// Context menu of the contextual tasks ephemeral toolbar button, shared by the
// Views (`ContextualTasksButton`) and WebUI toolbar implementations. It has an
// item to remove the button for the current task (or to close the side panel).
// When the profile is eligible for pinning, it is followed by the items to pin
// the button to, or unpin it from, the toolbar and to customize the toolbar.
class ContextualTasksEphemeralButtonMenuModel
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRemoveForTaskMenuItem);

  explicit ContextualTasksEphemeralButtonMenuModel(
      BrowserWindowInterface* browser_window_interface);
  ContextualTasksEphemeralButtonMenuModel(
      const ContextualTasksEphemeralButtonMenuModel&) = delete;
  ContextualTasksEphemeralButtonMenuModel& operator=(
      const ContextualTasksEphemeralButtonMenuModel&) = delete;
  ~ContextualTasksEphemeralButtonMenuModel() override;

  // Rebuilds the items based on whether the side panel is closed, the pin
  // button is not pinned, and whether the profile is eligible for pinning.
  // Called before the menu is shown and when the eligibility changes.
  void UpdateItems();

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

 private:
  bool ShouldShowCloseSidePanel() const;
  bool IsPinningEligible() const;

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<PinnedActionToolbarButtonMenuModel> pinned_action_menu_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_MENU_MODEL_H_
