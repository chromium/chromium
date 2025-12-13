// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accessibility_util.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/accessibility/view_accessibility.h"

void AnnounceInActiveBrowser(const std::u16string& message) {
  BrowserWindowInterface* const browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  const bool is_type_normal =
      browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
  if (!browser || !is_type_normal || !browser->IsActive()) {
    return;
  }

  BrowserView::GetBrowserViewForBrowser(browser)
      ->GetViewAccessibility()
      .AnnounceText(message);
}
