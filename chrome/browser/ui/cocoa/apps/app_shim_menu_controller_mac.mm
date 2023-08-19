// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"

#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/containers/adapters.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

// Gets an item from the main menu given the tag of the top level item
// |menu_tag| and the tag of the item |item_tag|.
NSMenuItem* GetItemByTag(NSInteger menu_tag, NSInteger item_tag) {
  return [[[[NSApp mainMenu] itemWithTag:menu_tag] submenu]
      itemWithTag:item_tag];
}

// Finds a top level menu item using |menu_tag| and creates a new NSMenuItem
// with the same title.
NSMenuItem* NewTopLevelItemFrom(NSInteger menu_tag) {
  NSMenuItem* original = [[NSApp mainMenu] itemWithTag:menu_tag];
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[original title]
                                                action:nil
                                         keyEquivalent:@""];
  DCHECK([original hasSubmenu]);
  NSMenu* sub_menu = [[NSMenu alloc] initWithTitle:[[original submenu] title]];
  [item setSubmenu:sub_menu];
  return item;
}

// Finds an item using |menu_tag| and |item_tag| and adds a duplicate of it to
// the submenu of |top_level_item|.
void AddDuplicateItem(NSMenuItem* top_level_item,
                      NSInteger menu_tag,
                      NSInteger item_tag) {
  NSMenuItem* item = [GetItemByTag(menu_tag, item_tag) copy];
  DCHECK(item);
  [[top_level_item submenu] addItem:item];
}

// Finds an item with |item_tag| and removes it from the submenu of
// |top_level_item|.
void RemoveMenuItemWithTag(NSMenuItem* top_level_item,
                           NSInteger item_tag,
                           bool remove_following_separator) {
  NSMenu* submenu = [top_level_item submenu];
  NSInteger index = [submenu indexOfItemWithTag:item_tag];
  if (index < 0)
    return;

  [submenu removeItemAtIndex:index];

  if (!remove_following_separator || index == [submenu numberOfItems])
    return;

  NSMenuItem* nextItem = [submenu itemAtIndex:index];
  if ([nextItem isSeparatorItem])
    [submenu removeItem:nextItem];
}

// Sets the menu item with |item_tag| in |top_level_item| visible.
// If |has_alternate| is true, the item immediately following |item_tag| is
// assumed to be its (only) alternate. Since AppKit is unable to hide items
// with alternates, |has_alternate| will cause -[NSMenuItem alternate] to be
// removed when hiding and restored when showing.
void SetItemWithTagVisible(NSMenuItem* top_level_item,
                           NSInteger item_tag,
                           bool visible,
                           bool has_alternate) {
  NSMenu* submenu = [top_level_item submenu];
  NSMenuItem* menu_item = [submenu itemWithTag:item_tag];
  DCHECK(menu_item);

  if (visible != [menu_item isHidden])
    return;

  if (!has_alternate) {
    [menu_item setHidden:!visible];
    return;
  }

  NSInteger next_index = [submenu indexOfItem:menu_item] + 1;
  DCHECK_LT(next_index, [submenu numberOfItems]);

  NSMenuItem* alternate_item = [submenu itemAtIndex:next_index];
  if (!visible) {
    // When hiding (only), we can verify the assumption that the item following
    // |item_tag| is actually an alternate.
    DCHECK([alternate_item isAlternate]);
  }

  // The alternate item visibility should always be in sync.
  DCHECK_EQ([alternate_item isHidden], [menu_item isHidden]);
  [alternate_item setAlternate:visible];
  [alternate_item setHidden:!visible];
  [menu_item setHidden:!visible];
}

// Retrieve the Extension and (optionally) Profile for an NSWindow.
const Extension* GetExtensionForNSWindow(NSWindow* window,
                                         Profile** profile = nullptr) {
  if (extensions::AppWindow* app_window =
          AppWindowRegistryUtil::GetAppWindowForNativeWindowAnyProfile(
              window)) {
    if (profile)
      *profile = Profile::FromBrowserContext(app_window->browser_context());
    return app_window->GetExtension();
  }
  // If there is no corresponding AppWindow, this could be a hosted app, so
  // check for a browser.
  if (Browser* browser = chrome::FindBrowserWithWindow(window)) {
    const std::string app_id =
        web_app::GetAppIdFromApplicationName(browser->app_name());
    if (profile)
      *profile = browser->profile();
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser->profile());
    const Extension* extension =
        registry->GetExtensionById(app_id, ExtensionRegistry::ENABLED);
    if (extension &&
        (extension->is_platform_app() || extension->is_hosted_app())) {
      return extension;
    }
  }
  return nullptr;
}

extensions::AppWindowRegistry::AppWindowList GetAppWindowsForNSWindow(
    NSWindow* window) {
  Profile* profile = nullptr;
  if (const Extension* extension = GetExtensionForNSWindow(window, &profile)) {
    return extensions::AppWindowRegistry::Get(profile)->GetAppWindowsForApp(
        extension->id());
  }
  return extensions::AppWindowRegistry::AppWindowList();
}

}  // namespace

// Used by AppShimMenuController to manage menu items that are a copy of a
// Chrome menu item but with a different action. This manages unsetting and
// restoring the original item's key equivalent, so that we can use the same
// key equivalent in the copied item with a different action. If |resourceId_|
// is non-zero, this will also update the title to include the app name.
// If the copy (menuItem) has no key equivalent, and the title does not have the
// app name, then enableForApp and disable do not need to be called. I.e. the
// doppelganger just copies the item and sets a new action.
@interface DoppelgangerMenuItem : NSObject {
 @private
  NSMenuItem* __strong _menuItem;
  NSMenuItem* __strong _sourceItem;
  NSString* __strong _sourceKeyEquivalent;
  int _resourceId;
}

@property(readonly, nonatomic) NSMenuItem* menuItem;

// Get the source item using the tags and create the menu item.
- (instancetype)initWithController:(AppShimMenuController*)controller
                           menuTag:(NSInteger)menuTag
                           itemTag:(NSInteger)itemTag
                        resourceId:(int)resourceId
                            action:(SEL)action
                     keyEquivalent:(NSString*)keyEquivalent;
// Retain the source item given |menuTag| and |sourceItemTag|. Copy
// the menu item given |menuTag| and |targetItemTag|.
// This is useful when we want a doppelganger with a different source item.
// For example, if there are conflicting key equivalents.
- (instancetype)initWithMenuTag:(NSInteger)menuTag
                  sourceItemTag:(NSInteger)sourceItemTag
                  targetItemTag:(NSInteger)targetItemTag
                  keyEquivalent:(NSString*)keyEquivalent;
// Set the title using |resourceId_| and unset the source item's key equivalent.
- (void)enableForApp:(const Extension*)app;
// Restore the source item's key equivalent.
- (void)disable;
@end

@implementation DoppelgangerMenuItem

- (NSMenuItem*)menuItem {
  return _menuItem;
}

- (instancetype)initWithController:(AppShimMenuController*)controller
                           menuTag:(NSInteger)menuTag
                           itemTag:(NSInteger)itemTag
                        resourceId:(int)resourceId
                            action:(SEL)action
                     keyEquivalent:(NSString*)keyEquivalent {
  if ((self = [super init])) {
    _sourceItem = GetItemByTag(menuTag, itemTag);
    DCHECK(_sourceItem);
    _sourceKeyEquivalent = [[_sourceItem keyEquivalent] copy];
    _menuItem = [[NSMenuItem alloc] initWithTitle:[_sourceItem title]
                                           action:action
                                    keyEquivalent:keyEquivalent];
    [_menuItem setTarget:controller];
    [_menuItem setTag:itemTag];
    _resourceId = resourceId;
  }
  return self;
}

- (instancetype)initWithMenuTag:(NSInteger)menuTag
                  sourceItemTag:(NSInteger)sourceItemTag
                  targetItemTag:(NSInteger)targetItemTag
                  keyEquivalent:(NSString*)keyEquivalent {
  if ((self = [super init])) {
    _menuItem = [GetItemByTag(menuTag, targetItemTag) copy];
    _sourceItem = GetItemByTag(menuTag, sourceItemTag);
    DCHECK(_menuItem);
    DCHECK(_sourceItem);
    _sourceKeyEquivalent = [[_sourceItem keyEquivalent] copy];
  }
  return self;
}

- (void)enableForApp:(const Extension*)app {
  // It seems that two menu items that have the same key equivalent must also
  // have the same action for the keyboard shortcut to work. (This refers to the
  // original keyboard shortcut, regardless of any overrides set in OSX).
  // In order to let the app menu items have a different action, we remove the
  // key equivalent of the original items and restore them later.
  [_sourceItem setKeyEquivalent:@""];
  if (!_resourceId)
    return;

  [_menuItem setTitle:l10n_util::GetNSStringF(_resourceId,
                                              base::UTF8ToUTF16(app->name()))];
}

- (void)disable {
  // Restore the keyboard shortcut to Chrome. This just needs to be set back to
  // the original keyboard shortcut, regardless of any overrides in OSX. The
  // overrides still work as they are based on the title of the menu item.
  [_sourceItem setKeyEquivalent:_sourceKeyEquivalent];
}

@end

@interface AppShimMenuController ()
// Construct the NSMenuItems for apps.
- (void)buildAppMenuItems;
// Register for NSWindow notifications.
- (void)registerEventHandlers;
// If the window is an app window, add or remove menu items.
- (void)windowMainStatusChanged:(NSNotification*)notification;
// Called when |app| becomes the main window in the Chrome process.
- (void)appBecameMain:(const Extension*)app;
// Called when there is no main window, or if the main window is not an app.
- (void)chromeBecameMain;
// Add menu items for an app and hide Chrome menu items.
- (void)addMenuItems:(const Extension*)app;
// If the window belongs to the currently focused app, remove the menu items and
// unhide Chrome menu items.
- (void)removeMenuItems;
// If the currently focused window belongs to a platform app, quit the app.
- (void)quitCurrentPlatformApp;
// If the currently focused window belongs to a platform app, hide the app.
- (void)hideCurrentPlatformApp;
// If the currently focused window belongs to a platform app, focus the app.
- (void)focusCurrentPlatformApp;
@end

@implementation AppShimMenuController

- (instancetype)init {
  if ((self = [super init])) {
    [self buildAppMenuItems];
    [self registerEventHandlers];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)buildAppMenuItems {
  _aboutDoppelganger =
      [[DoppelgangerMenuItem alloc] initWithController:self
                                               menuTag:IDC_CHROME_MENU
                                               itemTag:IDC_ABOUT
                                            resourceId:IDS_ABOUT_MAC
                                                action:nil
                                         keyEquivalent:@""];
  _hideDoppelganger = [[DoppelgangerMenuItem alloc]
      initWithController:self
                 menuTag:IDC_CHROME_MENU
                 itemTag:IDC_HIDE_APP
              resourceId:IDS_HIDE_APP_MAC
                  action:@selector(hideCurrentPlatformApp)
           keyEquivalent:@"h"];
  _quitDoppelganger = [[DoppelgangerMenuItem alloc]
      initWithController:self
                 menuTag:IDC_CHROME_MENU
                 itemTag:IDC_EXIT
              resourceId:IDS_EXIT_MAC
                  action:@selector(quitCurrentPlatformApp)
           keyEquivalent:@"q"];
  _newDoppelganger =
      [[DoppelgangerMenuItem alloc] initWithController:self
                                               menuTag:IDC_FILE_MENU
                                               itemTag:IDC_NEW_WINDOW
                                            resourceId:0
                                                action:nil
                                         keyEquivalent:@"n"];
  // Since the "Close Window" menu item will have the same shortcut as "Close
  // Tab" on the Chrome menu, we need to create a doppelganger.
  _closeWindowDoppelganger =
      [[DoppelgangerMenuItem alloc] initWithMenuTag:IDC_FILE_MENU
                                      sourceItemTag:IDC_CLOSE_TAB
                                      targetItemTag:IDC_CLOSE_WINDOW
                                      keyEquivalent:@"w"];
  // For apps, the "Window" part of "New Window" is dropped to match the default
  // menu set given to Cocoa Apps.
  [[_newDoppelganger menuItem] setTitle:l10n_util::GetNSString(IDS_NEW_MAC)];
  _openDoppelganger =
      [[DoppelgangerMenuItem alloc] initWithController:self
                                               menuTag:IDC_FILE_MENU
                                               itemTag:IDC_OPEN_FILE
                                            resourceId:0
                                                action:nil
                                         keyEquivalent:@"o"];
  _allToFrontDoppelganger = [[DoppelgangerMenuItem alloc]
      initWithController:self
                 menuTag:IDC_WINDOW_MENU
                 itemTag:IDC_ALL_WINDOWS_FRONT
              resourceId:0
                  action:@selector(focusCurrentPlatformApp)
           keyEquivalent:@""];

  // The app's menu.
  _appMenuItem = [[NSMenuItem alloc] initWithTitle:@""
                                            action:nil
                                     keyEquivalent:@""];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@""];
  [_appMenuItem setSubmenu:appMenu];
  [appMenu setAutoenablesItems:NO];

  [appMenu addItem:[_aboutDoppelganger menuItem]];
  [[_aboutDoppelganger menuItem] setEnabled:NO];  // Not implemented yet.
  [appMenu addItem:[NSMenuItem separatorItem]];
  [appMenu addItem:[_hideDoppelganger menuItem]];
  [appMenu addItem:[NSMenuItem separatorItem]];
  [appMenu addItem:[_quitDoppelganger menuItem]];

  // File menu.
  _fileMenuItem = NewTopLevelItemFrom(IDC_FILE_MENU);
  [[_fileMenuItem submenu] addItem:[_newDoppelganger menuItem]];
  [[_fileMenuItem submenu] addItem:[_openDoppelganger menuItem]];
  [[_fileMenuItem submenu] addItem:[NSMenuItem separatorItem]];
  [[_fileMenuItem submenu] addItem:[_closeWindowDoppelganger menuItem]];

  // Edit menu. We copy the menu because the last two items, "Start Dictation"
  // and "Special Characters" are added by OSX, so we can't copy them
  // explicitly.
  _editMenuItem = [[[NSApp mainMenu] itemWithTag:IDC_EDIT_MENU] copy];

  // View menu. Remove "Always Show Bookmark Bar" and separator.
  _viewMenuItem = [[[NSApp mainMenu] itemWithTag:IDC_VIEW_MENU] copy];
  RemoveMenuItemWithTag(_viewMenuItem, IDC_SHOW_BOOKMARK_BAR, YES);

  // History menu.
  _historyMenuItem = NewTopLevelItemFrom(IDC_HISTORY_MENU);
  AddDuplicateItem(_historyMenuItem, IDC_HISTORY_MENU, IDC_BACK);
  AddDuplicateItem(_historyMenuItem, IDC_HISTORY_MENU, IDC_FORWARD);

  // Window menu.
  _windowMenuItem = NewTopLevelItemFrom(IDC_WINDOW_MENU);
  AddDuplicateItem(_windowMenuItem, IDC_WINDOW_MENU, IDC_MINIMIZE_WINDOW);
  AddDuplicateItem(_windowMenuItem, IDC_WINDOW_MENU, IDC_MAXIMIZE_WINDOW);
  [[_windowMenuItem submenu] addItem:[NSMenuItem separatorItem]];
  [[_windowMenuItem submenu] addItem:[_allToFrontDoppelganger menuItem]];
}

- (void)registerEventHandlers {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowMainStatusChanged:)
             name:NSWindowDidBecomeMainNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowMainStatusChanged:)
             name:NSWindowDidResignMainNotification
           object:nil];
}

- (void)windowMainStatusChanged:(NSNotification*)notification {
  // A Yosemite AppKit bug causes this notification to be sent during the
  // -dealloc for a specific NSWindow. Any autoreleases sent to that window
  // must be drained before the window finishes -dealloc. In this method, an
  // autorelease is sent by the invocation of [NSApp windows].
  // http://crbug.com/406944.
  base::apple::ScopedNSAutoreleasePool pool;

  NSString* name = [notification name];
  if ([name isEqualToString:NSWindowDidBecomeMainNotification]) {
    id window = [notification object];
    // Ignore is_browser: if a window becomes main that does not belong to an
    // extension or browser, treat it the same as switching to a browser.
    const Extension* extension = GetExtensionForNSWindow(window);
    if (extension)
      [self appBecameMain:extension];
    else
      [self chromeBecameMain];
  } else if ([name isEqualToString:NSWindowDidResignMainNotification]) {
    // When a window resigns main status, reset back to the Chrome menu.
    // In the past we've tried:
    // - Only doing this when a window closes, but this would not be triggered
    // when an app becomes hidden (Cmd+h), and there are no Chrome windows to
    // become main.
    // - Scanning [NSApp windows] to predict whether we could
    // expect another Chrome window to become main, and skip the reset. However,
    // panels need to do strange things during window close to ensure panels
    // never get chosen for key status over a browser window (which is likely
    // because they are given an elevated [NSWindow level]). Trying to handle
    // this case is not robust.
    //
    // Unfortunately, resetting the menu to Chrome
    // unconditionally means that if another packaged app window becomes key,
    // the menu will flicker. TODO(tapted): Investigate restoring the logic when
    // the panel code is removed.
    [self chromeBecameMain];
  } else {
    NOTREACHED();
  }
}

- (void)appBecameMain:(const Extension*)app {
  if (_appId == app->id())
    return;

  if (!_appId.empty())
    [self removeMenuItems];

  _appId = app->id();
  [self addMenuItems:app];
}

- (void)chromeBecameMain {
  if (_appId.empty())
    return;

  _appId.clear();
  [self removeMenuItems];
}

- (void)addMenuItems:(const Extension*)app {
  DCHECK_EQ(_appId, app->id());
  NSString* title = base::SysUTF8ToNSString(app->name());

  // Hide Chrome menu items.
  NSMenu* mainMenu = [NSApp mainMenu];
  for (NSMenuItem* item in [mainMenu itemArray])
    [item setHidden:YES];

  [_aboutDoppelganger enableForApp:app];
  [_hideDoppelganger enableForApp:app];
  [_quitDoppelganger enableForApp:app];
  [_newDoppelganger enableForApp:app];
  [_openDoppelganger enableForApp:app];
  [_closeWindowDoppelganger enableForApp:app];

  [_appMenuItem setTitle:base::SysUTF8ToNSString(_appId)];
  [[_appMenuItem submenu] setTitle:title];

  [mainMenu addItem:_appMenuItem];
  [mainMenu addItem:_fileMenuItem];

  SetItemWithTagVisible(_editMenuItem,
                        IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE,
                        app->is_hosted_app(), true);
  SetItemWithTagVisible(_editMenuItem, IDC_FIND_MENU, app->is_hosted_app(),
                        false);
  [mainMenu addItem:_editMenuItem];

  if (app->is_hosted_app()) {
    [mainMenu addItem:_viewMenuItem];
    [mainMenu addItem:_historyMenuItem];
  }
  [mainMenu addItem:_windowMenuItem];
}

- (void)removeMenuItems {
  NSMenu* mainMenu = [NSApp mainMenu];
  [mainMenu removeItem:_appMenuItem];
  [mainMenu removeItem:_fileMenuItem];
  if ([mainMenu indexOfItem:_viewMenuItem] >= 0)
    [mainMenu removeItem:_viewMenuItem];
  if ([mainMenu indexOfItem:_historyMenuItem] >= 0)
    [mainMenu removeItem:_historyMenuItem];
  [mainMenu removeItem:_editMenuItem];
  [mainMenu removeItem:_windowMenuItem];

  // Restore the Chrome main menu bar.
  for (NSMenuItem* item in [mainMenu itemArray])
    [item setHidden:NO];

  [_aboutDoppelganger disable];
  [_hideDoppelganger disable];
  [_quitDoppelganger disable];
  [_newDoppelganger disable];
  [_openDoppelganger disable];
  [_closeWindowDoppelganger disable];
}

- (void)quitCurrentPlatformApp {
  auto windows = GetAppWindowsForNSWindow([NSApp keyWindow]);
  for (extensions::AppWindow* window : base::Reversed(windows))
    window->GetBaseWindow()->Close();
}

- (void)hideCurrentPlatformApp {
  auto windows = GetAppWindowsForNSWindow([NSApp keyWindow]);
  for (extensions::AppWindow* window : base::Reversed(windows))
    window->GetBaseWindow()->Hide();
}

- (void)focusCurrentPlatformApp {
  auto windows = GetAppWindowsForNSWindow([NSApp keyWindow]);
  for (extensions::AppWindow* window : base::Reversed(windows))
    window->GetBaseWindow()->Show();
}

@end
