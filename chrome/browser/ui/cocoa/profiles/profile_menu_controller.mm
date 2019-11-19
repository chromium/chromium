// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_menu_controller.h"

#include <stddef.h>

#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"

namespace {

// Used in UMA histogram macros, shouldn't be reordered or renumbered
enum ValidateMenuItemSelector {
  UNKNOWN_SELECTOR = 0,
  NEW_PROFILE,
  EDIT_PROFILE,
  SWITCH_PROFILE_MENU,
  SWITCH_PROFILE_DOCK,
  MAX_VALIDATE_MENU_SELECTOR,
};

// Check Add Person pref.
bool IsAddPersonEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserAddPersonEnabled);
}

}  // namespace

@interface ProfileMenuController (Private)
- (void)initializeMenu;
@end

namespace ProfileMenuControllerInternal {

class Observer : public BrowserListObserver, public AvatarMenuObserver {
 public:
  Observer(ProfileMenuController* controller) : controller_(controller) {
    BrowserList::AddObserver(this);
  }

  ~Observer() override { BrowserList::RemoveObserver(this); }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {}
  void OnBrowserRemoved(Browser* browser) override {
    [controller_ activeBrowserChangedTo:chrome::GetLastActiveBrowser()];
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

@implementation ProfileMenuController

- (id)initWithMainMenuItem:(NSMenuItem*)item {
  if ((self = [super init])) {
    mainMenuItem_ = item;

    base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:
        l10n_util::GetNSStringWithFixup(IDS_PROFILES_OPTIONS_GROUP_NAME)]);
    [mainMenuItem_ setSubmenu:menu];

    // This object will be constructed as part of nib loading, which happens
    // before the message loop starts and g_browser_process is available.
    // Schedule this on the loop to do work when the browser is ready.
    [self performSelector:@selector(initializeMenu)
               withObject:nil
               afterDelay:0];
  }
  return self;
}

- (IBAction)switchToProfileFromMenu:(id)sender {
  avatarMenu_->SwitchToProfile([sender tag], false,
                               ProfileMetrics::SWITCH_PROFILE_MENU);
}

- (IBAction)switchToProfileFromDock:(id)sender {
  // Explicitly bring to the foreground when taking action from the dock.
  [NSApp activateIgnoringOtherApps:YES];
  avatarMenu_->SwitchToProfile([sender tag], false,
                               ProfileMetrics::SWITCH_PROFILE_DOCK);
}

- (IBAction)editProfile:(id)sender {
  avatarMenu_->EditProfile(avatarMenu_->GetActiveProfileIndex());
}

- (IBAction)newProfile:(id)sender {
  profiles::CreateAndSwitchToNewProfile(ProfileManager::CreateCallback(),
                                        ProfileMetrics::ADD_NEW_USER_MENU);
}

- (BOOL)insertItemsIntoMenu:(NSMenu*)menu
                   atOffset:(NSInteger)offset
                   fromDock:(BOOL)dock {
  if (!avatarMenu_)
    return NO;

  // Don't show the list of profiles in the dock if only one profile exists.
  if (dock && avatarMenu_->GetNumberOfItems() <= 1)
    return NO;

  if (dock) {
    NSString* headerName =
        l10n_util::GetNSStringWithFixup(IDS_PROFILES_OPTIONS_GROUP_NAME);
    base::scoped_nsobject<NSMenuItem> header(
        [[NSMenuItem alloc] initWithTitle:headerName
                                   action:NULL
                            keyEquivalent:@""]);
    [header setEnabled:NO];
    [menu insertItem:header atIndex:offset++];
  }

  for (size_t i = 0; i < avatarMenu_->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& itemData = avatarMenu_->GetItemAt(i);
    NSString* name = base::SysUTF16ToNSString(itemData.name);
    SEL action = dock ? @selector(switchToProfileFromDock:)
                      : @selector(switchToProfileFromMenu:);
    NSMenuItem* item = [self createItemWithTitle:name
                                          action:action];
    [item setTag:itemData.menu_index];
    if (dock) {
      [item setIndentationLevel:1];
    } else {
      gfx::Image itemIcon =
          profiles::GetAvatarIconForNSMenu(itemData.profile_path);
      [item setImage:itemIcon.ToNSImage()];
      [item setState:itemData.active ? NSOnState : NSOffState];
    }
    [menu insertItem:item atIndex:i + offset];
  }

  return YES;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  // In guest mode, chrome://settings isn't available, so disallow creating
  // or editing a profile.
  Profile* activeProfile = ProfileManager::GetLastUsedProfile();
  if (activeProfile->IsGuestSession()) {
    return [menuItem action] != @selector(newProfile:) &&
           [menuItem action] != @selector(editProfile:);
  }

  if (!IsAddPersonEnabled())
    return [menuItem action] != @selector(newProfile:);

  size_t index = avatarMenu_->GetActiveProfileIndex();
  if (avatarMenu_->GetNumberOfItems() <= index) {
    ValidateMenuItemSelector currentSelector = UNKNOWN_SELECTOR;
    if ([menuItem action] == @selector(newProfile:))
      currentSelector = NEW_PROFILE;
    else if ([menuItem action] == @selector(editProfile:))
      currentSelector = EDIT_PROFILE;
    else if ([menuItem action] == @selector(switchToProfileFromMenu:))
      currentSelector = SWITCH_PROFILE_MENU;
    else if ([menuItem action] == @selector(switchToProfileFromDock:))
      currentSelector = SWITCH_PROFILE_DOCK;
    UMA_HISTOGRAM_BOOLEAN("Profile.ValidateMenuItemInvalidIndex.IsGuest",
                          activeProfile->IsGuestSession());
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Profile.ValidateMenuItemInvalidIndex.ProfileCount",
        avatarMenu_->GetNumberOfItems(),
        1, 20, 20);
    UMA_HISTOGRAM_ENUMERATION("Profile.ValidateMenuItemInvalidIndex.Selector",
                              currentSelector,
                              MAX_VALIDATE_MENU_SELECTOR);

    return NO;
  }

  const AvatarMenu::Item& itemData = avatarMenu_->GetItemAt(index);
  if ([menuItem action] == @selector(switchToProfileFromDock:) ||
      [menuItem action] == @selector(switchToProfileFromMenu:)) {
    if (!itemData.legacy_supervised)
      return YES;

    return [menuItem tag] == static_cast<NSInteger>(itemData.menu_index);
  }

  if ([menuItem action] == @selector(newProfile:))
    return !itemData.legacy_supervised;

  return YES;
}

// Private /////////////////////////////////////////////////////////////////////

- (NSMenu*)menu {
  return [mainMenuItem_ submenu];
}

- (void)initializeMenu {
  observer_ = std::make_unique<ProfileMenuControllerInternal::Observer>(self);
  avatarMenu_ = std::make_unique<AvatarMenu>(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      observer_.get(), nullptr);
  avatarMenu_->RebuildMenu();

  [[self menu] addItem:[NSMenuItem separatorItem]];

  NSMenuItem* item = [self createItemWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_PROFILES_MANAGE_BUTTON_LABEL)
                                        action:@selector(editProfile:)];
  [[self menu] addItem:item];

  if (IsAddPersonEnabled()) {
    [[self menu] addItem:[NSMenuItem separatorItem]];

    item = [self createItemWithTitle:l10n_util::GetNSStringWithFixup(
                                         IDS_PROFILES_CREATE_NEW_PROFILE_OPTION)
                              action:@selector(newProfile:)];
    [[self menu] addItem:item];
  }

  [self rebuildMenu];
}

// Notifies the controller that the active browser has changed and that the
// menu item and menu need to be updated to reflect that.
- (void)activeBrowserChangedTo:(Browser*)browser {
  // Tell the menu that the browser has changed.
  avatarMenu_->ActiveBrowserChanged(browser);

  // If |browser| is NULL, it may be because the current profile was deleted
  // and there are no other loaded profiles. In this case, calling
  // |avatarMenu_->GetActiveProfileIndex()| may result in a profile being
  // loaded, which is inappropriate to do on the UI thread.
  //
  // An early return provides the desired behavior:
  //   a) If the profile was deleted, the menu would have been rebuilt and no
  //      profile will have a check mark.
  //   b) If the profile was not deleted, but there is no active browser, then
  //      the previous profile will remain checked.
  if (!browser)
    return;

  // Update the avatar menu to get the active item states. Don't call
  // avatarMenu_->GetActiveProfileIndex() as the index might be
  // incorrect if -activeBrowserChangedTo: is called while we deleting the
  // active profile and closing all its browser windows.
  avatarMenu_->RebuildMenu();

  // Update the state for the menu items.
  for (size_t i = 0; i < avatarMenu_->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& itemData = avatarMenu_->GetItemAt(i);
    [[[self menu] itemWithTag:itemData.menu_index]
        setState:itemData.active ? NSOnState : NSOffState];
  }
}

- (void)rebuildMenu {
  NSMenu* menu = [self menu];

  for (NSMenuItem* item = [menu itemAtIndex:0];
       ![item isSeparatorItem];
       item = [menu itemAtIndex:0]) {
    [menu removeItemAtIndex:0];
  }

  BOOL hasContent = [self insertItemsIntoMenu:menu atOffset:0 fromDock:NO];

  [mainMenuItem_ setHidden:!hasContent];
}

- (NSMenuItem*)createItemWithTitle:(NSString*)title action:(SEL)sel {
  base::scoped_nsobject<NSMenuItem> item(
      [[NSMenuItem alloc] initWithTitle:title action:sel keyEquivalent:@""]);
  [item setTarget:self];
  return [item.release() autorelease];
}

@end
