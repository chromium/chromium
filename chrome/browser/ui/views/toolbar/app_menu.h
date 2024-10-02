// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_H_

#include <map>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/global_error/global_error_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_delegate.h"

class BookmarkMenuDelegate;
class Browser;

namespace views {
class MenuButtonController;
class MenuItemView;
class MenuRunner;
}

// AppMenu adapts the AppMenuModel to view's menu related classes.
class AppMenu final : public views::MenuDelegate,
                      public bookmarks::BaseBookmarkModelObserver,
                      public GlobalErrorObserver {
 public:
  AppMenu(Browser* browser, ui::MenuModel* model, int run_types);
  AppMenu(const AppMenu&) = delete;
  AppMenu& operator=(const AppMenu&) = delete;
  ~AppMenu() override;

  // Shows the menu relative to the specified controller's button.
  void RunMenu(views::MenuButtonController* host);

  // Closes the menu if it is open, otherwise does nothing.
  void CloseMenu();

  // Whether the menu is currently visible to the user.
  bool IsShowing() const;

  bool for_drop() const {
    return (run_types_ & views::MenuRunner::FOR_DROP) != 0;
  }

  views::MenuItemView* root_menu_item() { return root_; }

  base::WeakPtr<AppMenu> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  void SetTimerForTesting(base::ElapsedTimer timer);

  // views::MenuDelegate:
  const gfx::FontList* GetLabelFontList(int command_id) const override;
  std::optional<SkColor> GetLabelColor(int command_id) const override;
  std::u16string GetTooltipText(int command_id,
                                const gfx::Point& p) const override;
  bool IsTriggerableEvent(views::MenuItemView* menu,
                          const ui::Event& e) override;
  bool GetDropFormats(views::MenuItemView* menu,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired(views::MenuItemView* menu) override;
  bool CanDrop(views::MenuItemView* menu,
               const ui::OSExchangeData& data) override;
  ui::mojom::DragOperation GetDropOperation(views::MenuItemView* item,
                                            const ui::DropTargetEvent& event,
                                            DropPosition* position) override;
  views::View::DropCallback GetDropCallback(
      views::MenuItemView* menu,
      DropPosition position,
      const ui::DropTargetEvent& event) override;
  bool ShowContextMenu(views::MenuItemView* source,
                       int command_id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  bool CanDrag(views::MenuItemView* menu) override;
  void WriteDragData(views::MenuItemView* sender,
                     ui::OSExchangeData* data) override;
  int GetDragOperations(views::MenuItemView* sender) override;
  int GetMaxWidthForMenu(views::MenuItemView* menu) override;
  bool IsItemChecked(int command_id) const override;
  bool IsCommandEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int mouse_event_flags) override;
  bool GetAccelerator(int command_id,
                      ui::Accelerator* accelerator) const override;
  void WillShowMenu(views::MenuItemView* menu) override;
  void WillHideMenu(views::MenuItemView* menu) override;
  bool ShouldCloseOnDragComplete() override;
  void OnMenuClosed(views::MenuItemView* menu) override;
  bool ShouldExecuteCommandWithoutClosingMenu(int command_id,
                                              const ui::Event& event) override;

  // bookmarks::BaseBookmarkModelObserver overrides:
  void BookmarkModelChanged() override;

  // GlobalErrorObserver:
  void OnGlobalErrorsChanged() override;

  views::View* GetZoomAppMenuViewForTest();

 private:
  class CutCopyPasteView;
  class RecentTabsMenuModelDelegate;
  class ZoomView;

  typedef std::pair<ui::MenuModel*, size_t> Entry;
  typedef std::map<int,Entry> CommandIDToEntry;

  // Populates |parent| with all the child menus in |model|. Recursively invokes
  // |PopulateMenu| for any submenu.
  void PopulateMenu(views::MenuItemView* parent,
                    ui::MenuModel* model);

  // Adds a new menu item to |parent| at |menu_index| to represent the item in
  // |model| at |model_index|:
  // - |menu_index|: position in |parent| to add the new item.
  // - |model_index|: position in |model| to retrieve information about the
  //   new menu item.
  // The returned item's MenuItemView::GetCommand() is the same as that of
  // |model|->GetCommandIdAt(|model_index|).
  views::MenuItemView* AddMenuItem(views::MenuItemView* parent,
                                   size_t menu_index,
                                   ui::MenuModel* model,
                                   size_t model_index,
                                   ui::MenuModel::ItemType menu_type);

  // Invoked from the cut/copy/paste menus. Cancels the current active menu and
  // activates the menu item in |model| at |index|.
  void CancelAndEvaluate(ui::ButtonMenuItemModel* model, size_t index);

  // Creates the bookmark menu if necessary. Does nothing if already created or
  // the bookmark model isn't loaded.
  void CreateBookmarkMenu();

  // Returns the index of the MenuModel/index pair representing the |command_id|
  // in |command_id_to_entry_|.
  size_t ModelIndexFromCommandId(int command_id) const;

  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The views menu. Owned by `menu_runner_`.
  raw_ptr<views::MenuItemView> root_ = nullptr;

  // Maps from the command ID in model to the model/index pair the item came
  // from.
  CommandIDToEntry command_id_to_entry_;

  // Browser the menu is being shown for.
  const raw_ptr<Browser, DanglingUntriaged> browser_;

  const raw_ptr<ui::MenuModel> model_;

  // |CancelAndEvaluate| sets |selected_menu_model_| and |selected_index_|.
  // If |selected_menu_model_| is non-null after the menu completes
  // ActivatedAt is invoked. This is done so that ActivatedAt isn't invoked
  // while the message loop is nested.
  raw_ptr<ui::ButtonMenuItemModel, DanglingUntriaged> selected_menu_model_ =
      nullptr;
  size_t selected_index_ = 0;

  std::vector<base::CallbackListSubscription>
      profile_menu_item_selected_subscription_list_;

  // Used for managing the bookmark menu items.
  std::unique_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  // Menu corresponding to IDC_BOOKMARKS_MENU.
  raw_ptr<views::MenuItemView, DanglingUntriaged> bookmark_menu_ = nullptr;

  // Used for managing the tab group menu items.
  std::unique_ptr<tab_groups::STGEverythingMenu> stg_everything_menu_;

  // Menu corresponding to IDC_SAVED_TAB_GROUPS_MENU.
  raw_ptr<views::MenuItemView> saved_tab_groups_menu_ = nullptr;

  // Menu corresponding to IDC_FEEDBACK.
  raw_ptr<views::MenuItemView, DanglingUntriaged> feedback_menu_item_ = nullptr;

  // Menu corresponding to IDC_TAKE_SCREENSHOT.
  raw_ptr<views::MenuItemView, DanglingUntriaged> screenshot_menu_item_ =
      nullptr;

  // Used for managing "Recent tabs" menu items.
  std::unique_ptr<RecentTabsMenuModelDelegate> recent_tabs_menu_model_delegate_;

  base::ScopedObservation<GlobalErrorService, GlobalErrorObserver>
      global_error_observation_{this};

  // The bit mask of views::MenuRunner::RunTypes.
  const int run_types_;

  // Records the time from when menu opens to when the user selects a menu item.
  base::ElapsedTimer menu_opened_timer_;

  base::WeakPtrFactory<AppMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_H_
