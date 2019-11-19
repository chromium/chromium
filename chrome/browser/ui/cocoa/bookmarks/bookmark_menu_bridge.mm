// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
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
  DCHECK(menu);
  // Either the delegate has been cleared, or items were never added.
  DCHECK(![menu delegate] || [menu numberOfItems] == 0);
  [menu setDelegate:nil];
  NSArray* items = [menu itemArray];
  for (NSMenuItem* item in items) {
    if ([item hasSubmenu])
      ClearDelegatesFromSubmenu([item submenu]);
  }
}

NSString* MenuTitleForNode(const BookmarkNode* node) {
  return base::SysUTF16ToNSString(node->GetTitle());
}

}  // namespace

BookmarkMenuBridge::BookmarkMenuBridge(Profile* profile, NSMenu* menu_root)
    : profile_(profile),
      controller_([[BookmarkMenuCocoaController alloc] initWithBridge:this]),
      menu_root_([menu_root retain]) {
  DCHECK(profile_);
  DCHECK(menu_root_);
  DCHECK(![menu_root_ delegate]);
  [menu_root_ setDelegate:controller_];

  DCHECK(GetBookmarkModel());
  ObserveBookmarkModel();
}

BookmarkMenuBridge::~BookmarkMenuBridge() {
  ClearBookmarkMenu();
  [menu_root_ setDelegate:nil];
  BookmarkModel* model = GetBookmarkModel();
  DCHECK(model);
  if (model)
    model->RemoveObserver(this);
}

void BookmarkMenuBridge::BookmarkModelLoaded(BookmarkModel* model,
                                             bool ids_reassigned) {
  InvalidateMenu();
}

void BookmarkMenuBridge::UpdateMenu(NSMenu* menu, const BookmarkNode* node) {
  DCHECK(menu);
  DCHECK(controller_);
  DCHECK_EQ([menu delegate], controller_.get());

  if (menu == menu_root_) {
    if (!IsMenuValid())
      BuildRootMenu();
    return;
  }

  AddNodeToMenu(node, menu);
  // Clear the delegate to prevent further refreshes.
  [menu setDelegate:nil];
}

void BookmarkMenuBridge::BuildRootMenu() {
  BookmarkModel* model = GetBookmarkModel();
  if (!model || !model->loaded())
    return;

  if (!folder_image_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    folder_image_.reset(
        [rb.GetNativeImageNamed(IDR_FOLDER_CLOSED).ToNSImage() retain]);
    [folder_image_ setTemplate:YES];
  }

  ClearBookmarkMenu();

  // Add at most one separator for the bookmark bar and the managed bookmarks
  // folder.
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile_);
  const BookmarkNode* barNode = model->bookmark_bar_node();
  const BookmarkNode* managedNode = managed->managed_node();
  if (!barNode->children().empty() || !managedNode->children().empty())
    [menu_root_ addItem:[NSMenuItem separatorItem]];
  if (!managedNode->children().empty()) {
    // Most users never see this node, so the image is only loaded if needed.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    NSImage* image =
        rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER_MANAGED).ToNSImage();
    AddNodeAsSubmenu(menu_root_, managedNode, image);
  }
  if (!barNode->children().empty())
    AddNodeToMenu(barNode, menu_root_);

  // If the "Other Bookmarks" folder has any content, make a submenu for it and
  // fill it in.
  if (!model->other_node()->children().empty()) {
    [menu_root_ addItem:[NSMenuItem separatorItem]];
    AddNodeAsSubmenu(menu_root_, model->other_node(), folder_image_);
  }

  // If the "Mobile Bookmarks" folder has any content, make a submenu for it and
  // fill it in.
  if (!model->mobile_node()->children().empty()) {
    // Add a separator if we did not already add one due to a non-empty
    // "Other Bookmarks" folder.
    if (model->other_node()->children().empty())
      [menu_root_ addItem:[NSMenuItem separatorItem]];

    AddNodeAsSubmenu(menu_root_, model->mobile_node(), folder_image_);
  }

  menuIsValid_ = true;
}

void BookmarkMenuBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  ClearBookmarkMenu();
}

void BookmarkMenuBridge::BookmarkNodeMoved(BookmarkModel* model,
                                           const BookmarkNode* old_parent,
                                           size_t old_index,
                                           const BookmarkNode* new_parent,
                                           size_t new_index) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           size_t index) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  InvalidateMenu();
}

void BookmarkMenuBridge::BookmarkNodeChanged(BookmarkModel* model,
                                             const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item, true);
}

void BookmarkMenuBridge::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  NSMenuItem* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item, false);
}

void BookmarkMenuBridge::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  InvalidateMenu();
}

// Watch for changes.
void BookmarkMenuBridge::ObserveBookmarkModel() {
  BookmarkModel* model = GetBookmarkModel();
  model->AddObserver(this);
  if (model->loaded())
    BookmarkModelLoaded(model, false);
}

BookmarkModel* BookmarkMenuBridge::GetBookmarkModel() {
  DCHECK(profile_);
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

Profile* BookmarkMenuBridge::GetProfile() {
  return profile_;
}

NSMenu* BookmarkMenuBridge::BookmarkMenu() {
  return menu_root_;
}

void BookmarkMenuBridge::ClearBookmarkMenu() {
  InvalidateMenu();
  bookmark_nodes_.clear();

  // Recursively delete all menus that look like a bookmark. Also delete all
  // separator items since we explicitly add them back in. This deletes
  // everything except the first item ("Add Bookmark...").
  NSArray* items = [menu_root_ itemArray];
  for (NSMenuItem* item in items) {
    // If there's a submenu, it may have a reference to |controller_|. Ensure
    // that gets nerfed recursively.
    if ([item hasSubmenu])
      ClearDelegatesFromSubmenu([item submenu]);

    // Convention: items in the bookmark list which are bookmarks have
    // an action of openBookmarkMenuItem:.  Also, assume all items
    // with submenus are submenus of bookmarks.
    if (([item action] == @selector(openBookmarkMenuItem:)) ||
        [item hasSubmenu] ||
        [item isSeparatorItem]) {
      // This will eventually [obj release] all its kids, if it has any.
      [menu_root_ removeItem:item];
    } else {
      // Leave it alone.
    }
  }
}

void BookmarkMenuBridge::AddNodeAsSubmenu(NSMenu* menu,
                                          const BookmarkNode* node,
                                          NSImage* image) {
  NSString* title = MenuTitleForNode(node);
  base::scoped_nsobject<NSMenuItem> items(
      [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""]);
  [items setImage:image];
  base::scoped_nsobject<NSMenu> submenu([[NSMenu alloc] initWithTitle:title]);
  [menu setSubmenu:submenu forItem:items];

  // Set a delegate and a tag on the item so that the submenu can be populated
  // when (and if) Cocoa asks for it.
  [submenu setDelegate:controller_];
  [items setTag:node->id()];

  [menu addItem:items];
}

// TODO(jrg): limit the number of bookmarks in the menubar?
void BookmarkMenuBridge::AddNodeToMenu(const BookmarkNode* node, NSMenu* menu) {
  if (node->children().empty()) {
    NSString* empty_string = l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU);
    base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
        initWithTitle:empty_string
               action:nil
        keyEquivalent:@""]);
    [menu addItem:item];
    return;
  }

  for (const auto& child : node->children()) {
    if (child->is_folder()) {
      AddNodeAsSubmenu(menu, child.get(), folder_image_);
    } else {
      base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
          initWithTitle:MenuTitleForNode(child.get())
                 action:nil
          keyEquivalent:@""]);
      bookmark_nodes_[child.get()] = item;
      ConfigureMenuItem(child.get(), item, false);
      [menu addItem:item];
    }
  }
}

void BookmarkMenuBridge::ConfigureMenuItem(const BookmarkNode* node,
                                           NSMenuItem* item,
                                           bool set_title) {
  if (set_title)
    [item setTitle:MenuTitleForNode(node)];
  [item setTarget:controller_];
  [item setAction:@selector(openBookmarkMenuItem:)];
  [item setTag:node->id()];
  if (node->is_url())
    [item setToolTip:[BookmarkMenuCocoaController tooltipForNode:node]];
  // Check to see if we have a favicon.
  NSImage* favicon = nil;
  BookmarkModel* model = GetBookmarkModel();
  if (model) {
    const gfx::Image& image = model->GetFavicon(node);
    if (!image.IsEmpty())
      favicon = image.ToNSImage();
  }
  // If we do not have a loaded favicon, use the default site image instead.
  if (!favicon) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    favicon = rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage();
    [favicon setTemplate:YES];
  }
  [item setImage:favicon];
}

NSMenuItem* BookmarkMenuBridge::MenuItemForNode(const BookmarkNode* node) {
  if (!node)
    return nil;
  auto it = bookmark_nodes_.find(node);
  if (it == bookmark_nodes_.end())
    return nil;
  return it->second;
}
