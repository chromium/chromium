// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Button;
class View;
}  // namespace views

class Browser;
class ExtensionsContainer;
class ExtensionsMenuItemView;

// This bubble view displays a list of user extensions and a button to get to
// managing the user's extensions (chrome://extensions).
// This class is only used with the kExtensionsToolbarMenu feature.
class ExtensionsMenuView : public views::BubbleDialogDelegateView,
                           public ToolbarActionsModel::Observer {
 public:
  static constexpr gfx::Size kExtensionsMenuIconSize = gfx::Size(28, 28);

  ExtensionsMenuView(views::View* anchor_view,
                     Browser* browser,
                     ExtensionsContainer* extensions_container);
  ~ExtensionsMenuView() override;

  // Displays the ExtensionsMenu under |anchor_view|, attached to |browser|, and
  // with the associated |extensions_container|.
  // Only one menu is allowed to be shown at a time (outside of tests).
  static void ShowBubble(views::View* anchor_view,
                         Browser* browser,
                         ExtensionsContainer* extensions_container);

  // Returns true if there is currently an ExtensionsMenuView showing (across
  // all browsers and profiles).
  static bool IsShowing();

  // Hides the currently-showing ExtensionsMenuView, if any exists.
  static void Hide();

  // Returns the currently-showing ExtensionsMenuView, if any exists.
  static ExtensionsMenuView* GetExtensionsMenuViewForTesting();

  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  // TODO(crbug.com/1003072): This override is copied from PasswordItemsView to
  // contrain the width. It would be nice to have a unified way of getting the
  // preferred size to not duplicate the code.
  gfx::Size CalculatePreferredSize() const override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& item,
                            int index) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionMoved(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionLoadFailed() override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarVisibleCountChanged() override;
  void OnToolbarHighlightModeChanged(bool is_highlighting) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  std::vector<ExtensionsMenuItemView*> extensions_menu_items_for_testing() {
    return extensions_menu_items_;
  }
  views::Button* manage_extensions_button_for_testing() {
    return manage_extensions_button_for_testing_;
  }
  // Returns a scoped object allowing test dialogs to be created (i.e.,
  // instances of the ExtensionsMenuView that are not created through
  // ShowBubble()).
  // We don't just use ShowBubble() in tests because a) there can be more than
  // one instance of the menu, and b) the menu, when shown, is dismissed by
  // changes in focus, which isn't always desirable. Additionally, constructing
  // the view directly is more friendly to unit test setups.
  static base::AutoReset<bool> AllowInstancesForTesting();

 private:
  class ButtonListener : public views::ButtonListener {
   public:
    explicit ButtonListener(Browser* browser);

    // views::ButtonListener:
    void ButtonPressed(views::Button* sender, const ui::Event& event) override;

   private:
    Browser* const browser_;
  };

  void Repopulate();
  std::unique_ptr<views::View> CreateExtensionButtonsContainer();

  Browser* const browser_;
  ExtensionsContainer* const extensions_container_;
  ToolbarActionsModel* const model_;
  ScopedObserver<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observer_;
  ButtonListener button_listener_;
  std::vector<ExtensionsMenuItemView*> extensions_menu_items_;

  views::Button* manage_extensions_button_for_testing_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
