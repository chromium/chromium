// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"

#include <memory>

#import "base/apple/foundation_util.h"
#include "base/memory/weak_ptr.h"
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
#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"

@interface WindowAppleScript ()

// The NSWindow that corresponds to this window.
@property(readonly) NSWindow* nativeHandle;

@end

@implementation WindowAppleScript {
  // A note about lifetimes: It's not expected that this object will ever be
  // deleted behind the back of this class. AppleScript does not hold onto
  // objects between script runs; it will retain the object specifier, and if
  // needed again, AppleScript will re-iterate over the objects, and look for
  // the specified object. However, there's no hard guarantee that a race
  // couldn't be made to happen, and in tests things are torn down at odd times,
  // so it's best to use a real weak pointer.
  base::WeakPtr<Browser> _browser;
}

- (instancetype)init {
  // Check which mode to open a new window.
  NSScriptCommand* command = [NSScriptCommand currentCommand];
  NSString* mode = command.evaluatedArguments[@"KeyDictionary"][@"mode"];

  Profile* lastProfile = AppController.sharedController.lastProfile;
  if (!lastProfile) {
    AppleScript::SetError(AppleScript::Error::kGetProfile);
    return nil;
  } else {
    // Ensure that the profile is a non-OTR profile, so that it's possible to
    // create a non-OTR window, below.
    lastProfile = lastProfile->GetOriginalProfile();
  }

  Profile* profile;
  if ([mode isEqualToString:AppleScript::kIncognitoWindowMode]) {
    profile = lastProfile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  } else if ([mode isEqualToString:AppleScript::kNormalWindowMode] || !mode) {
    profile = lastProfile;
  } else {
    // Mode cannot be anything else.
    AppleScript::SetError(AppleScript::Error::kInvalidMode);
    return nil;
  }
  // Set the mode to nil, to ensure that it is not set once more.
  [command.evaluatedArguments[@"KeyDictionary"] setValue:nil forKey:@"mode"];
  return [self initWithProfile:profile];
}

- (instancetype)initWithProfile:(Profile*)aProfile {
  if (!aProfile) {
    self = nil;
    return nil;
  }

  if ((self = [super init])) {
    // Since AppleScript requests can arrive at any time, including during
    // browser shutdown or profile deletion, we have to check whether it's okay
    // to spawn a new browser for the specified profile or not.
    if (Browser::GetCreationStatusForProfile(aProfile) !=
        Browser::CreationStatus::kOk) {
      self = nil;
      return nil;
    }

    Browser* browser = Browser::Create(
        Browser::CreateParams(aProfile, /*user_gesture=*/false));
    chrome::NewTab(browser);
    browser->window()->Show();

    _browser = browser->AsWeakPtr();
    self.uniqueID =
        [NSString stringWithFormat:@"%d", _browser->session_id().id()];
  }
  return self;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  if (!browser) {
    self = nil;
    return nil;
  }

  if ((self = [super init])) {
    // It is safe to be weak, if a window goes away (eg user closing a window)
    // the AppleScript runtime calls appleScriptWindows in
    // BrowserCrApplication and this particular window is never returned.
    _browser = browser->AsWeakPtr();
    self.uniqueID =
        [NSString stringWithFormat:@"%d", _browser->session_id().id()];
  }
  return self;
}

- (NSWindow*)nativeHandle {
  if (!_browser) {
    return nil;
  }

  // window() can be null during startup.
  if (_browser->window()) {
    return _browser->window()->GetNativeWindow().GetNativeNSWindow();
  }
  return nil;
}

- (NSNumber*)activeTabIndex {
  if (!_browser) {
    return nil;
  }

  // Note: AppleScript is 1-based, that is lists begin with index 1.
  int activeTabIndex = _browser->tab_strip_model()->active_index() + 1;
  if (!activeTabIndex) {
    return nil;
  }
  return @(activeTabIndex);
}

- (void)setActiveTabIndex:(NSNumber*)anActiveTabIndex {
  if (!_browser) {
    return;
  }

  // Note: AppleScript is 1-based, that is lists begin with index 1.
  int atIndex = anActiveTabIndex.intValue - 1;
  if (atIndex >= 0 && atIndex < _browser->tab_strip_model()->count()) {
    _browser->tab_strip_model()->ActivateTabAt(
        atIndex, TabStripUserGestureDetails(
                     TabStripUserGestureDetails::GestureType::kOther));
  } else {
    AppleScript::SetError(AppleScript::Error::kInvalidTabIndex);
  }
}

- (NSString*)givenName {
  if (!_browser) {
    return nil;
  }

  return base::SysUTF8ToNSString(_browser->user_title());
}

- (void)setGivenName:(NSString*)name {
  if (!_browser) {
    return;
  }

  _browser->SetWindowUserTitle(base::SysNSStringToUTF8(name));
}

- (NSString*)mode {
  if (!_browser) {
    return nil;
  }

  Profile* profile = _browser->profile();
  if (profile->IsOffTheRecord()) {
    return AppleScript::kIncognitoWindowMode;
  }
  return AppleScript::kNormalWindowMode;
}

- (void)setMode:(NSString*)theMode {
  // Cannot set mode after window is created.
  if (theMode) {
    AppleScript::SetError(AppleScript::Error::kSetMode);
  }
}

- (TabAppleScript*)activeTab {
  if (!_browser) {
    return nil;
  }

  TabAppleScript* currentTab = [[TabAppleScript alloc]
      initWithWebContents:_browser->tab_strip_model()->GetActiveWebContents()];
  [currentTab setContainer:self property:AppleScript::kTabsProperty];
  return currentTab;
}

- (NSArray<TabAppleScript*>*)tabs {
  if (!_browser) {
    return nil;
  }

  TabStripModel* tabStrip = _browser->tab_strip_model();
  NSMutableArray* tabs = [NSMutableArray arrayWithCapacity:tabStrip->count()];

  for (int i = 0; i < tabStrip->count(); ++i) {
    // Check to see if tab is closing.
    content::WebContents* webContents = tabStrip->GetWebContentsAt(i);
    if (webContents->IsBeingDestroyed()) {
      continue;
    }

    TabAppleScript* tab =
        [[TabAppleScript alloc] initWithWebContents:webContents];
    [tab setContainer:self
             property:AppleScript::kTabsProperty];
    [tabs addObject:tab];
  }
  return tabs;
}

- (void)insertInTabs:(TabAppleScript*)aTab {
  if (!_browser) {
    return;
  }

  // This method gets called when a new tab is created so
  // the container and property are set here.
  [aTab setContainer:self property:AppleScript::kTabsProperty];

  // Set how long it takes a tab to be created.
  base::TimeTicks newTabStartTime = base::TimeTicks::Now();
  content::WebContents* contents = chrome::AddSelectedTabWithURL(
      _browser.get(), GURL(chrome::kChromeUINewTabURL),
      ui::PAGE_TRANSITION_TYPED);
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
  core_tab_helper->set_new_tab_start_time(newTabStartTime);
  [aTab setWebContents:contents];
}

- (void)insertInTabs:(TabAppleScript*)aTab atIndex:(int)index {
  if (!_browser) {
    return;
  }

  // This method gets called when a new tab is created so
  // the container and property are set here.
  [aTab setContainer:self property:AppleScript::kTabsProperty];

  // Set how long it takes a tab to be created.
  base::TimeTicks newTabStartTime = base::TimeTicks::Now();
  NavigateParams params(_browser.get(), GURL(chrome::kChromeUINewTabURL),
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
  if (!_browser) {
    return;
  }

  if (index < 0 || index >= _browser->tab_strip_model()->count()) {
    return;
  }
  _browser->tab_strip_model()->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

- (NSNumber*)orderedIndex {
  return @(self.nativeHandle.orderedIndex);
}

- (void)setOrderedIndex:(NSNumber*)anIndex {
  int index = anIndex.intValue - 1;
  if (index < 0 || index >= static_cast<int>(chrome::GetTotalBrowserCount())) {
    AppleScript::SetError(AppleScript::Error::kWrongIndex);
    return;
  }
  self.nativeHandle.orderedIndex = index;
}

// Get and set values from the associated NSWindow.
- (id)valueForUndefinedKey:(NSString*)key {
  return [self.nativeHandle valueForKey:key];
}

- (void)setValue:(id)value forUndefinedKey:(NSString*)key {
  [self.nativeHandle setValue:value forKey:key];
}

- (void)handlesCloseScriptCommand:(NSCloseCommand*)command {
  if (!_browser) {
    return;
  }

  // window() can be null during startup.
  if (_browser->window()) {
    _browser->window()->Close();
  }
}

@end
