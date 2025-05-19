// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// Recursively clear any delegates from |menu| and its unbuilt submenus.
void ClearDelegatesFromSubmenu(NSMenu* menu) {
  CHECK(menu);
  // Either the delegate has been cleared, or items were never added.
  CHECK(![menu delegate] || [menu numberOfItems] == 0);
  [menu setDelegate:nil];
  NSArray* items = [menu itemArray];
  for (NSMenuItem* item in items) {
    if ([item hasSubmenu]) {
      ClearDelegatesFromSubmenu([item submenu]);
    }
  }
}

NSString* MenuTitleForNode(const BookmarkNode* node) {
  return base::SysUTF16ToNSString(node->GetTitle());
}

}  // namespace

BookmarkMenuBridge::BookmarkMenuBridge(Profile* profile, NSMenu* menu_root)
    : profile_(profile),
      controller_([[BookmarkMenuCocoaController alloc] initWithBridge:this]),
      menu_root_(menu_root) {
  CHECK(profile_);
  profile_dir_ = profile->GetPath();
  CHECK(menu_root_);
  CHECK(![menu_root_ delegate]);
  [menu_root_ setDelegate:controller_];

  bookmark_service_ =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_);

  // The bookmark service is only availble for Regular and Guest profiles.
  if (!bookmark_service_) {
    return;
  }

  bookmark_service_observation_.Observe(bookmark_service_);
  if (bookmark_service_->loaded()) {
    BookmarkMergedSurfaceServiceLoaded();
  }
}

BookmarkMenuBridge::~BookmarkMenuBridge() {
  ClearBookmarkMenu();
  [menu_root_ setDelegate:nil];
}

void BookmarkMenuBridge::BookmarkMergedSurfaceServiceLoaded() {
  InvalidateMenu();
}

bool BookmarkMenuBridge::IsMenuRoot(NSMenu* menu) {
  CHECK(menu);
  return menu == menu_root_;
}

void BookmarkMenuBridge::UpdateRootMenuIfInvalid() {
  CHECK(menu_root_);
  if (!IsMenuValid()) {
    BuildRootMenu(/*recurse=*/false);
  }
}

void BookmarkMenuBridge::UpdateNonRootMenu(NSMenu* menu,
                                           const BookmarkParentFolder& folder) {
  CHECK(menu);
  CHECK(!IsMenuRoot(menu));
  CHECK(controller_);
  CHECK_EQ([menu delegate], controller_);

  AddChildrenToMenu(folder, menu, /*recurse=*/false);

  // Clear the delegate to prevent further refreshes.
  [menu setDelegate:nil];
}

bool BookmarkMenuBridge::HasContent(const BookmarkParentFolder& folder) {
  // TODO(crbug.com/390398329): Verify if this should be replaced with
  // checking visibility of underlying nodes.
  return bookmark_service_->GetChildrenCount(folder) > 0;
}

void BookmarkMenuBridge::BuildRootMenu(bool recurse) {
  if (!bookmark_service_ || !bookmark_service_->loaded()) {
    return;
  }

  if (!folder_image_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    folder_image_ = rb.GetNativeImageNamed(IDR_FOLDER_CLOSED).ToNSImage();
    [folder_image_ setTemplate:YES];
  }

  ClearBookmarkMenu();

  BookmarkParentFolder bookmark_bar_folder =
      BookmarkParentFolder::BookmarkBarFolder();
  BookmarkParentFolder managed_folder = BookmarkParentFolder::ManagedFolder();

  // Add at most one separator for the bookmark bar and the managed bookmarks
  // folder.
  if (HasContent(bookmark_bar_folder) || HasContent(managed_folder)) {
    [menu_root_ addItem:[NSMenuItem separatorItem]];
  }

  if (HasContent(managed_folder)) {
    // Most users never see this node, so the image is only loaded if needed.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    NSImage* image =
        rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER_MANAGED).ToNSImage();
    AddSubmenu(menu_root_, managed_folder, image, recurse);
  }

  // Add entries from the bookmark bar to the root menu.
  for (const BookmarkNode* node :
       bookmark_service_->GetChildren(bookmark_bar_folder)) {
    AddNodeToMenu(node, menu_root_, recurse);
  }

  BookmarkParentFolder other_folder = BookmarkParentFolder::OtherFolder();
  BookmarkParentFolder mobile_folder = BookmarkParentFolder::MobileFolder();

  // Add at most one separator for the "Other Bookmarks" and "Mobile Bookmarks"
  // folders.
  if (HasContent(other_folder) || HasContent(mobile_folder)) {
    [menu_root_ addItem:[NSMenuItem separatorItem]];
  }

  if (HasContent(other_folder)) {
    AddSubmenu(menu_root_, other_folder, folder_image_, recurse);
  }
  if (HasContent(mobile_folder)) {
    AddSubmenu(menu_root_, mobile_folder, folder_image_, recurse);
  }

  is_menu_valid_ = true;
}

void BookmarkMenuBridge::BookmarkMergedSurfaceServiceBeingDeleted() {}

void BookmarkMenuBridge::BookmarkNodeAdded(const BookmarkParentFolder& parent,
                                           size_t index) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodesRemoved(
    const BookmarkParentFolder& parent,
    const base::flat_set<const bookmarks::BookmarkNode*>& nodes) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeMoved(
    const BookmarkParentFolder& old_parent,
    size_t old_index,
    const BookmarkParentFolder& new_parent,
    size_t new_index) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeChanged(const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item) {
    ConfigureMenuItem(node, item);
  }
}

void BookmarkMenuBridge::BookmarkNodeFaviconChanged(const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item) {
    ConfigureMenuItem(node, item);
  }
}

void BookmarkMenuBridge::BookmarkParentFolderChildrenReordered(
    const BookmarkParentFolder& folder) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkAllUserNodesRemoved() {
  InvalidateMenu();
}

BookmarkModel* BookmarkMenuBridge::GetBookmarkModelForTesting() {
  CHECK(bookmark_service_);
  return bookmark_service_->bookmark_model();
}

Profile* BookmarkMenuBridge::GetProfile() {
  return profile_;
}

const base::FilePath& BookmarkMenuBridge::GetProfileDir() const {
  return profile_dir_;
}

NSMenu* BookmarkMenuBridge::BookmarkMenu() {
  return menu_root_;
}

void BookmarkMenuBridge::ClearBookmarkMenu() {
  InvalidateMenu();
  bookmark_nodes_.clear();
  tag_to_guid_.clear();

  // Recursively delete all menus that look like a bookmark. Also delete all
  // separator items since we explicitly add them back in. This deletes
  // everything except the first item ("Add Bookmark...").
  NSArray* items = [menu_root_ itemArray];
  for (NSMenuItem* item in items) {
    // If there's a submenu, it may have a reference to |controller_|. Ensure
    // that gets nerfed recursively.
    if ([item hasSubmenu]) {
      ClearDelegatesFromSubmenu([item submenu]);
    }

    // Convention: items in the bookmark list which are bookmarks have
    // an action of openBookmarkMenuItem:.  Also, assume all items
    // with submenus are submenus of bookmarks.
    if (([item action] == @selector(openBookmarkMenuItem:)) ||
        [item hasSubmenu] || [item isSeparatorItem]) {
      // This will eventually [obj release] all its kids, if it has any.
      [menu_root_ removeItem:item];
    } else {
      // Leave it alone.
    }
  }
}

void BookmarkMenuBridge::AddSubmenu(NSMenu* menu,
                                    const BookmarkParentFolder& folder,
                                    NSImage* image,
                                    bool recurse) {
  // For permanent folders containing multiple nodes, use the first node's title
  // as the menu title.
  auto nodes = bookmark_service_->GetUnderlyingNodes(folder);
  CHECK(!nodes.empty());
  const BookmarkNode* node = nodes[0];
  NSString* title = MenuTitleForNode(node);
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                action:nil
                                         keyEquivalent:@""];
  [item setImage:image];
  ConfigureMenuItem(node, item);
  bookmark_nodes_[node] = item;

  NSMenu* submenu = [[NSMenu alloc] initWithTitle:title];
  [menu setSubmenu:submenu forItem:item];

  // Set a delegate and a tag on the item so that the submenu can be populated
  // when (and if) Cocoa asks for it.
  if (!recurse) {
    [submenu setDelegate:controller_];
  }

  [menu addItem:item];

  if (recurse) {
    AddChildrenToMenu(folder, submenu, recurse);
  }
}

void BookmarkMenuBridge::AddChildrenToMenu(const BookmarkParentFolder& folder,
                                           NSMenu* menu,
                                           bool recurse) {
  BookmarkParentFolderChildren children =
      bookmark_service_->GetChildren(folder);
  if (!children.size()) {
    // Permanent folders with no children are not visible.
    CHECK(folder.HoldsNonPermanentFolder());
    // For empty non-permanent folder, show an unclickable entry with the text
    // "(empty)".
    NSString* empty_string = l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU);
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:empty_string
                                                  action:nil
                                           keyEquivalent:@""];
    [menu addItem:item];
    return;
  }
  for (const BookmarkNode* child : children) {
    AddNodeToMenu(child, menu, recurse);
  }
}

void BookmarkMenuBridge::AddNodeToMenu(const BookmarkNode* node,
                                       NSMenu* menu,
                                       bool recurse) {
  if (node->is_folder()) {
    AddSubmenu(menu, BookmarkParentFolder::FromFolderNode(node), folder_image_,
               recurse);
  } else {
    CHECK(node->is_url());
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:MenuTitleForNode(node)
                                                  action:nil
                                           keyEquivalent:@""];
    bookmark_nodes_[node] = item;
    ConfigureMenuItem(node, item);
    [menu addItem:item];
  }
}

void BookmarkMenuBridge::ConfigureMenuItem(const BookmarkNode* node,
                                           NSMenuItem* item) {
  [item setTitle:MenuTitleForNode(node)];
  [item setTag:node->id()];
  tag_to_guid_[node->id()] = node->uuid();

  // The following settings only apply to URL items.
  if (node->is_folder()) {
    return;
  }
  CHECK(node->is_url());

  [item setTarget:controller_];
  [item setAction:@selector(openBookmarkMenuItem:)];
  [item setToolTip:[BookmarkMenuCocoaController tooltipForNode:node]];

  // Check to see if we have a favicon.
  NSImage* favicon = nil;
  BookmarkModel* model = bookmark_service_->bookmark_model();
  if (model) {
    const gfx::Image& image = model->GetFavicon(node);
    if (!image.IsEmpty()) {
      favicon = image.ToNSImage();
    }
  }
  // If we do not have a loaded favicon, use the default site image instead.
  if (!favicon) {
    favicon = favicon::GetDefaultFavicon().ToNSImage();
    [favicon setTemplate:YES];
  }
  [item setImage:favicon];
}

NSMenuItem* BookmarkMenuBridge::MenuItemForNode(const BookmarkNode* node) {
  if (!node) {
    return nil;
  }
  auto it = bookmark_nodes_.find(node);
  if (it == bookmark_nodes_.end()) {
    return nil;
  }
  return it->second;
}

NSMenuItem* BookmarkMenuBridge::MenuItemForNodeForTest(
    const bookmarks::BookmarkNode* node) {
  return MenuItemForNode(node);
}

void BookmarkMenuBridge::OnProfileWillBeDestroyed() {
  // Recursively populate the menu before the bookmark service is destroyed.
  BuildRootMenu(/*recurse=*/true);

  bookmark_service_observation_.Reset();
  bookmark_service_ = nullptr;
  profile_ = nullptr;

  // |bookmark_nodes_| stores the nodes by pointer, so it would be unsafe to
  // keep them.
  bookmark_nodes_.clear();
}

base::Uuid BookmarkMenuBridge::TagToGUID(int64_t tag) const {
  const auto& it = tag_to_guid_.find(tag);
  return it == tag_to_guid_.end() ? base::Uuid() : it->second;
}
