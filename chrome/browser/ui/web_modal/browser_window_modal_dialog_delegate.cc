// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_modal/browser_window_modal_dialog_delegate.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"         // nogncheck
#include "chrome/browser/ui/browser_window.h"  // nogncheck
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"

DEFINE_USER_DATA(BrowserWindowModalDialogDelegate);

BrowserWindowModalDialogDelegate::BrowserWindowModalDialogDelegate(
    BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}

BrowserWindowModalDialogDelegate::~BrowserWindowModalDialogDelegate() = default;

// static
BrowserWindowModalDialogDelegate* BrowserWindowModalDialogDelegate::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

base::CallbackListSubscription
BrowserWindowModalDialogDelegate::RegisterDevToolsScrimCallback(
    DevToolsScrimCallback callback) {
  return devtools_scrim_callbacks_.Add(std::move(callback));
}

void BrowserWindowModalDialogDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  int index = tab_strip_model->GetIndexOfWebContents(web_contents);
  if (index == TabStripModel::kNoTab) {
    // The WebContents may no longer exist in the TabStripModel.
    // If the WebContents has a DevTools window, the call is meant for the
    // DevTools area.
    if (DevToolsWindow::AsDevToolsWindow(web_contents)) {
      devtools_scrim_callbacks_.Notify(blocked);
    }
    return;
  }

  // Drop HTML fullscreen to give users context for making informed decisions.
  // Skip browser-fullscreen, which is more expressly user-initiated.
  // Skip fullscreen-within-tab, which shows the browser frame.
  if (blocked) {
    content::FullscreenState fullscreen_state =
        browser_->GetFeatures()
            .exclusive_access_manager()
            ->fullscreen_controller()
            ->GetFullscreenState(web_contents);
    if (fullscreen_state.target_mode == content::FullscreenMode::kContent) {
      // Skip URLs with the automatic fullscreen content setting granted.
      const GURL& url = web_contents->GetLastCommittedURL();
      const HostContentSettingsMap* const content_settings =
          HostContentSettingsMapFactory::GetForProfile(
              web_contents->GetBrowserContext());
      if (content_settings->GetContentSetting(
              url, url, ContentSettingsType::AUTOMATIC_FULLSCREEN) !=
          CONTENT_SETTING_ALLOW) {
        web_contents->ExitFullscreen(true);
      }
    }
  }

  tab_strip_model->SetTabBlocked(index, blocked);

  const bool browser_active =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile() == browser_;
  bool contents_is_active =
      tab_strip_model->GetActiveWebContents() == web_contents;
  // If the WebContents is foremost (the active tab in the front-most browser)
  // and is being unblocked, focus it to make sure that input works again.
  if (!blocked && contents_is_active && browser_active) {
    web_contents->Focus();
  }
}

web_modal::WebContentsModalDialogHost*
BrowserWindowModalDialogDelegate::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  // Use BrowserWindow::GetWebContentsModalDialogHostFor which handles both
  // tab and non-tab WebContents (e.g. DevTools) with correct fallback.
  return browser_->GetBrowserForMigrationOnly()
      ->window()
      ->GetWebContentsModalDialogHostFor(web_contents);
}
