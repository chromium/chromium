// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PINNED_ACTION_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PINNED_ACTION_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "ui/actions/action_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

class BrowserWindowInterface;
class PinnedToolbarActionsContainer;

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class ActionViewInterface;
}  // namespace views

// The Contextual Tasks toolbar button created by the pinned toolbar actions
// container under the ephemeral pin button refactor. While unpinned, it is the
// ephemeral Contextual Tasks button, so its context menu also records the
// Contextual Tasks ephemeral button metrics recorded by the legacy
// `ContextualTasksEphemeralButtonMenuModel`, and its presses are not counted as
// pinned toolbar button activations. Once pinned, it behaves like any other
// `PinnedActionToolbarButton`.
class ContextualTasksPinnedActionToolbarButton
    : public PinnedActionToolbarButton {
  METADATA_HEADER(ContextualTasksPinnedActionToolbarButton,
                  PinnedActionToolbarButton)

 public:
  ContextualTasksPinnedActionToolbarButton(
      BrowserWindowInterface* browser,
      actions::ActionId action_id,
      base::WeakPtr<PinnedToolbarActionsContainer> container);
  ContextualTasksPinnedActionToolbarButton(
      const ContextualTasksPinnedActionToolbarButton&) = delete;
  ContextualTasksPinnedActionToolbarButton& operator=(
      const ContextualTasksPinnedActionToolbarButton&) = delete;
  ~ContextualTasksPinnedActionToolbarButton() override;

  // Creates the button. Matches `CreateCustomPinnedActionToolbarButtonCallback`
  // so it can be set as the action's
  // `kCustomPinnedActionToolbarButtonFactoryKey`.
  static std::unique_ptr<PinnedActionToolbarButton> Create(
      BrowserWindowInterface* browser,
      actions::ActionId action_id,
      base::WeakPtr<PinnedToolbarActionsContainer> container);

  // PinnedActionToolbarButton:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

 protected:
  // ToolbarButton:
  void ShowDropDownMenu(ui::mojom::MenuSourceType source_type) override;

 private:
  // Forwards to the button's `PinnedActionToolbarButtonMenuModel` and records
  // the ephemeral button context menu metrics. Shown while unpinned.
  std::unique_ptr<ui::MenuModel> ephemeral_menu_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PINNED_ACTION_TOOLBAR_BUTTON_H_
