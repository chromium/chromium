// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accessibility_util.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "ui/views/accessibility/view_accessibility.h"

void AnnounceInActiveBrowser(const std::u16string& message) {
  BrowserWindowInterface* const browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!browser ||
      browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
    return;
  }

  if (!browser->IsActive()) {
    // If the inactive browser is waiting for the initial WebUI to finish, we
    // will defer this announcement.
    if (InitialWebUIManager* manager = InitialWebUIManager::From(browser)) {
      manager->RequestDeferShow(base::BindOnce(
          [](std::u16string msg, BrowserWindowInterface* b) {
            BrowserView* browser_view =
                BrowserView::GetBrowserViewForBrowser(b);
            if (browser_view) {
              browser_view->GetViewAccessibility().AnnounceText(msg);
            }
          },
          message, browser));
    }
    return;
  }

  BrowserView::GetBrowserViewForBrowser(browser)
      ->GetViewAccessibility()
      .AnnounceText(message);
}
