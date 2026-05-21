// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_

#include <string>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

class AvatarToolbarButton;
class AvatarToolbarButtonInterface;
class Browser;
class BrowserWindowInterface;
class WebUIAvatarToolbarButton;
class WebUIToolbarWebView;

namespace ui {
class ElementIdentifier;
class TrackedElement;
}  // namespace ui

namespace views {
class WebView;
class Widget;
}  // namespace views

// Waits until the initial WebUI component has performed its first non-empty
// paint.
void WaitUntilInitialWebUIPaintAndFlushMetricsForTesting(
    BrowserWindowInterface* browser);

// Waits until the InitialWebUIManager says the toolbar is ready.
void WaitForInitialWebUIToolbar(BrowserWindowInterface* browser);

// Sets up the WebUI toolbar for testing by waiting for the toolbar view,
// resolving the element associated with `element_id`, extracting the child
// WebView, and waiting for it to finish composition.
void SetUpWebUI(const ui::ElementIdentifier& element_id,
                ui::TrackedElement** element_out,
                WebUIToolbarWebView** webui_toolbar_view_out,
                views::WebView** web_view_out,
                Browser* browser);

// Retrieves the WebUIToolbarWebView instance associated with the given
// `browser`.
WebUIToolbarWebView* GetWebUIToolbarWebView(Browser* browser);

class AvatarToolbarButtonTestAccessor {
 public:
  using ButtonVariant =
      std::variant<AvatarToolbarButton*, WebUIAvatarToolbarButton*>;

  explicit AvatarToolbarButtonTestAccessor(BrowserWindowInterface* browser);
  ~AvatarToolbarButtonTestAccessor();
  void WaitForAvatarButton();
  bool GetEnabled();
  bool GetVisible();
  std::u16string GetText();
  views::Widget* GetWidget();
  gfx::ImageSkia GetImage(views::Button::ButtonState state);
  std::u16string GetRenderedTooltipText(const gfx::Point& p);
  void Click();
  void SetAnnounceCallbackForTesting(
      base::OnceCallback<void(std::u16string)> callback);

 private:
  AvatarToolbarButtonInterface* GetInterface();
  ButtonVariant GetButton();

  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
