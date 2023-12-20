// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"

#include <memory>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkNode;

namespace {

base::OnceClosure& PreRunCallback() {
  static base::NoDestructor<base::OnceClosure> instance;
  return *instance;
}

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
    BookmarkLaunchLocation opened_from,
    const BookmarkNode* parent,
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection,
    bool close_on_remove)
    : controller_(new BookmarkContextMenuController(
          parent_widget ? parent_widget->GetNativeWindow() : nullptr,
          this,
          browser,
          profile,
          opened_from,
          parent,
          selection)),
      parent_widget_(parent_widget),
      menu_(new views::MenuItemView(this)),
      close_on_remove_(close_on_remove) {
  menu_runner_ = std::make_unique<views::MenuRunner>(
      base::WrapUnique<views::MenuItemView>(menu_),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::IS_NESTED |
          views::MenuRunner::CONTEXT_MENU);
  ui::SimpleMenuModel* menu_model = controller_->menu_model();
  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    views::MenuModelAdapter::AppendMenuItemFromModel(
        menu_model, i, menu_, menu_model->GetCommandIdAt(i));
  }
}

BookmarkContextMenu::~BookmarkContextMenu() {}

void BookmarkContextMenu::InstallPreRunCallback(base::OnceClosure callback) {
  DCHECK(PreRunCallback().is_null());
  PreRunCallback() = std::move(callback);
}

void BookmarkContextMenu::RunMenuAt(const gfx::Point& point,
                                    ui::MenuSourceType source_type) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return;

  if (!PreRunCallback().is_null())
    std::move(PreRunCallback()).Run();

  // width/height don't matter here.
  menu_runner_->RunMenuAt(parent_widget_, nullptr,
                          gfx::Rect(point.x(), point.y(), 0, 0),
                          views::MenuAnchorPosition::kTopLeft, source_type);
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
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        bookmarks) {
  if (observer_ && IsRemoveBookmarksCommand(command_id))
    observer_->WillRemoveBookmarks(bookmarks);
}

void BookmarkContextMenu::DidExecuteCommand(int command_id) {
  if (observer_ && IsRemoveBookmarksCommand(command_id))
    observer_->DidRemoveBookmarks();
}
