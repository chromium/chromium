// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

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
                           public TabStripModelObserver,
                           public ToolbarActionsModel::Observer {
 public:
  METADATA_HEADER(ExtensionsMenuView);
  ExtensionsMenuView(views::View* anchor_view,
                     Browser* browser,
                     ExtensionsContainer* extensions_container,
                     bool allow_pinning);
  ExtensionsMenuView(const ExtensionsMenuView&) = delete;
  ExtensionsMenuView& operator=(const ExtensionsMenuView&) = delete;
  ~ExtensionsMenuView() override;

  // Displays the ExtensionsMenu under |anchor_view|, attached to |browser|, and
  // with the associated |extensions_container|.
  // Only one menu is allowed to be shown at a time (outside of tests).
  static views::Widget* ShowBubble(views::View* anchor_view,
                                   Browser* browser,
                                   ExtensionsContainer* extensions_container,
                                   bool allow_pinning);

  // Returns true if there is currently an ExtensionsMenuView showing (across
  // all browsers and profiles).
  static bool IsShowing();

  // Hides the currently-showing ExtensionsMenuView, if any exists.
  static void Hide();

  // Returns the currently-showing ExtensionsMenuView, if any exists.
  static ExtensionsMenuView* GetExtensionsMenuViewForTesting();

  // Returns the children of a section for the given `status`.
  static std::vector<ExtensionsMenuItemView*>
  GetSortedItemsForSectionForTesting(
      ToolbarActionViewController::PageInteractionStatus status);

  // WidgetDelegate:
  std::u16string GetAccessibleWindowTitle() const override;

  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

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
  // A "section" within the menu, based on the extension's current access to
  // the page.
  struct Section {
    // The root view for this section; this is used to toggle the visibility of
    // the entire section (depending on whether there are any menu items).
    views::View* container;

    // The view containing only the extension menu items for this section. This
    // is separated for easy sorting, insertion, and iteration of menu items.
    // The children are guaranteed to only be ExtensionMenuItemViews.
    views::View* menu_items;

    // The id of the string to use for the section heading.
    const int header_string_id;

    // The id of the string to use for the longer description of the section.
    const int description_string_id;

    // The PageInteractionStatus that this section is handling.
    const ToolbarActionViewController::PageInteractionStatus page_status;
  };

  // Initially populates the menu by creating sections with menu items for all
  // extensions.
  void Populate();

  std::unique_ptr<views::View> CreateExtensionButtonsContainer();

  // Returns the appropriate section for the given |status|.
  Section* GetSectionForStatus(
      ToolbarActionViewController::PageInteractionStatus status);

  // Updates the states of all actions in the menu.
  void UpdateActionStates();

  // Sorts the views within all sections by the name of the action.
  void SortMenuItemsByName();

  // Inserts the menu item into the appropriate section (but not necessarily at
  // the right spot).
  void InsertMenuItem(ExtensionsMenuItemView* menu_item);

  // Adds a menu item for a newly-added extension.
  void CreateAndInsertNewItem(const ToolbarActionsModel::ActionId& id);

  // Updates the visibility of the menu sections. A given section should be
  // visible if there are any extensions displayed in it.
  void UpdateSectionVisibility();

  // Updates the menu.
  void Update();

  // Runs a set of sanity checks on the appearance of the menu. This is a no-op
  // if DCHECKs are disabled.
  void SanityCheck();

  Browser* const browser_;
  ExtensionsContainer* const extensions_container_;
  bool allow_pinning_;
  ToolbarActionsModel* const toolbar_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};
  std::vector<ExtensionsMenuItemView*> extensions_menu_items_;

  views::Button* manage_extensions_button_for_testing_ = nullptr;

  // The different sections in the menu.
  Section cant_access_;
  Section wants_access_;
  Section has_access_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
