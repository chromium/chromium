// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/accessibility_util.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/accessibility/view_accessibility.h"

void AnnounceInActiveBrowser(const std::u16string& message) {
  Browser* const browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser || !browser->is_type_normal() || !browser->window()->IsActive())
    return;

  BrowserView::GetBrowserViewForBrowser(browser)
      ->GetViewAccessibility()
      .AnnounceText(message);
}
