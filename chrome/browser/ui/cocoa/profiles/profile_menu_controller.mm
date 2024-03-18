// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_menu_controller.h"

#include <AppKit/AppKit.h>
#include <stddef.h>

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"

namespace {

NSString* GetProfileMenuTitle() {
  return l10n_util::GetNSStringWithFixup(IDS_PROFILES_MENU_NAME);
}

}  // namespace

@interface ProfileMenuController (Private)
- (void)initializeMenuWithProfileAttributesStorage:
    (ProfileAttributesStorage*)storage;
- (void)rebuildMenu;
@end

namespace ProfileMenuControllerInternal {

class Observer : public BrowserListObserver, public AvatarMenuObserver {
 public:
  explicit Observer(ProfileMenuController* controller)
      : controller_(controller) {
    BrowserList::AddObserver(this);
  }

  ~Observer() override { BrowserList::RemoveObserver(this); }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {}
  void OnBrowserRemoved(Browser* browser) override {
    [controller_ activeBrowserChangedTo:chrome::FindLastActive()];
  }
  void OnBrowserSetLastActive(Browser* browser) override {
    [controller_ activeBrowserChangedTo:browser];
  }

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* menu) override {
    [controller_ rebuildMenu];
  }

 private:
  ProfileMenuController* controller_;  // Weak; owns this.
};

}  // namespace ProfileMenuControllerInternal

////////////////////////////////////////////////////////////////////////////////

@implementation ProfileMenuController {
  // An observer to be notified when the active browser changes and when the
  // menu model changes.
  std::unique_ptr<ProfileMenuControllerInternal::Observer> _observer;

  // The controller for the profile submenu.
  std::unique_ptr<AvatarMenu> _avatarMenu;

  // The main menu item to which the profile menu is attached.
  NSMenuItem* __strong _mainMenuItem;
}

- (instancetype)initWithMainMenuItem:(NSMenuItem*)item
            profileAttributesStorage:(ProfileAttributesStorage*)storage {
  if ((self = [super init])) {
    _mainMenuItem = item;

    _mainMenuItem.submenu =
        [[NSMenu alloc] initWithTitle:GetProfileMenuTitle()];

    // When this object is constructed in non-test code, right after the main
    // menu is created, that happens before the message loop starts and thus
    // `g_browser_process` is not yet available. In that case, schedule
    // initialization on the loop to do work when the browser is ready. For test
    // code, the required object is available, so initialize immediately to
    // allow test code to avoid loop spinning calls, which could cause
    // flakiness.

    if (storage) {
      [self initializeMenuWithProfileAttributesStorage:storage];
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        [self initializeMenuWithProfileAttributesStorage:
                  &g_browser_process->profile_manager()
                       ->GetProfileAttributesStorage()];
      });
    }
  }
  return self;
}

- (instancetype)initWithMainMenuItem:(NSMenuItem*)item {
  return [self initWithMainMenuItem:item profileAttributesStorage:nullptr];
}

- (instancetype)initSynchronouslyForTestingWithMainMenuItem:(NSMenuItem*)item
                                   profileAttributesStorage:
                                       (ProfileAttributesStorage*)storage {
  return [self initWithMainMenuItem:item profileAttributesStorage:storage];
}

- (void)deinitialize {
  _avatarMenu.reset();
  _observer.reset();
}

- (IBAction)switchToProfileFromMenu:(id)sender {
  _avatarMenu->SwitchToProfile([sender tag], false);
}

- (IBAction)switchToProfileFromDock:(id)sender {
  // Explicitly bring to the foreground when taking action from the dock.
  [NSApp activateIgnoringOtherApps:YES];
  _avatarMenu->SwitchToProfile([sender tag], false);
}

- (IBAction)editProfile:(id)sender {
  std::optional<size_t> active_profile_index =
      _avatarMenu->GetActiveProfileIndex();
  DCHECK(active_profile_index);
  _avatarMenu->EditProfile(*active_profile_index);
}

- (IBAction)newProfile:(id)sender {
  _avatarMenu->AddNewProfile();
}

- (BOOL)insertItemsIntoMenu:(NSMenu*)menu
                   atOffset:(NSInteger)offset
                   fromDock:(BOOL)dock {
  if (!_avatarMenu) {
    return NO;
  }

  // Don't show the list of profiles in the dock if only one profile exists.
  if (dock && _avatarMenu->GetNumberOfItems() <= 1) {
    return NO;
  }

  if (dock) {
    NSMenuItem* header;
    if (@available(macOS 14, *)) {
      header = [NSMenuItem sectionHeaderWithTitle:GetProfileMenuTitle()];
    } else {
      header = [[NSMenuItem alloc] initWithTitle:GetProfileMenuTitle()
                                          action:nil
                                   keyEquivalent:@""];
    }
    header.enabled = NO;
    [menu insertItem:header atIndex:offset++];
  }

  for (size_t i = 0; i < _avatarMenu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& itemData = _avatarMenu->GetItemAt(i);
    NSString* name = base::SysUTF16ToNSString(itemData.name);
    SEL action = dock ? @selector(switchToProfileFromDock:)
                      : @selector(switchToProfileFromMenu:);
    NSMenuItem* item = [self createItemWithTitle:name action:action];
    item.tag = itemData.menu_index;
    if (!dock) {
      gfx::Image itemIcon =
          profiles::GetAvatarIconForNSMenu(itemData.profile_path);
      item.image = itemIcon.ToNSImage();
      item.state =
          itemData.active ? NSControlStateValueOn : NSControlStateValueOff;
    }
    [menu insertItem:item atIndex:i + offset];
  }

  return YES;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  if (!_avatarMenu->ShouldShowAddNewProfileLink() &&
      menuItem.action == @selector(newProfile:)) {
    return NO;
  }

  if (!_avatarMenu->ShouldShowEditProfileLink() && menuItem.action == @selector
                                                       (editProfile:)) {
    return NO;
  }

  return YES;
}

// Private /////////////////////////////////////////////////////////////////////

- (NSMenu*)menu {
  return _mainMenuItem.submenu;
}

- (void)initializeMenuWithProfileAttributesStorage:
    (ProfileAttributesStorage*)storage {
  _observer = std::make_unique<ProfileMenuControllerInternal::Observer>(self);
  _avatarMenu = std::make_unique<AvatarMenu>(storage, _observer.get(),
                                             /*browser=*/nullptr);
  _avatarMenu->RebuildMenu();

  [self.menu addItem:[NSMenuItem separatorItem]];

  NSMenuItem* item = [self createItemWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_PROFILES_MANAGE_BUTTON_LABEL)
                                        action:@selector(editProfile:)];
  [self.menu addItem:item];

  if (_avatarMenu->ShouldShowAddNewProfileLink()) {
    [self.menu addItem:[NSMenuItem separatorItem]];

    item = [self createItemWithTitle:l10n_util::GetNSStringWithFixup(
                                         IDS_PROFILES_ADD_PROFILE_LABEL)
                              action:@selector(newProfile:)];
    [self.menu addItem:item];
  }

  [self rebuildMenu];
}

// Notifies the controller that the active browser has changed and that the
// menu item and menu need to be updated to reflect that.
- (void)activeBrowserChangedTo:(Browser*)browser {
  // Tell the menu that the browser has changed.
  _avatarMenu->ActiveBrowserChanged(browser);

  // If |browser| is NULL, it may be because the current profile was deleted
  // and there are no other loaded profiles.
  //
  // An early return provides the desired behavior:
  //   a) If the profile was deleted, the menu would have been rebuilt and no
  //      profile will have a check mark.
  //   b) If the profile was not deleted, but there is no active browser, then
  //      the previous profile will remain checked.
  if (!browser) {
    return;
  }

  // Update the avatar menu to get the active item states. Don't call
  // avatarMenu_->GetActiveProfileIndex() as the index might be
  // incorrect if -activeBrowserChangedTo: is called while we deleting the
  // active profile and closing all its browser windows.
  _avatarMenu->RebuildMenu();

  // Update the state for the menu items.
  for (size_t i = 0; i < _avatarMenu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& itemData = _avatarMenu->GetItemAt(i);
    [[self.menu itemWithTag:itemData.menu_index]
        setState:itemData.active ? NSControlStateValueOn
                                 : NSControlStateValueOff];
  }
}

- (void)rebuildMenu {
  NSMenu* menu = self.menu;

  for (NSMenuItem* item = [menu itemAtIndex:0]; !item.separatorItem;
       item = [menu itemAtIndex:0]) {
    [menu removeItemAtIndex:0];
  }

  BOOL hasContent = [self insertItemsIntoMenu:menu atOffset:0 fromDock:NO];

  _mainMenuItem.hidden = !hasContent;
}

- (NSMenuItem*)createItemWithTitle:(NSString*)title action:(SEL)sel {
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                action:sel
                                         keyEquivalent:@""];
  item.target = self;
  return item;
}

@end
