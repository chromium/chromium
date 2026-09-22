// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/glic_button_interface.h"

#include "base/check.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

namespace glic {

// static
views::LabelButton* GlicButtonInterface::FromBrowser(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  CHECK(browser_view);
  return browser_view->GetGlicButton();
}

}  // namespace glic
