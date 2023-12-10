// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkNode;
using content::PageNavigator;
using views::MenuItemView;

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               views::Widget* parent,
                                               const BookmarkNode* node,
                                               size_t start_child_index,
                                               bool for_drop)
    : menu_delegate_(new BookmarkMenuDelegate(browser, parent)),
      node_(node),
      observer_(nullptr),
      for_drop_(for_drop),
      bookmark_bar_(nullptr) {
  menu_delegate_->Init(this, nullptr, node, start_child_index,
                       BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                       BookmarkLaunchLocation::kSubfolder);
  int run_type = 0;
  if (for_drop)
    run_type |= views::MenuRunner::FOR_DROP;
  menu_runner_ = std::make_unique<views::MenuRunner>(
      base::WrapUnique<MenuItemView>(menu_delegate_->menu()), run_type);
}

void BookmarkMenuController::RunMenuAt(BookmarkBarView* bookmark_bar) {
  bookmark_bar_ = bookmark_bar;
  views::MenuButton* menu_button = bookmark_bar_->GetMenuButtonForNode(node_);
  DCHECK(menu_button);
  views::MenuAnchorPosition anchor;
  bookmark_bar_->GetAnchorPositionForButton(menu_button, &anchor);
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(menu_button, &screen_loc);
  gfx::Rect bounds(screen_loc.x(), screen_loc.y(), menu_button->width(),
                   menu_button->height());
  menu_delegate_->GetBookmarkModel()->AddObserver(this);
  // We only delete ourself after the menu completes, so we can safely ignore
  // the return value.
  menu_runner_->RunMenuAt(menu_delegate_->parent(),
                          menu_button->button_controller(), bounds, anchor,
                          ui::MENU_SOURCE_NONE);
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
      int id, const ui::Event& e) {
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
  if (for_drop_)
    delete this;
  return drop_cb;
}

bool BookmarkMenuController::ShowContextMenu(MenuItemView* source,
                                             int id,
                                             const gfx::Point& p,
                                             ui::MenuSourceType source_type) {
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

void BookmarkMenuController::OnMenuClosed(views::MenuItemView* menu) {
  delete this;
}

views::MenuItemView* BookmarkMenuController::GetSiblingMenu(
    views::MenuItemView* menu,
    const gfx::Point& screen_point,
    views::MenuAnchorPosition* anchor,
    bool* has_mnemonics,
    views::MenuButton** button) {
  if (!bookmark_bar_ || for_drop_)
    return nullptr;
  gfx::Point bookmark_bar_loc(screen_point);
  views::View::ConvertPointFromScreen(bookmark_bar_, &bookmark_bar_loc);
  size_t start_index;
  const BookmarkNode* node = bookmark_bar_->GetNodeForButtonAtModelIndex(
      bookmark_bar_loc, &start_index);
  if (!node || !node->is_folder())
    return nullptr;

  menu_delegate_->SetActiveMenu(node, start_index);
  *button = bookmark_bar_->GetMenuButtonForNode(node);
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

void BookmarkMenuController::BookmarkModelChanged() {
  if (!menu_delegate_->is_mutating_model())
    menu()->Cancel();
}

bool BookmarkMenuController::ShouldTryPositioningBesideAnchor() const {
  // The bookmark menu appears from the bookmark bar, which has a set of buttons positioned next to
  // each other; if the bookmark menu appears beside its anchor button, it will likely overlay the
  // adjacent bookmark button, which prevents easy scrubbing through the bookmark bar's menus.
  return false;
}

BookmarkMenuController::~BookmarkMenuController() {
  menu_delegate_->GetBookmarkModel()->RemoveObserver(this);
  if (observer_)
    observer_->BookmarkMenuControllerDeleted(this);
}
