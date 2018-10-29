// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_command_handler.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

void SetToggleState(bool toggled, id item) {
  NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item);
  NSButton* buttonItem = base::mac::ObjCCast<NSButton>(item);
  if (menuItem) {
    NSInteger old_state = [menuItem state];
    NSInteger new_state = toggled ? NSOnState : NSOffState;
    if (old_state != new_state)
      [menuItem setState:new_state];
  } else if (buttonItem) {
    NSInteger old_state = [buttonItem state];
    NSInteger new_state = toggled ? NSOnState : NSOffState;
    if (old_state != new_state)
      [buttonItem setState:new_state];
  }
}

// Update a toggle state for an item if modified. The item may be an NSMenuItem
// or NSButton. Called by -validateUserInterfaceItem:.
void UpdateToggleStateWithTag(NSInteger tag, id item, NSWindow* window) {
  if (!base::mac::ObjCCast<NSMenuItem>(item) &&
      !base::mac::ObjCCast<NSButton>(item))
    return;

  Browser* browser = chrome::FindBrowserWithWindow(window);
  DCHECK(browser);

  // On Windows this logic happens in bookmark_bar_view.cc. This simply updates
  // the menu item; it does not display the bookmark bar itself.
  if (tag == IDC_SHOW_BOOKMARK_BAR) {
    PrefService* prefs = browser->profile()->GetPrefs();
    SetToggleState(prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar), item);
    return;
  }

  if (tag == IDC_TOGGLE_FULLSCREEN_TOOLBAR) {
    PrefService* prefs = browser->profile()->GetPrefs();
    SetToggleState(prefs->GetBoolean(prefs::kShowFullscreenToolbar), item);
    return;
  }

  if (tag == IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS) {
    PrefService* prefs = browser->profile()->GetPrefs();
    SetToggleState(prefs->GetBoolean(prefs::kAllowJavascriptAppleEvents), item);
    return;
  }

  if (tag == IDC_WINDOW_MUTE_SITE) {
    TabStripModel* model = browser->tab_strip_model();
    bool will_mute =
        base::FeatureList::IsEnabled(features::kSoundContentSetting)
            ? model->WillContextMenuMuteSites(model->active_index())
            : model->WillContextMenuMute(model->active_index());
    // Menu items may be validated during browser startup, before the
    // TabStripModel has been populated.
    SetToggleState(!model->empty() && !will_mute, item);
    return;
  }

  if (tag == IDC_WINDOW_PIN_TAB) {
    TabStripModel* model = browser->tab_strip_model();
    SetToggleState(
        !model->empty() && !model->WillContextMenuPin(model->active_index()),
        item);
    return;
  }
}

NSString* GetTitleForViewsFullscreenMenuItem(Browser* browser) {
  return l10n_util::GetNSString(browser->window()->IsFullscreen()
                                    ? IDS_EXIT_FULLSCREEN_MAC
                                    : IDS_ENTER_FULLSCREEN_MAC);
}

// Get the text for the "Enter/Exit Fullscreen" menu item.
// TODO(jackhou): Remove the dependency on BrowserWindowController(Private).
NSString* GetTitleForFullscreenMenuItem(Browser* browser) {
  return GetTitleForViewsFullscreenMenuItem(browser);
}

// Identify the actual Browser to which the command should be dispatched. It
// might belong to a background window, yet another dispatcher gets it because
// it is the foreground window's dispatcher and thus in the responder chain.
// Some senders don't have this problem (for example, menus only operate on the
// foreground window), so this is only an issue for senders that are part of
// windows.
Browser* FindBrowserForSender(id sender, NSWindow* window) {
  NSWindow* targetWindow = window;
  if ([sender respondsToSelector:@selector(window)])
    targetWindow = [sender window];
  Browser* browser = chrome::FindBrowserWithWindow(targetWindow);
  DCHECK(browser);
  return browser;
}

}  // namespace

@implementation BrowserWindowCommandHandler

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                           window:(NSWindow*)window {
  if ([item action] != @selector(commandDispatch:) &&
      [item action] != @selector(commandDispatchUsingKeyModifiers:)) {
    // NSWindow should only forward the above selectors here. All other
    // selectors must be handled by the default -[NSWindow
    // validateUserInterfaceItem:window:].
    NOTREACHED();
    // By default, interface items are enabled if the object in the responder
    // chain that implements the action does not implement
    // -validateUserInterfaceItem. Since we only care about -commandDispatch,
    // return YES for all other actions.
    return YES;
  }

  Browser* browser = chrome::FindBrowserWithWindow(window);
  DCHECK(browser);
  NSInteger tag = [item tag];
  if (!chrome::SupportsCommand(browser, tag))
    return NO;

  // Generate return value (enabled state).
  BOOL enable = chrome::IsCommandEnabled(browser, tag);
  switch (tag) {
    case IDC_CLOSE_TAB:
      // Disable "close tab" if the receiving window is not tabbed.
      // We simply check whether the item has a keyboard shortcut set here;
      // app_controller_mac.mm actually determines whether the item should
      // be enabled.
      if (NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item))
        enable &= !![[menuItem keyEquivalent] length];
      break;
    case IDC_FULLSCREEN: {
      if (NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item))
        [menuItem setTitle:GetTitleForFullscreenMenuItem(browser)];
      break;
    }
    case IDC_BOOKMARK_PAGE: {
      // Extensions have the ability to hide the bookmark page menu item.
      // This only affects the bookmark page menu item under the main menu.
      // The bookmark page menu item under the app menu has its visibility
      // controlled by AppMenuModel.
      bool shouldHide =
          chrome::ShouldRemoveBookmarkThisPageUI(browser->profile());
      NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item);
      [menuItem setHidden:shouldHide];
      break;
    }
    case IDC_BOOKMARK_ALL_TABS: {
      // Extensions have the ability to hide the bookmark all tabs menu
      // item.  This only affects the bookmark page menu item under the main
      // menu.  The bookmark page menu item under the app menu has its
      // visibility controlled by AppMenuModel.
      bool shouldHide =
          chrome::ShouldRemoveBookmarkOpenPagesUI(browser->profile());
      NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item);
      [menuItem setHidden:shouldHide];
      break;
    }
    case IDC_SHOW_AS_TAB: {
      // Hide this menu option if the window is tabbed or is the devtools
      // window.
      NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item);
      [menuItem setHidden:browser->is_type_tabbed() || browser->is_devtools()];
      break;
    }
    case IDC_ROUTE_MEDIA: {
      // Hide this menu option if Media Router is disabled.
      NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item);
      [menuItem
          setHidden:!media_router::MediaRouterEnabled(browser->profile())];
      break;
    }
    default:
      break;
  }

  // If the item is toggleable, find its toggle state and
  // try to update it.  This is a little awkward, but the alternative is
  // to check after a commandDispatch, which seems worse.
  UpdateToggleStateWithTag(tag, item, window);

  return enable;
}

- (void)commandDispatch:(id)sender window:(NSWindow*)window {
  DCHECK(sender);
  int command = [sender tag];
  chrome::ExecuteCommand(FindBrowserForSender(sender, window), command);
}

- (void)commandDispatchUsingKeyModifiers:(id)sender window:(NSWindow*)window {
  DCHECK(sender);

  if (![sender isEnabled]) {
    // This code is reachable e.g. if the user mashes the back button, queuing
    // up a bunch of events before the button's enabled state is updated:
    // http://crbug.com/63254
    return;
  }

  NSInteger command = [sender tag];
  NSUInteger modifierFlags = [[NSApp currentEvent] modifierFlags];
  if ((command == IDC_RELOAD) &&
      (modifierFlags & (NSShiftKeyMask | NSControlKeyMask))) {
    command = IDC_RELOAD_BYPASSING_CACHE;
    // Mask off Shift and Control so they don't affect the disposition below.
    modifierFlags &= ~(NSShiftKeyMask | NSControlKeyMask);
  }
  if (![[sender window] isMainWindow]) {
    // Remove the command key from the flags, it means "keep the window in
    // the background" in this case.
    modifierFlags &= ~NSCommandKeyMask;
  }
  chrome::ExecuteCommandWithDisposition(
      FindBrowserForSender(sender, window), command,
      ui::WindowOpenDispositionFromNSEventWithFlags([NSApp currentEvent],
                                                    modifierFlags));
}

@end
