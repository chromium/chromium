// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"

#import "base/apple/foundation_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_HISTORY_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/window_open_disposition.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

// TODO(crbug.com/40228273): Single-tab windows get restored as tabs instead of
// windows, which is confusing.
//
// NB: Takes |node| by value, because the HistoryMenuBridge could be destroyed
// before RunInSafeProfileHelper finishes.
void OpenURLForItem(HistoryMenuBridge::HistoryItem node,
                    WindowOpenDisposition disposition,
                    Profile* profile) {
  if (!profile)
    return;  // Failed to load profile, ignore.
  // If this item can be restored using TabRestoreService, do so. Otherwise,
  // just load the URL.
  if (node.session_id.is_valid()) {
    app_controller_mac::TabRestorer::RestoreByID(profile, node.session_id);
  } else {
    DCHECK(node.url.is_valid());
    Profile* target_profile = profile;

    // Allow a history menu item to open in an active incognito window.
    // Specifically, if the active window has the same root profile as the
    // bridge, target the active profile. Without this, history menu items open
    // in the nearest non-incognito window, or create one.
    if (auto* active_browser = chrome::FindBrowserWithActiveWindow()) {
      if (active_browser->profile()->GetOriginalProfile() == target_profile)
        target_profile = active_browser->profile();
    }

    NavigateParams params(target_profile, node.url,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = disposition;
    Navigate(&params);
  }
}

}  // namespace

@implementation HistoryMenuCocoaController {
  raw_ptr<HistoryMenuBridge, AcrossTasksDanglingUntriaged>
      _bridge;  // weak; owns us
}

- (instancetype)initWithBridge:(HistoryMenuBridge*)bridge {
  if ((self = [super init])) {
    _bridge = bridge;
    DCHECK(_bridge);
  }
  return self;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  return ![AppController.sharedController keyWindowIsModal];
}

// Open the URL of the given history item in the current tab.
- (void)openURLForItem:(const HistoryMenuBridge::HistoryItem*)node {
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (Profile* profile = _bridge->profile()) {
    OpenURLForItem(*node, disposition, profile);
  } else {
    // Both HistoryMenuBridge and HistoryMenuCocoaController could get destroyed
    // before RunInSafeProfileHelper finishes. The callback needs to be
    // self-contained.
    app_controller_mac::RunInProfileSafely(
        _bridge->profile_dir(),
        base::BindOnce(&OpenURLForItem, *node, disposition),
        app_controller_mac::kIgnoreOnFailure);
  }
}

- (IBAction)openHistoryMenuItem:(id)sender {
  const HistoryMenuBridge::HistoryItem* item =
      _bridge->HistoryItemForMenuItem(sender);

  if ([sender tag] == HistoryMenuBridge::kRecentlyClosed) {
    base::RecordAction(
        base::UserMetricsAction("TopMenu_History_RecentlyClosed"));
  } else if ([sender tag] == HistoryMenuBridge::kVisited) {
    base::RecordAction(
        base::UserMetricsAction("TopMenu_History_RecentlyVisited"));
  }

  [self openURLForItem:item];
}

// NSMenuDelegate:

- (void)menuWillOpen:(NSMenu*)menu {
  _bridge->SetIsMenuOpen(true);
}

- (void)menuDidClose:(NSMenu*)menu {
  _bridge->SetIsMenuOpen(false);
}

@end  // HistoryMenuCocoaController
