// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TEST_ACCESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TEST_ACCESSOR_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/actions/action_id.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BrowserWindowInterface;
class PinnedActionToolbarButton;
class PinnedToolbarActions;
class PinnedToolbarActionsContainer;

namespace actions {
class ActionItem;
}

namespace ui {
class TrackedElement;
}

// Test accessor providing a uniform interface for inspecting and interacting
// with pinned toolbar action buttons across both Views and WebUI toolbar
// implementations.
class PinnedActionTestAccessor {
 public:
  PinnedActionTestAccessor(BrowserWindowInterface* browser,
                           actions::ActionId action_id);
  PinnedActionTestAccessor(const PinnedActionTestAccessor&) = default;
  PinnedActionTestAccessor& operator=(const PinnedActionTestAccessor&) =
      default;
  ~PinnedActionTestAccessor();

  // Returns true if the action is currently pinned to the toolbar.
  bool IsPinned() const;

  // Returns true if the action is currently popped out in the toolbar.
  bool IsPoppedOut() const;

  // Returns true if the action is either pinned or popped out.
  bool IsPinnedOrPoppedOut() const;

  // Returns true if the action button is visible on screen.
  bool GetVisible() const;

  // Returns true if the action button is enabled.
  bool GetEnabled() const;

  // Returns true if the action button or indicator is active/underlined.
  bool IsActivated() const;

  // Returns true if the action button is highlighted (e.g. ink drop activated
  // in Views, or marked popped-out/highlighted in WebUI).
  bool IsHighlighted() const;

  // Returns the icon image model for the action item.
  ui::ImageModel GetImageModel() const;

  // Returns the tooltip text for the action button.
  std::u16string GetTooltipText() const;

  // Returns the accessible name for the action button.
  std::u16string GetAccessibleName() const;

  // Returns the BubbleAnchor for the action.
  views::BubbleAnchor GetBubbleAnchor() const;

  // Returns the tracked element for this action button, if one exists.
  ui::TrackedElement* GetElement() const;

  // Triggers a click on the action button.
  void Click() const;

  // Returns the ordered list of action IDs currently in the toolbar container
  // (pinned actions followed by popped-out actions).
  static std::vector<actions::ActionId> GetActionIds(
      BrowserWindowInterface* browser);

  // Returns true if the toolbar divider is currently visible.
  static bool IsDividerVisible(BrowserWindowInterface* browser);

  // Synchronously waits for any ongoing toolbar layout animations to finish.
  static void WaitForAnimation(BrowserWindowInterface* browser);

  // Reduces layout animation duration to 0/minimal in tests.
  static void ReduceAnimationDuration(BrowserWindowInterface* browser);

 private:
  static PinnedToolbarActions* GetPinnedToolbarActions(
      BrowserWindowInterface* browser);
  static PinnedToolbarActionsContainer* GetViewsContainer(
      BrowserWindowInterface* browser);
  PinnedToolbarActions* GetPinnedToolbarActions() const;
  actions::ActionItem* GetActionItem() const;
  PinnedToolbarActionsContainer* GetViewsContainer() const;
  PinnedActionToolbarButton* GetViewsButton() const;

  raw_ptr<BrowserWindowInterface> browser_;
  actions::ActionId action_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TEST_ACCESSOR_H_
