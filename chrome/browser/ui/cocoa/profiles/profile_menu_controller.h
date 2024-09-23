// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

class Browser;
class ProfileAttributesStorage;

// This controller manages the title and submenu of the Profiles item in the
// system menu bar. It updates the contents of the menu and the menu's title
// whenever the active browser changes.
@interface ProfileMenuController<NSMenuItemValidation> : NSObject

// Designated initializer. This may be called before the message loop is
// started; it will do the remainder of the work asynchronously.
- (instancetype)initWithMainMenuItem:(NSMenuItem*)item;

// Actions for the menu items.
- (IBAction)switchToProfileFromMenu:(id)sender;
- (IBAction)switchToProfileFromDock:(id)sender;
- (IBAction)editProfile:(id)sender;
- (IBAction)newProfile:(id)sender;

// If profiles are enabled and there is more than one profile, this inserts
// profile menu items into the specified menu at the specified offset and
// returns YES. Otherwise, this returns NO and does not modify the menu.
- (BOOL)insertItemsIntoMenu:(NSMenu*)menu
                   atOffset:(NSInteger)offset
                   fromDock:(BOOL)dock;

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem;

@end

@interface ProfileMenuController (PrivateExposedForTesting)

@property(readonly) NSMenu* menu;

- (instancetype)initSynchronouslyForTestingWithMainMenuItem:(NSMenuItem*)item
                                   profileAttributesStorage:
                                       (ProfileAttributesStorage*)storage;

// Clears various internal observers. Not needed for non-test code, where there
// is effectively a singleton ProfileMenuController to run the profile menu, but
// needed for test code.
- (void)deinitialize;

- (NSMenuItem*)createItemWithTitle:(NSString*)title action:(SEL)sel;

- (void)activeBrowserChangedTo:(Browser*)browser;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_MENU_CONTROLLER_H_
