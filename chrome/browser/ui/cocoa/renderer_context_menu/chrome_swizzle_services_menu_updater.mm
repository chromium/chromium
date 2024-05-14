// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/chrome_swizzle_services_menu_updater.h"

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#import "chrome/browser/mac/nsprocessinfo_additions.h"

namespace {

base::apple::ScopedObjCClassSwizzler* g_populatemenu_swizzler = nullptr;

// |g_filtered_entries_array| is only set during testing (see
// +[ChromeSwizzleServicesMenuUpdater storeFilteredEntriesForTestingInArray:]).
// Otherwise it remains nil.
NSMutableArray* g_filtered_entries_array = nil;

}  // namespace

// An AppKit-private class that adds Services items to contextual menus and
// the application Services menu.
@interface _NSServicesMenuUpdater : NSObject
- (void)populateMenu:(NSMenu*)menu
    withServiceEntries:(NSArray*)entries
            forDisplay:(BOOL)display;
@end

// An AppKit-private class representing a Services menu entry.
@interface _NSServiceEntry : NSObject
- (NSString*)bundleIdentifier;
@end

@implementation ChromeSwizzleServicesMenuUpdater

- (void)populateMenu:(NSMenu*)menu
    withServiceEntries:(NSArray*)entries
            forDisplay:(BOOL)display {
  NSMutableArray* remainingEntries = [NSMutableArray array];
  [g_filtered_entries_array removeAllObjects];

  // Remove some services.
  //   - Remove the ones from Safari, as they are redundant to the ones provided
  //     by Chromium, and confusing to the user due to them switching apps
  //     upon their selection.
  //   - Remove the "Open URL" one provided by SystemUIServer, as it is
  //     redundant to the one provided by Chromium and has other serious issues.
  //     (https://crbug.com/960209)

  for (_NSServiceEntry* nextEntry in entries) {
    NSString* bundleIdentifier = [nextEntry bundleIdentifier];
    NSString* message = [nextEntry valueForKey:@"message"];
    bool shouldRemove =
        ([bundleIdentifier isEqualToString:@"com.apple.Safari"]) ||
        ([bundleIdentifier isEqualToString:@"com.apple.systemuiserver"] &&
         [message isEqualToString:@"openURL"]);

    if (!shouldRemove) {
      [remainingEntries addObject:nextEntry];
    } else {
      [g_filtered_entries_array addObject:nextEntry];
    }
  }

  // Pass the filtered array along to the _NSServicesMenuUpdater.
  g_populatemenu_swizzler->InvokeOriginal<void, NSMenu*, NSArray*, BOOL>(
      self, _cmd, menu, remainingEntries, display);
}

+ (void)storeFilteredEntriesForTestingInArray:(NSMutableArray*)array {
  g_filtered_entries_array = array;
}

+ (void)install {
  // Swizzling should not happen in renderer processes.
  CHECK([[NSProcessInfo processInfo] cr_isMainBrowserOrTestProcess]);

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    // Confirm that the AppKit's private _NSServiceEntry class exists. This
    // class cannot be accessed at link time and is not guaranteed to exist in
    // past or future AppKits so use NSClassFromString() to locate it. Also
    // check that the class implements the bundleIdentifier method. The browser
    // test checks for all of this as well, but the checks here ensure that we
    // don't crash out in the wild when running on some future version of OS X.
    // Odds are a developer will be running a newer version of OS X sooner than
    // the bots - NOTREACHED() will get them to tell us if compatibility breaks.
    if (![NSClassFromString(@"_NSServiceEntry")
            instancesRespondToSelector:@selector(bundleIdentifier)]) {
      NOTREACHED_IN_MIGRATION();
      return;
    }

    // Perform similar checks on the AppKit's private _NSServicesMenuUpdater
    // class.
    SEL targetSelector = @selector(populateMenu:withServiceEntries:forDisplay:);
    Class targetClass = NSClassFromString(@"_NSServicesMenuUpdater");
    if (![targetClass instancesRespondToSelector:targetSelector]) {
      NOTREACHED_IN_MIGRATION();
      return;
    }

    // Replace the populateMenu:withServiceEntries:forDisplay: method in
    // _NSServicesMenuUpdater with an implementation that can filter Services
    // menu entries from contextual menus and elsewhere. Place the swizzler into
    // a static so that it never goes out of scope, because the scoper's
    // destructor undoes the swizzling.
    Class swizzleClass = [ChromeSwizzleServicesMenuUpdater class];
    static base::NoDestructor<base::apple::ScopedObjCClassSwizzler>
        servicesMenuFilter(targetClass, swizzleClass, targetSelector);
    g_populatemenu_swizzler = servicesMenuFilter.get();
  });
}

@end
