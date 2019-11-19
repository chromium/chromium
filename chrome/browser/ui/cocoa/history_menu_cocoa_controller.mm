// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"

#import "base/mac/foundation_util.h"
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
#include "components/sessions/core/tab_restore_service.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/window_open_disposition.h"

using content::OpenURLParams;
using content::Referrer;

@implementation HistoryMenuCocoaController

- (id)initWithBridge:(HistoryMenuBridge*)bridge {
  if ((self = [super init])) {
    bridge_ = bridge;
    DCHECK(bridge_);
  }
  return self;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  AppController* controller =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  return ![controller keyWindowIsModal];
}

// Open the URL of the given history item in the current tab.
- (void)openURLForItem:(const HistoryMenuBridge::HistoryItem*)node {
  // If this item can be restored using TabRestoreService, do so. Otherwise,
  // just load the URL.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(bridge_->profile());
  if (node->session_id.is_valid() && service) {
    Browser* browser = chrome::FindTabbedBrowser(bridge_->profile(), false);
    BrowserLiveTabContext* context =
        browser ? browser->live_tab_context() : NULL;
    service->RestoreEntryById(context, node->session_id,
                              WindowOpenDisposition::UNKNOWN);
  } else {
    DCHECK(node->url.is_valid());
    WindowOpenDisposition disposition =
        ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
    Profile* target_profile = bridge_->profile();

    // Allow a history menu item to open in an active incognito window.
    // Specifically, if the active window has the same root profile as the
    // bridge, target the active profile. Without this, history menu items open
    // in the nearest non-incognito window, or create one.
    if (auto* active_browser = chrome::FindBrowserWithActiveWindow()) {
      if (active_browser->profile()->GetOriginalProfile() == target_profile)
        target_profile = active_browser->profile();
    }

    NavigateParams params(target_profile, node->url,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = disposition;
    Navigate(&params);
  }
}

- (IBAction)openHistoryMenuItem:(id)sender {
  const HistoryMenuBridge::HistoryItem* item =
      bridge_->HistoryItemForMenuItem(sender);

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
  bridge_->SetIsMenuOpen(true);
}

- (void)menuDidClose:(NSMenu*)menu {
  bridge_->SetIsMenuOpen(false);
}

@end  // HistoryMenuCocoaController
