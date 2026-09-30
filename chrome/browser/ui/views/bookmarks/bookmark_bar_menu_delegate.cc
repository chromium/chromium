// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_menu_delegate.h"

#include "base/check_op.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/views/controls/menu/menu_item_view.h"

BookmarkBarMenuDelegate::BookmarkBarMenuDelegate(
    BrowserWindowInterface* browser,
    views::Widget* parent,
    views::MenuDelegate* real_delegate,
    BookmarkLaunchLocation location)
    : BookmarkMenuDelegate(browser, parent, real_delegate, location) {}

BookmarkBarMenuDelegate::~BookmarkBarMenuDelegate() = default;

void BookmarkBarMenuDelegate::SetActiveMenu(const BookmarkParentFolder& folder,
                                            size_t start_index) {
  const BookmarkFolderOrURL node(folder);
  auto node_to_menu = node_to_menu_map().find(node);
  if (node_to_menu == node_to_menu_map().end() || !node_to_menu->second) {
    menu_ = CreateMenu(folder, start_index);
  } else {
    menu_ = node_to_menu->second;
  }
}

void BookmarkBarMenuDelegate::SetMenuStartIndex(
    const BookmarkParentFolder& folder,
    size_t start_index) {
  const BookmarkMergedSurfaceService* service =
      GetBookmarkMergedSurfaceService();
  auto node_to_start_idx = node_start_child_idx_map_.find(folder);
  const size_t prev_start_idx =
      node_to_start_idx == node_start_child_idx_map_.end()
          ? 0
          : node_to_start_idx->second;

  if (prev_start_idx == start_index) {
    return;
  }

  // It's possible the menu hasn't been built yet, so no update is necessary.
  auto node_to_menu = node_to_menu_map().find(BookmarkFolderOrURL(folder));
  if (node_to_menu == node_to_menu_map().end()) {
    return;
  }

  CHECK_LE(start_index, service->GetChildrenCount(folder));
  node_start_child_idx_map_[folder] = start_index;
  views::MenuItemView* parent_menu = node_to_menu->second;

  // Remove obsolete bookmark menus if the start index increased.
  BookmarkParentFolderChildren children = service->GetChildren(folder);
  for (size_t idx = prev_start_idx; idx < start_index; ++idx) {
    const bookmarks::BookmarkNode* child_node = children[idx];
    if (auto child_node_to_menu =
            node_to_menu_map().find(BookmarkFolderOrURL(child_node));
        child_node_to_menu != node_to_menu_map().end()) {
      RemoveBookmarkNode(child_node, child_node_to_menu->second);
    }
  }

  // Add missing bookmark menus if the start index decreased.
  for (size_t idx = start_index; idx < prev_start_idx; ++idx) {
    const bookmarks::BookmarkNode* child_node = children[idx];
    AddBookmarkNode(child_node, parent_menu, idx);
  }

  parent_menu->ChildrenChanged();
}

bool BookmarkBarMenuDelegate::ShouldCloseOnRemove(
    const BookmarkFolderOrURL& folder_or_url) const {
  const bookmarks::BookmarkNode* node = folder_or_url.GetIfNonPermanentNode();
  if (!node) {
    // Permanent node.
    return false;
  }

  const bool is_only_child_of_other_folder =
      node->parent()->type() == bookmarks::BookmarkNode::OTHER_NODE &&
      GetBookmarkMergedSurfaceService()->GetChildrenCount(
          BookmarkParentFolder::OtherFolder()) == 1u;
  const bool is_child_of_bookmark_bar =
      node->parent()->type() == bookmarks::BookmarkNode::BOOKMARK_BAR;

  // Fast-path for non-bookmark-bar nodes.
  if (!is_child_of_bookmark_bar) {
    return is_only_child_of_other_folder;
  }

  bool is_shown_from_bookmark_bar_overflow = false;
  if (menu_) {
    auto active_menu = menu_id_to_node_map().find(menu_->GetCommand());
    if (active_menu != menu_id_to_node_map().end()) {
      if (const BookmarkParentFolder* active_folder =
              active_menu->second.GetIfBookmarkFolder();
          active_folder &&
          active_folder->as_permanent_folder() ==
              BookmarkParentFolder::PermanentFolderType::kBookmarkBarNode) {
        auto menu_start_idx = node_start_child_idx_map_.find(*active_folder);
        is_shown_from_bookmark_bar_overflow =
            menu_start_idx != node_start_child_idx_map_.end() &&
            menu_start_idx->second > 0;
      }
    }
  }
  // The 'other' bookmarks folder hides when it has no more items, so we need
  // to exit the menu when the last node is removed.
  // If the parent is the bookmark bar and we're not in the overflow menu, then
  // the menu is anchored to an individual bookmark button. Removing it requires
  // closing the menu because there is no longer a stable anchor.
  return is_only_child_of_other_folder || !is_shown_from_bookmark_bar_overflow;
}

std::optional<size_t> BookmarkBarMenuDelegate::AdjustInsertionIndex(
    const BookmarkParentFolder& folder,
    views::MenuItemView* parent_menu,
    size_t new_index) {
  size_t insertion_idx = new_index;

  // The bookmark bar view creates individual menus for bookmarks in the
  // bookmarks bar. Bookmarks that overflow from the bar belong to a
  // single menu, which uses a node offset. This offset should be applied to
  // `new_index` to ensure the moved node's menu item appears in the right
  // spot in the overflow menu.
  if (auto node_to_start_child_idx = node_start_child_idx_map_.find(folder);
      node_to_start_child_idx != node_start_child_idx_map_.end()) {
    // If `new_index` is less than the menu's start index, this means that
    // the moved bookmark isn't in its parent's menu. The client will reorder
    // the menu in the bookmarks bar. Therefore, we skip the update.
    if (new_index < node_to_start_child_idx->second) {
      return std::nullopt;
    }
    insertion_idx -= node_to_start_child_idx->second;
  }

  return insertion_idx;
}

views::MenuItemView* BookmarkBarMenuDelegate::CreateMenu(
    const BookmarkParentFolder& folder,
    size_t start_child_index) {
  views::MenuItemView* menu = new views::MenuItemView(real_delegate());
  menu->SetCommand(GetAndIncrementNextMenuID());
  AddMenuToMaps(menu, BookmarkFolderOrURL(folder));
  node_start_child_idx_map_[folder] = start_child_index;

  BuildMenu(folder, start_child_index, menu);
  return menu;
}
