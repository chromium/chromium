// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"

#include <memory>

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#include "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "chrome/browser/ui/cocoa/applescript/metrics_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"

@interface WindowAppleScript(WindowAppleScriptPrivateMethods)
// The NSWindow that corresponds to this window.
- (NSWindow*)nativeHandle;
@end

@implementation WindowAppleScript

- (instancetype)init {
  // Check which mode to open a new window.
  NSScriptCommand* command = [NSScriptCommand currentCommand];
  NSString* mode = [command evaluatedArguments][@"KeyDictionary"][@"mode"];
  AppController* appDelegate =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);

  Profile* lastProfile = [appDelegate lastProfile];

  if (!lastProfile) {
    AppleScript::SetError(AppleScript::errGetProfile);
    return nil;
  }

  Profile* profile;
  if ([mode isEqualToString:AppleScript::kIncognitoWindowMode]) {
    profile = lastProfile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
  else if ([mode isEqualToString:AppleScript::kNormalWindowMode] || !mode) {
    profile = lastProfile;
  } else {
    // Mode cannot be anything else
    AppleScript::SetError(AppleScript::errInvalidMode);
    return nil;
  }
  // Set the mode to nil, to ensure that it is not set once more.
  [[command evaluatedArguments][@"KeyDictionary"] setValue:nil forKey:@"mode"];
  return [self initWithProfile:profile];
}

- (instancetype)initWithProfile:(Profile*)aProfile {
  if (!aProfile) {
    [self release];
    return nil;
  }

  if ((self = [super init])) {
    // Since AppleScript requests can arrive at any time, including during
    // browser shutdown or profile deletion, we have to check whether it's okay
    // to spawn a new browser for the specified profile or not.
    if (Browser::GetCreationStatusForProfile(aProfile) !=
        Browser::CreationStatus::kOk) {
      [self release];
      return nil;
    }
    _browser = Browser::Create(Browser::CreateParams(aProfile, false));
    chrome::NewTab(_browser);
    _browser->window()->Show();
    base::scoped_nsobject<NSNumber> numID(
        [[NSNumber alloc] initWithInt:_browser->session_id().id()]);
    [self setUniqueID:numID];
  }
  return self;
}

- (instancetype)initWithBrowser:(Browser*)aBrowser {
  if (!aBrowser) {
    [self release];
    return nil;
  }

  if ((self = [super init])) {
    // It is safe to be weak, if a window goes away (eg user closing a window)
    // the applescript runtime calls appleScriptWindows in
    // BrowserCrApplication and this particular window is never returned.
    _browser = aBrowser;
    base::scoped_nsobject<NSNumber> numID(
        [[NSNumber alloc] initWithInt:_browser->session_id().id()]);
    [self setUniqueID:numID];
  }
  return self;
}

- (NSWindow*)nativeHandle {
  // window() can be NULL during startup.
  if (_browser->window())
    return _browser->window()->GetNativeWindow().GetNativeNSWindow();
  return nil;
}

- (NSNumber*)activeTabIndex {
  // Note: applescript is 1-based, that is lists begin with index 1.
  int activeTabIndex = _browser->tab_strip_model()->active_index() + 1;
  if (!activeTabIndex) {
    return nil;
  }
  return @(activeTabIndex);
}

- (void)setActiveTabIndex:(NSNumber*)anActiveTabIndex {
  // Note: applescript is 1-based, that is lists begin with index 1.
  int atIndex = [anActiveTabIndex intValue] - 1;
  if (atIndex >= 0 && atIndex < _browser->tab_strip_model()->count()) {
    _browser->tab_strip_model()->ActivateTabAt(
        atIndex, TabStripUserGestureDetails(
                     TabStripUserGestureDetails::GestureType::kOther));
  } else
    AppleScript::SetError(AppleScript::errInvalidTabIndex);
}

- (NSString*)givenName {
  return base::SysUTF8ToNSString(_browser->user_title());
}

- (void)setGivenName:(NSString*)name {
  _browser->SetWindowUserTitle(base::SysNSStringToUTF8(name));
}

- (NSString*)mode {
  Profile* profile = _browser->profile();
  if (profile->IsOffTheRecord())
    return AppleScript::kIncognitoWindowMode;
  return AppleScript::kNormalWindowMode;
}

- (void)setMode:(NSString*)theMode {
  // cannot set mode after window is created.
  if (theMode) {
    AppleScript::SetError(AppleScript::errSetMode);
  }
}

- (TabAppleScript*)activeTab {
  TabAppleScript* currentTab =
      [[[TabAppleScript alloc] initWithWebContents:
          _browser->tab_strip_model()->GetActiveWebContents()] autorelease];
  [currentTab setContainer:self
                  property:AppleScript::kTabsProperty];
  return currentTab;
}

- (NSArray*)tabs {
  TabStripModel* tabStrip = _browser->tab_strip_model();
  NSMutableArray* tabs = [NSMutableArray arrayWithCapacity:tabStrip->count()];

  for (int i = 0; i < tabStrip->count(); ++i) {
    // Check to see if tab is closing.
    content::WebContents* webContents = tabStrip->GetWebContentsAt(i);
    if (webContents->IsBeingDestroyed()) {
      continue;
    }

    base::scoped_nsobject<TabAppleScript> tab(
        [[TabAppleScript alloc] initWithWebContents:webContents]);
    [tab setContainer:self
             property:AppleScript::kTabsProperty];
    [tabs addObject:tab];
  }
  return tabs;
}

- (void)insertInTabs:(TabAppleScript*)aTab {
  // This method gets called when a new tab is created so
  // the container and property are set here.
  [aTab setContainer:self
            property:AppleScript::kTabsProperty];

  // Set how long it takes a tab to be created.
  base::TimeTicks newTabStartTime = base::TimeTicks::Now();
  content::WebContents* contents = chrome::AddSelectedTabWithURL(
      _browser,
      GURL(chrome::kChromeUINewTabURL),
      ui::PAGE_TRANSITION_TYPED);
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
  core_tab_helper->set_new_tab_start_time(newTabStartTime);
  [aTab setWebContents:contents];
}

- (void)insertInTabs:(TabAppleScript*)aTab atIndex:(int)index {
  // This method gets called when a new tab is created so
  // the container and property are set here.
  [aTab setContainer:self
            property:AppleScript::kTabsProperty];

  // Set how long it takes a tab to be created.
  base::TimeTicks newTabStartTime = base::TimeTicks::Now();
  NavigateParams params(_browser, GURL(chrome::kChromeUINewTabURL),
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = index;
  Navigate(&params);
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(params.navigated_or_inserted_contents);
  core_tab_helper->set_new_tab_start_time(newTabStartTime);

  [aTab setWebContents:params.navigated_or_inserted_contents];
}

- (void)removeFromTabsAtIndex:(int)index {
  if (index < 0 || index >= _browser->tab_strip_model()->count())
    return;
  _browser->tab_strip_model()->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

- (NSNumber*)orderedIndex {
  return [NSNumber numberWithInt:[[self nativeHandle] orderedIndex]];
}

- (void)setOrderedIndex:(NSNumber*)anIndex {
  int index = [anIndex intValue] - 1;
  if (index < 0 || index >= static_cast<int>(chrome::GetTotalBrowserCount())) {
    AppleScript::SetError(AppleScript::errWrongIndex);
    return;
  }
  [[self nativeHandle] setOrderedIndex:index];
}

- (NSComparisonResult)windowComparator:(WindowAppleScript*)otherWindow {
  int thisIndex = [[self orderedIndex] intValue];
  int otherIndex = [[otherWindow orderedIndex] intValue];
  if (thisIndex < otherIndex)
    return NSOrderedAscending;
  else if (thisIndex > otherIndex)
    return NSOrderedDescending;
  // Indexes can never be same.
  NOTREACHED();
  return NSOrderedSame;
}

// Get and set values from the associated NSWindow.
- (id)valueForUndefinedKey:(NSString*)key {
  return [[self nativeHandle] valueForKey:key];
}

- (void)setValue:(id)value forUndefinedKey:(NSString*)key {
  [[self nativeHandle] setValue:value forKey:key];
}

- (void)handlesCloseScriptCommand:(NSCloseCommand*)command {
  AppleScript::LogAppleScriptUMA(AppleScript::AppleScriptCommand::WINDOW_CLOSE);

  // window() can be NULL during startup.
  if (_browser->window())
    _browser->window()->Close();
}

@end
