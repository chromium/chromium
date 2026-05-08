// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

class BrowserWindowInterface;
class AvatarToolbarButtonInterface;
class AvatarToolbarButton;

namespace views {
class Widget;
}  // namespace views

// Waits until the initial WebUI component has performed its first non-empty
// paint.
void WaitUntilInitialWebUIPaintAndFlushMetricsForTesting(
    BrowserWindowInterface* browser);

// Waits until the InitialWebUIManager says the toolbar is ready.
void WaitForInitialWebUIToolbar(BrowserWindowInterface* browser);

class AvatarToolbarButtonTestAccessor {
 public:
  explicit AvatarToolbarButtonTestAccessor(BrowserWindowInterface* browser);
  void WaitForAvatarButton();
  bool GetEnabled();
  bool GetVisible();
  std::u16string GetText();
  views::Widget* GetWidget();
  gfx::ImageSkia GetImage(views::Button::ButtonState state);
  std::u16string GetRenderedTooltipText(const gfx::Point& p);

  void Click();

 private:
  AvatarToolbarButtonInterface* GetInterface();
  AvatarToolbarButton* GetButton();

  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
