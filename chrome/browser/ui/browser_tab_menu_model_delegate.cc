// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"

namespace chrome {

BrowserTabMenuModelDelegate::BrowserTabMenuModelDelegate(
    SessionID session_id,
    const Profile* profile,
    const web_app::AppBrowserController* app_controller,
    tab_groups::TabGroupSyncService* tgss)
    : session_id_(session_id),
      profile_(profile),
      app_controller_(app_controller),
      tab_group_sync_service_(tgss) {}

BrowserTabMenuModelDelegate::~BrowserTabMenuModelDelegate() = default;

std::vector<BrowserWindowInterface*>
BrowserTabMenuModelDelegate::GetOtherBrowserWindows(bool is_app) {
  std::vector<BrowserWindowInterface*> browsers;

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        // We can only move into a tabbed view of the same profile, and not the
        // same window we're currently in.
        if (browser->GetSessionID() != session_id_ &&
            browser->GetProfile() == profile_) {
          if (is_app &&
              browser->GetType() == BrowserWindowInterface::TYPE_APP &&
              web_app::AppBrowserController::From(browser)->app_id() ==
                  app_controller_->app_id()) {
            browsers.push_back(browser);
          } else if (!is_app && browser->GetType() ==
                                    BrowserWindowInterface::TYPE_NORMAL) {
            browsers.push_back(browser);
          }
        }
        return true;  // continue iterating
      });
  return browsers;
}

tab_groups::TabGroupSyncService*
BrowserTabMenuModelDelegate::GetTabGroupSyncService() {
  return tab_group_sync_service_;
}

}  // namespace chrome
