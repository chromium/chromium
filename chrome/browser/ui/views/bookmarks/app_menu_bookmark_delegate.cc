// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/app_menu_bookmark_delegate.h"

#include <cstddef>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"

AppMenuBookmarkDelegate::AppMenuBookmarkDelegate(
    BrowserWindowInterface* browser,
    views::Widget* parent,
    views::MenuDelegate* real_delegate,
    BookmarkLaunchLocation location)
    : BookmarkMenuDelegate(browser, parent, real_delegate, location) {}

AppMenuBookmarkDelegate::~AppMenuBookmarkDelegate() = default;

void AppMenuBookmarkDelegate::BuildFullMenu(views::MenuItemView* parent) {
  CHECK(!parent_menu_item_);
  CHECK(parent);
  CHECK(parent->GetSubmenu());
  parent_menu_item_ = parent;
  // Assume that the menu will only use mnemonics if there's already a parent
  // menu that uses them.
  set_menu_uses_mnemonics(
      parent_menu_item_->GetRootMenuItem()->has_mnemonics());
  if (ShouldHaveBookmarksTitle()) {
    const size_t title_index = GetSubmenuChildCount(parent_menu_item_);
    BuildBookmarksTitle(title_index);
  }

  const BookmarkParentFolder managed_folder =
      BookmarkParentFolder::ManagedFolder();
  if (ShouldBuildPermanentNode(managed_folder)) {
    BuildMenuForFolder(
        managed_folder,
        chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kManaged,
                                      ui::kColorMenuIcon),
        parent_menu_item_);
  }
  BuildMenu(BookmarkParentFolder::BookmarkBarFolder(), 0, parent);
  BuildMenusForPermanentNodes();
}

std::optional<size_t> AppMenuBookmarkDelegate::AdjustInsertionIndex(
    const BookmarkParentFolder& folder,
    views::MenuItemView* parent_menu,
    size_t new_index) {
  size_t insertion_idx = new_index;

  // If the bookmark is embedded in a larger menu not controlled by this (e.g.,
  // App menu), then the bookmark's menu item is inserted relative to the
  // "Bookmarks" title.
  if (parent_menu == parent_menu_item_) {
    if (bookmarks_title_) {
      insertion_idx += SubmenuIndexOf(parent_menu_item_, bookmarks_title_) + 1;
    }
    // The managed bookmarks folder is displayed immediately after the
    // "Bookmarks" title.
    if (node_to_menu_map().contains(
            BookmarkFolderOrURL(BookmarkParentFolder::ManagedFolder()))) {
      ++insertion_idx;
    }
  }

  return insertion_idx;
}

std::vector<raw_ref<views::MenuItemView>>
AppMenuBookmarkDelegate::GetAndUpdateStaleMenuArtifacts() {
  std::vector<raw_ref<views::MenuItemView>> updated_menus =
      BookmarkMenuDelegate::GetAndUpdateStaleMenuArtifacts();
  if (parent_menu_item_) {
    if (views::MenuItemView* updated_menu = UpdateBookmarksTitle()) {
      updated_menus.emplace_back(*updated_menu);
    }
  }
  return updated_menus;
}

bool AppMenuBookmarkDelegate::ShouldBuildPermanentNode(
    const BookmarkParentFolder& folder) const {
  return GetBookmarkMergedSurfaceService()->GetChildrenCount(folder);
}

void AppMenuBookmarkDelegate::BuildMenusForPermanentNodes() {
  CHECK(parent_menu_item_);
  const BookmarkParentFolder other_folder(BookmarkParentFolder::OtherFolder());
  const BookmarkParentFolder mobile_folder(
      BookmarkParentFolder::MobileFolder());
  const bool should_build_other_node = ShouldBuildPermanentNode(other_folder);
  const bool should_build_mobile_node = ShouldBuildPermanentNode(mobile_folder);

  if (!should_build_other_node && !should_build_mobile_node) {
    return;
  }

  views::SubmenuView* submenu = parent_menu_item_->GetSubmenu();
  CHECK(!permanent_nodes_separator_);
  parent_menu_item_->AppendSeparator();
  permanent_nodes_separator_ = submenu->children().back().get();

  const ui::ImageModel& folder_icon = chrome::GetBookmarkFolderIcon(
      chrome::BookmarkFolderIconType::kNormal, ui::kColorMenuIcon);
  if (should_build_other_node) {
    BuildMenuForFolder(other_folder, folder_icon, parent_menu_item_);
  }

  if (should_build_mobile_node) {
    BuildMenuForFolder(mobile_folder, folder_icon, parent_menu_item_);
  }
}

views::MenuItemView* AppMenuBookmarkDelegate::UpdateBookmarksTitle() {
  CHECK(parent_menu_item_);
  CHECK(parent_menu_item_->HasSubmenu());
  // Check if we need to add/remove the bookmarks title. If not, then return
  // null since the parent menu doesn't need to be updated.
  const bool should_have_title = ShouldHaveBookmarksTitle();
  if (!bookmarks_title_ && !should_have_title) {
    return nullptr;
  }
  if (bookmarks_title_ && should_have_title) {
    return nullptr;
  }

  if (bookmarks_title_) {
    RemoveBookmarksTitle();
  } else {
    // If permanent nodes are already built in `parent_menu_item_`, then add the
    // title above them. Otherwise, append the title to the parent menu.
    // E.g., this can happen in the App menu if there are initially no bookmarks
    // in the bookmarks bar, but there are bookmarks in the "other" bookmarks
    // folder, which has its own section. The "Bookmarks" title would need
    // to be inserted above the "other" bookmarks.
    size_t offset =
        permanent_nodes_separator_
            ? SubmenuIndexOf(parent_menu_item_, permanent_nodes_separator_)
            : GetSubmenuChildCount(parent_menu_item_);
    BuildBookmarksTitle(offset);
  }
  return parent_menu_item_;
}

bool AppMenuBookmarkDelegate::ShouldHaveBookmarksTitle() {
  CHECK(parent_menu_item_);
  // In practice, the parent menu item is never empty.
  // If this assumption is wrong, there may be a redundant "separator" visual
  // artifact, but the code will continue to function correctly (hence why we
  // don't crash here).
  // If this ever changes, then the delegate will need to observe and
  // react to non-bookmark changes in its parent menu, which is currently not
  // supported.
  if (parent_menu_item_->GetSubmenu()->children().empty()) {
    // Report the unexpected empty parent menu without crashing.
    base::debug::DumpWithoutCrashing();
  }
  const BookmarkMergedSurfaceService* service =
      GetBookmarkMergedSurfaceService();
  const bool bookmark_bar_has_children =
      service->GetChildrenCount(BookmarkParentFolder::BookmarkBarFolder());
  return (bookmark_bar_has_children ||
          ShouldBuildPermanentNode(BookmarkParentFolder::ManagedFolder()));
}

void AppMenuBookmarkDelegate::BuildBookmarksTitle(size_t index) {
  CHECK(!bookmarks_title_);
  CHECK(!bookmarks_title_separator_);
  parent_menu_item_->AddSeparatorAt(index);
  bookmarks_title_separator_ =
      parent_menu_item_->GetSubmenu()->children()[index].get();
  bookmarks_title_ = parent_menu_item_->AddTitleAt(
      l10n_util::GetStringUTF16(IDS_BOOKMARKS_LIST_TITLE), index + 1);
}

void AppMenuBookmarkDelegate::RemoveBookmarksTitle() {
  CHECK(parent_menu_item_);
  CHECK(bookmarks_title_);
  CHECK(bookmarks_title_separator_);
  views::View* title = bookmarks_title_.get();
  views::View* separator = bookmarks_title_separator_.get();
  bookmarks_title_ = nullptr;
  bookmarks_title_separator_ = nullptr;
  parent_menu_item_->RemoveMenuItem(title);
  parent_menu_item_->RemoveMenuItem(separator);
}
