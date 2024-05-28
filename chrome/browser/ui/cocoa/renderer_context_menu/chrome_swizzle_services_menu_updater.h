// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_CHROME_SWIZZLE_SERVICES_MENU_UPDATER_H_
#define CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_CHROME_SWIZZLE_SERVICES_MENU_UPDATER_H_

#import <Cocoa/Cocoa.h>

// The ChromeSwizzleServicesMenuUpdater filters Services menu items in the
// contextual menus and elsewhere using swizzling.
@interface ChromeSwizzleServicesMenuUpdater : NSObject
// Installs the swizzler. Only does something the first time it is called.
+ (void)install;

// Return filtered entries, for testing.
+ (void)storeFilteredEntriesForTestingInArray:(NSMutableArray*)array;
@end

#endif  // CHROME_BROWSER_UI_COCOA_RENDERER_CONTEXT_MENU_CHROME_SWIZZLE_SERVICES_MENU_UPDATER_H_
