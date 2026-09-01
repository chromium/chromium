// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_TEST_ACCESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_TEST_ACCESSOR_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;
class LocationIconView;

class LocationIconTestAccessor {
 public:
  explicit LocationIconTestAccessor(BrowserWindowInterface* browser);
  ~LocationIconTestAccessor();

  // Simulates clicking the location icon to show the Page Info bubble.
  void Click();

  // Shows the Page Info bubble.
  bool ShowBubble();

  // Checks if the Page Info bubble is currently showing.
  bool IsBubbleShowing() const;

  // Returns true if the icon is visible. This may update asynchronously.
  bool IsVisible();

  // Returns true if the text is showing. This may update asynchronously.
  bool IsShowingText();

  // Returns the configured text. This may update asynchronously.
  std::u16string GetText();

  // Returns the legacy LocationIconView if in Views mode, or nullptr if WebUI.
  LocationIconView* GetLocationIconView();

 private:
  content::WebContents* GetWebContents();

  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_TEST_ACCESSOR_H_
