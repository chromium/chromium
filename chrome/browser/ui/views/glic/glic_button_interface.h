// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_INTERFACE_H_

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/class_property.h"
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

  // Width factor of button, used in animations.
  virtual float GetWidthFactor() const = 0;

  // True when the button state is showing a nudge
  virtual void SetIsShowingNudge(bool is_showing) = 0;
  virtual bool GetVisible() = 0;

  // Expose the property handler via a virtual method to avoid diamond
  // inheritance when using GlicButtonInterface in addition to a view.
  virtual ui::PropertyHandler* GetPropertyHandler() = 0;
};
}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_INTERFACE_H_
