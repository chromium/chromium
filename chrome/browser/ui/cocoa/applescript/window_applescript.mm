// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"

#import <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

#include <algorithm>
#include <map>
#include <memory>

#import "base/apple/foundation_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/cocoa/applescript/apple_event_util.h"
#include "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#include "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/waap/window_creation.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"

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
  base::WeakPtr<BrowserWindowInterface> _browser;
}

- (instancetype)init {
  // Check which mode to open a new window.
  NSScriptCommand* command = AppleScript::GetCurrentScriptCommand();
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
  CHECK(aProfile);

  // Since AppleScript requests can arrive at any time, including during browser
  // shutdown or profile deletion, we have to check whether it's okay to spawn a
  // new browser for the specified profile or not.
  if (GetBrowserWindowCreationStatusForProfile(*aProfile) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    self = nil;
    return nil;
  }

  BrowserWindowInterface* browser =
      waap::CreateBrowserWindowAndRecordCreationSource(
          BrowserWindowCreateParams(aProfile, /*from_user_gesture=*/false),
          waap::NewWindowCreationSource::kNewEmptyWindow);
  // TODO(crbug.com/452431839): Make a new NewTabTypes enum value for new tabs
  // made with AppleScript requests.
  chrome::NewTab(browser, NewTabTypes::kNewTabCommand);
  browser->GetWindow()->Show();

  return [self initWithBrowser:browser];
}

- (instancetype)initWithBrowser:(BrowserWindowInterface*)browser {
  CHECK(browser);

  if ((self = [super init])) {
    // It is safe to be weak, if a window goes away (eg user closing a window)
    // the AppleScript runtime calls appleScriptWindows in
    // BrowserCrApplication and this particular window is never returned.
    _browser = browser->GetWeakPtr();
    self.uniqueID =
        [NSString stringWithFormat:@"%d", _browser->GetSessionID().id()];
    [self setContainer:NSApp property:AppleScript::kWindowsProperty];
  }
  return self;
}

- (NSWindow*)nativeHandle {
  if (!_browser) {
    return nil;
  }

  // GetWindow() can return null during startup.
  if (_browser->GetWindow()) {
    return _browser->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  }
  return nil;
}

- (NSNumber*)activeTabIndex {
  if (!_browser) {
    return nil;
  }

  int activeTabIndex = _browser->GetTabStripModel()->active_index();
  if (activeTabIndex == TabStripModel::kNoTab) {
    return nil;
  }
  // Note: AppleScript is 1-based, that is lists begin with index 1.
  return @(activeTabIndex + 1);
}

- (void)setActiveTabIndex:(NSNumber*)anActiveTabIndex {
  if (!_browser) {
    return;
  }
  TabStripModel* tabStrip = _browser->GetTabStripModel();

  // Note: AppleScript is 1-based, that is lists begin with index 1.
  int atIndex = anActiveTabIndex.intValue - 1;
  if (atIndex >= 0 && atIndex < tabStrip->count()) {
    tabStrip->ActivateTabAt(
        atIndex, TabStripUserGestureDetails(
                     TabStripUserGestureDetails::GestureType::kOther));
  } else {
    AppleScript::SetError(AppleScript::Error::kInvalidTab);
  }
}

- (NSString*)givenName {
  if (!_browser) {
    return nil;
  }

  return base::SysUTF8ToNSString(
      WindowMetadataController::From(_browser.get())->user_title());
}

- (void)setGivenName:(NSString*)name {
  if (!_browser) {
    return;
  }

  WindowMetadataController::From(_browser.get())
      ->SetWindowUserTitle(base::SysNSStringToUTF8(name));
}

- (NSString*)mode {
  if (!_browser) {
    return nil;
  }

  Profile* profile = _browser->GetProfile();
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
      initWithWebContents:_browser->GetTabStripModel()->GetActiveWebContents()];
  [currentTab setContainer:self property:AppleScript::kTabsProperty];
  return currentTab;
}

- (void)setActiveTab:(TabAppleScript*)aTab {
  if (!_browser) {
    return;
  }
  TabStripModel* tabStrip = _browser->GetTabStripModel();

  int tabIndex = tabStrip->GetIndexOfWebContents(aTab.webContents);
  if (tabIndex != TabStripModel::kNoTab) {
    tabStrip->ActivateTabAt(
        tabIndex, TabStripUserGestureDetails(
                      TabStripUserGestureDetails::GestureType::kOther));
  } else {
    AppleScript::SetError(AppleScript::Error::kInvalidTab);
  }
}

- (NSArray<TabAppleScript*>*)tabs {
  if (!_browser) {
    return nil;
  }

  TabStripModel* tabStrip = _browser->GetTabStripModel();
  NSMutableArray* tabs = [NSMutableArray arrayWithCapacity:tabStrip->count()];

  for (int i = 0; i < tabStrip->count(); ++i) {
    // Check to see if tab is closing.
    content::WebContents* webContents = tabStrip->GetWebContentsAt(i);
    if (webContents->IsBeingDestroyed()) {
      continue;
    }

    TabAppleScript* tab =
        [[TabAppleScript alloc] initWithWebContents:webContents];
    [tab setContainer:self property:AppleScript::kTabsProperty];
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

  content::WebContents* contents = chrome::AddSelectedTabWithURL(
      _browser.get(), GURL(chrome::kChromeUINewTabURL),
      ui::PAGE_TRANSITION_TYPED);
  [aTab setWebContents:contents];
}

- (void)insertInTabs:(TabAppleScript*)aTab atIndex:(int)index {
  if (!_browser) {
    return;
  }

  // This method gets called when a new tab is created so
  // the container and property are set here.
  [aTab setContainer:self property:AppleScript::kTabsProperty];

  NavigateParams params(_browser.get(), GURL(chrome::kChromeUINewTabURL),
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = ui::mojom::WindowOpenDisposition::kNewForegroundTab;
  params.tabstrip_index = index;
  Navigate(&params);

  [aTab setWebContents:params.navigated_or_inserted_contents];
}

- (void)removeFromTabsAtIndex:(int)index {
  if (!_browser) {
    return;
  }

  if (index < 0 || index >= _browser->GetTabStripModel()->count()) {
    return;
  }
  _browser->GetTabStripModel()->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

- (NSNumber*)orderedIndex {
  return @(self.nativeHandle.orderedIndex);
}

- (void)setOrderedIndex:(NSNumber*)anIndex {
  int index = anIndex.intValue - 1;
  if (index < 0 ||
      index >=
          static_cast<int>(GlobalBrowserCollection::GetInstance()->GetSize())) {
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

  // GetWindow() can return null during startup.
  if (_browser->GetWindow()) {
    _browser->GetWindow()->Close();
  }
}

@end

namespace AppleScript {

NSArray<WindowAppleScript*>* GetWindows() {
  std::map<NSWindow*, BrowserWindowInterface*> browsers;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->capabilities()->IsAttemptingToCloseBrowser()) {
          return true;
        }

        browsers.emplace(
            browser->GetWindow()->GetNativeWindow().GetNativeNSWindow(),
            browser);
        return true;
      });

  NSMutableArray<WindowAppleScript*>* result = [NSMutableArray array];
  for (NSWindow* window in NSApp.orderedWindows) {
    const auto& browser_it = browsers.find(window);
    if (browser_it == browsers.end()) {
      continue;
    }

    WindowAppleScript* a_window =
        [[WindowAppleScript alloc] initWithBrowser:browser_it->second];
    [result addObject:a_window];
  }

  return result;
}

void InsertWindowAtIndex(WindowAppleScript* window, int index) {
  if (!window || index < 0) {
    AppleScript::SetError(AppleScript::Error::kWrongIndex);
    return;
  }

  NSWindow* target_native_window = window.nativeHandle;
  NSArray<WindowAppleScript*>* container_windows = GetWindows();
  NSMutableArray<NSWindow*>* other_windows = [NSMutableArray array];
  for (WindowAppleScript* existing in container_windows) {
    NSWindow* native_window = existing.nativeHandle;
    if (native_window && native_window != target_native_window) {
      [other_windows addObject:native_window];
    }
  }

  if (index > static_cast<int>(other_windows.count)) {
    AppleScript::SetError(AppleScript::Error::kWrongIndex);
    return;
  }

  if (!target_native_window || other_windows.count == 0) {
    return;
  }

  if (index == 0) {
    NSInteger first_window_number = other_windows[0].windowNumber;
    [target_native_window orderWindow:NSWindowAbove
                           relativeTo:first_window_number];
  } else {
    NSInteger anchor_window_number = other_windows[index - 1].windowNumber;
    [target_native_window orderWindow:NSWindowBelow
                           relativeTo:anchor_window_number];
    // At this point, while -[NSApp orderedWindows] returns the new window
    // order, the order is not reflected on the screen. It seems that doing this
    // window order manipulation from within an AppleScript handler does not
    // work. One approach might be to post the window order manipulation to the
    // main thread for processing after the handler ends, but that would then
    // create a new issue for subsequent window manipulations from the same
    // AppleScript, where one window's positioning would not be reflected in the
    // next.
    //
    // Looking at -[NSWindow(NSScripting) setOrderedIndex:] which is used by the
    // stock AppleScript handlers, after calling -orderWindow:relativeTo:, they
    // then force the front window to be key. That seems to make the window
    // order take effect, but has the unfortunate side effect of activating and
    // ordering the front window to the global front whether or not Chromium is
    // active itself.
    //
    // Use that workaround for now; it matches the (unfortunate) built-in
    // behavior. Perhaps another, better, way can be discovered.
    [NSApp.orderedWindows.firstObject makeKeyWindow];
  }
}

}  // namespace AppleScript
