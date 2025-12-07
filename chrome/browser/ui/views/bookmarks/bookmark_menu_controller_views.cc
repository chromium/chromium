// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkNode;
using content::PageNavigator;
using views::MenuItemView;

BookmarkMenuController::BookmarkMenuController(
    Browser* browser,
    views::Widget* parent,
    const BookmarkParentFolder& folder,
    size_t start_child_index,
    bool for_drop)
    : menu_delegate_(std::make_unique<BookmarkMenuDelegate>(
          browser,
          parent,
          this,
          BookmarkLaunchLocation::kSubfolder)),
      folder_(folder),
      observer_(nullptr),
      for_drop_(for_drop),
      bookmark_bar_(nullptr) {
  menu_delegate_->SetActiveMenu(folder, start_child_index);

  int run_type = 0;
  if (for_drop) {
    run_type |= views::MenuRunner::FOR_DROP;
  }

  run_type |= views::MenuRunner::HAS_MNEMONICS;
  menu_runner_ = std::make_unique<views::MenuRunner>(
      base::WrapUnique<MenuItemView>(menu_delegate_->menu()), run_type);
}

void BookmarkMenuController::RunMenuAt(BookmarkBarView* bookmark_bar) {
  bookmark_bar_ = bookmark_bar;
  views::MenuButton* menu_button =
      bookmark_bar_->GetMenuButtonForFolder(folder_);
  DCHECK(menu_button);
  views::MenuAnchorPosition anchor;
  bookmark_bar_->GetAnchorPositionForButton(menu_button, &anchor);
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(menu_button, &screen_loc);
  gfx::Rect bounds(screen_loc.x(), screen_loc.y(), menu_button->width(),
                   menu_button->height());
  menu_delegate_->GetBookmarkMergedSurfaceService()->AddObserver(this);
  // We only delete ourself after the menu completes, so we can safely ignore
  // the return value.
  menu_runner_->RunMenuAt(menu_delegate_->parent(),
                          menu_button->button_controller(), bounds, anchor,
                          ui::mojom::MenuSourceType::kNone);
}

void BookmarkMenuController::Cancel() {
  menu_delegate_->menu()->Cancel();
}

MenuItemView* BookmarkMenuController::menu() const {
  return menu_delegate_->menu();
}

MenuItemView* BookmarkMenuController::context_menu() const {
  return menu_delegate_->context_menu();
}

std::u16string BookmarkMenuController::GetTooltipText(
    int id,
    const gfx::Point& p) const {
  return menu_delegate_->GetTooltipText(id, p);
}

bool BookmarkMenuController::IsTriggerableEvent(views::MenuItemView* menu,
                                                const ui::Event& e) {
  return menu_delegate_->IsTriggerableEvent(menu, e);
}

void BookmarkMenuController::ExecuteCommand(int id, int mouse_event_flags) {
  menu_delegate_->ExecuteCommand(id, mouse_event_flags);
}

bool BookmarkMenuController::ShouldExecuteCommandWithoutClosingMenu(
    int id,
    const ui::Event& e) {
  return menu_delegate_->ShouldExecuteCommandWithoutClosingMenu(id, e);
}

bool BookmarkMenuController::GetDropFormats(
    MenuItemView* menu,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return menu_delegate_->GetDropFormats(menu, formats, format_types);
}

bool BookmarkMenuController::AreDropTypesRequired(MenuItemView* menu) {
  return menu_delegate_->AreDropTypesRequired(menu);
}

bool BookmarkMenuController::CanDrop(MenuItemView* menu,
                                     const ui::OSExchangeData& data) {
  return menu_delegate_->CanDrop(menu, data);
}

ui::mojom::DragOperation BookmarkMenuController::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  return menu_delegate_->GetDropOperation(item, event, position);
}

views::View::DropCallback BookmarkMenuController::GetDropCallback(
    views::MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  auto drop_cb = menu_delegate_->GetDropCallback(menu, position, event);
  if (for_drop_) {
    delete this;
  }
  return drop_cb;
}

bool BookmarkMenuController::ShowContextMenu(
    MenuItemView* source,
    int id,
    const gfx::Point& p,
    ui::mojom::MenuSourceType source_type) {
  return menu_delegate_->ShowContextMenu(source, id, p, source_type);
}

bool BookmarkMenuController::CanDrag(MenuItemView* menu) {
  return menu_delegate_->CanDrag(menu);
}

void BookmarkMenuController::WriteDragData(MenuItemView* sender,
                                           ui::OSExchangeData* data) {
  return menu_delegate_->WriteDragData(sender, data);
}

int BookmarkMenuController::GetDragOperations(MenuItemView* sender) {
  return menu_delegate_->GetDragOperations(sender);
}

bool BookmarkMenuController::ShouldCloseOnDragComplete() {
  return false;
}

void BookmarkMenuController::OnMenuClosed(views::MenuItemView* menu) {
  delete this;
}

views::MenuItemView* BookmarkMenuController::GetSiblingMenu(
    views::MenuItemView* menu,
    const gfx::Point& screen_point,
    views::MenuAnchorPosition* anchor,
    bool* has_mnemonics,
    views::MenuButton** button) {
  if (!bookmark_bar_ || for_drop_) {
    return nullptr;
  }
  gfx::Point bookmark_bar_loc(screen_point);
  views::View::ConvertPointFromScreen(bookmark_bar_, &bookmark_bar_loc);
  size_t start_index;
  std::optional<BookmarkParentFolder> folder =
      bookmark_bar_->GetBookmarkFolderForButtonAtLocation(bookmark_bar_loc,
                                                          &start_index);
  if (!folder) {
    return nullptr;
  }

  menu_delegate_->SetActiveMenu(*folder, start_index);
  *button = bookmark_bar_->GetMenuButtonForFolder(*folder);
  bookmark_bar_->GetAnchorPositionForButton(*button, anchor);
  *has_mnemonics = false;
  return this->menu();
}

int BookmarkMenuController::GetMaxWidthForMenu(MenuItemView* view) {
  return menu_delegate_->GetMaxWidthForMenu(view);
}

void BookmarkMenuController::WillShowMenu(MenuItemView* menu) {
  menu_delegate_->WillShowMenu(menu);
}

void BookmarkMenuController::BookmarkMergedSurfaceServiceChanged() {
  if (!menu_delegate_->is_mutating_model()) {
    menu()->Cancel();
  }
}

void BookmarkMenuController::BookmarkStartIndexChanged(
    const BookmarkParentFolder& folder,
    size_t new_start_index) {
  menu_delegate_->SetMenuStartIndex(folder, new_start_index);
}

void BookmarkMenuController::BookmarkMergedSurfaceServiceLoaded() {
  BookmarkMergedSurfaceServiceChanged();
}

void BookmarkMenuController::BookmarkMergedSurfaceServiceBeingDeleted() {
  BookmarkMergedSurfaceServiceChanged();
}

void BookmarkMenuController::BookmarkNodeAdded(
    const BookmarkParentFolder& parent,
    size_t index) {
  BookmarkMergedSurfaceServiceChanged();
}

void BookmarkMenuController::BookmarkNodesRemoved(
    const BookmarkParentFolder& parent,
    const base::flat_set<const bookmarks::BookmarkNode*>& nodes) {
  BookmarkMergedSurfaceServiceChanged();
}

void BookmarkMenuController::BookmarkNodeMoved(
    const BookmarkParentFolder& old_parent,
    size_t old_index,
    const BookmarkParentFolder& new_parent,
    size_t new_index) {
  // The delegate is also an observer and will handle updating the menu.
  // Overriding the BookmarkNodeMoved method prevents the base class from
  // invoking `BookmarkModelChanged`, which would close the menu.
  CHECK(menu_delegate_.get());
}

void BookmarkMenuController::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  BookmarkMergedSurfaceServiceChanged();
}

void BookmarkMenuController::BookmarkParentFolderChildrenReordered(
    const BookmarkParentFolder& folder) {
  BookmarkMergedSurfaceServiceChanged();
}

void BookmarkMenuController::BookmarkAllUserNodesRemoved() {
  BookmarkMergedSurfaceServiceChanged();
}

bool BookmarkMenuController::ShouldTryPositioningBesideAnchor() const {
  // The bookmark menu appears from the bookmark bar, which has a set of buttons
  // positioned next to each other; if the bookmark menu appears beside its
  // anchor button, it will likely overlay the adjacent bookmark button, which
  // prevents easy scrubbing through the bookmark bar's menus.
  return false;
}

BookmarkMenuController::~BookmarkMenuController() {
  menu_delegate_->GetBookmarkMergedSurfaceService()->RemoveObserver(this);
  if (observer_) {
    observer_->BookmarkMenuControllerDeleted(this);
  }
}
