// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/browser/notification_service.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkNode;
using content::PageNavigator;

namespace {

base::LazyInstance<base::OnceClosure>::Leaky pre_run_callback =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if |command_id| corresponds to a command that causes one or more
// bookmarks to be removed.
bool IsRemoveBookmarksCommand(int command_id) {
  return command_id == IDC_CUT || command_id == IDC_BOOKMARK_BAR_REMOVE;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BookmarkContextMenu, public:

BookmarkContextMenu::BookmarkContextMenu(
    views::Widget* parent_widget,
    Browser* browser,
    Profile* profile,
    PageNavigator* page_navigator,
    BookmarkLaunchLocation opened_from,
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection,
    bool close_on_remove)
    : controller_(new BookmarkContextMenuController(
          parent_widget ? parent_widget->GetNativeWindow() : nullptr,
          this,
          browser,
          profile,
          page_navigator,
          opened_from,
          parent,
          selection)),
      parent_widget_(parent_widget),
      menu_(new views::MenuItemView(this)),
      menu_runner_(new views::MenuRunner(menu_,
                                         views::MenuRunner::HAS_MNEMONICS |
                                             views::MenuRunner::IS_NESTED |
                                             views::MenuRunner::CONTEXT_MENU)),
      observer_(NULL),
      close_on_remove_(close_on_remove) {
  ui::SimpleMenuModel* menu_model = controller_->menu_model();
  for (int i = 0; i < menu_model->GetItemCount(); ++i) {
    views::MenuModelAdapter::AppendMenuItemFromModel(
        menu_model, i, menu_, menu_model->GetCommandIdAt(i));
  }
}

BookmarkContextMenu::~BookmarkContextMenu() {
}

void BookmarkContextMenu::InstallPreRunCallback(base::OnceClosure callback) {
  DCHECK(pre_run_callback.Get().is_null());
  pre_run_callback.Get() = std::move(callback);
}

void BookmarkContextMenu::RunMenuAt(const gfx::Point& point,
                                    ui::MenuSourceType source_type) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return;

  if (!pre_run_callback.Get().is_null())
    std::move(pre_run_callback.Get()).Run();

  // width/height don't matter here.
  menu_runner_->RunMenuAt(parent_widget_, nullptr,
                          gfx::Rect(point.x(), point.y(), 0, 0),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void BookmarkContextMenu::SetPageNavigator(PageNavigator* navigator) {
  controller_->set_navigator(navigator);
}

////////////////////////////////////////////////////////////////////////////////
// BookmarkContextMenu, views::MenuDelegate implementation:

void BookmarkContextMenu::ExecuteCommand(int command_id, int event_flags) {
  controller_->ExecuteCommand(command_id, event_flags);
}

bool BookmarkContextMenu::IsItemChecked(int command_id) const {
  return controller_->IsCommandIdChecked(command_id);
}

bool BookmarkContextMenu::IsCommandEnabled(int command_id) const {
  return controller_->IsCommandIdEnabled(command_id);
}

bool BookmarkContextMenu::IsCommandVisible(int command_id) const {
  return controller_->IsCommandIdVisible(command_id);
}

bool BookmarkContextMenu::ShouldCloseAllMenusOnExecute(int id) {
  return (id != IDC_BOOKMARK_BAR_REMOVE) || close_on_remove_;
}

void BookmarkContextMenu::OnMenuClosed(views::MenuItemView* menu) {
  if (observer_)
    observer_->OnContextMenuClosed();
}

////////////////////////////////////////////////////////////////////////////////
// BookmarkContextMenuControllerDelegate
// implementation:

void BookmarkContextMenu::CloseMenu() {
  menu_->Cancel();
}

void BookmarkContextMenu::WillExecuteCommand(
    int command_id,
    const std::vector<const BookmarkNode*>& bookmarks) {
  if (observer_ && IsRemoveBookmarksCommand(command_id))
    observer_->WillRemoveBookmarks(bookmarks);
}

void BookmarkContextMenu::DidExecuteCommand(int command_id) {
  if (observer_ && IsRemoveBookmarksCommand(command_id))
    observer_->DidRemoveBookmarks();
}
