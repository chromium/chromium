// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_INTERFACE_H_

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

namespace views {
class LabelButton;
}

namespace glic {
class GlicButtonInterface {
 public:
  static views::LabelButton* FromBrowser(BrowserWindowInterface* browser) {
    if (!browser) {
      return nullptr;
    }

    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    CHECK(browser_view);
    return browser_view->GetGlicButton();
  }
};
}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_INTERFACE_H_
