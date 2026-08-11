// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_

#include <memory>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

class AvatarToolbarButton;
class Browser;
class BrowserWindowInterface;
class WebUIAvatarToolbarButton;
enum class AvatarToolbarButtonState;
class WebUIToolbarWebView;

namespace ui {
class ElementIdentifier;
class TrackedElement;
}  // namespace ui

namespace content {
class WebContents;
}

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

// Simulates a left-click on the WebUI toolbar extension button with the given
// `id` (or the puzzle piece extensions menu button if `id` is empty).
void LeftClickExtensionButton(content::WebContents* web_contents,
                              const std::string& id);

// Simulates a right-click (context menu) on the WebUI toolbar extension button
// with the given `id`.
void RightClickExtensionButton(content::WebContents* web_contents,
                               const std::string& id);

// Returns JavaScript expression selecting an element inside toolbar-app.
std::string GetButtonAppJS(const std::string& selector);

// Checks if an element matching `selector` inside toolbar-app is visible.
bool IsButtonVisible(content::WebContents* web_contents,
                     const std::string& selector);

// Waits until an element matching `selector` inside toolbar-app is visible.
bool WaitForButtonVisible(content::WebContents* web_contents,
                          const std::string& selector);

// Waits until an element matching `selector` inside toolbar-app is hidden.
bool WaitForButtonHidden(content::WebContents* web_contents,
                         const std::string& selector);

// Pins a button preference and waits for composition in WebUI toolbar.
void PinButton(Browser* browser, views::WebView* web_view, const char* pref);

// Pins Home button and waits for it to become visible in WebUI toolbar.
WebUIToolbarWebView* SetUpAndPinHomeButton(Browser* browser);

// Returns JS expression selecting the inner icon/chip button element.
std::string GetButtonIconJS(const std::string& selector);

// JavaScript snippet that computes center x and y coordinates of `target`.
extern const char kGetCoordinatesJS[];

// Adds functions to `target` to mimic pointer capture functions. Note that real
// pointer capture is lost on pointer up, but the returned functions cannot
// handle that, so if that is important for a test, it must manually call
// `releasePointerCapture('*')`.
std::string AddMockPointerCaptureFunctions(const char* target);

// Dispatches an event to a WebUI toolbar button.
// `selector`: The CSS selector for the button element.
// `event_class`: The JS event class (e.g. 'MouseEvent', 'PointerEvent').
// `type`: The event type string (e.g. 'click', 'contextmenu').
// `options`: JS object string for event options (e.g. "detail: 1, button: 2").
std::string DispatchEventScript(const std::string& selector,
                                const std::string& event_class,
                                const std::string& type,
                                const std::string& options = "");

class AvatarButtonUpdateWaiter : public AvatarToolbarButtonInterface::Observer {
 public:
  explicit AvatarButtonUpdateWaiter(AvatarToolbarButtonInterface* button);
  ~AvatarButtonUpdateWaiter() override;

  AvatarButtonUpdateWaiter(const AvatarButtonUpdateWaiter&) = delete;
  AvatarButtonUpdateWaiter& operator=(const AvatarButtonUpdateWaiter&) = delete;

  void Wait();

  // AvatarToolbarButtonInterface::Observer:
  void OnIconUpdated() override;

 private:
  bool updated_ = false;
  base::RunLoop run_loop_;
  base::RepeatingClosure quit_closure_;
  base::ScopedObservation<AvatarToolbarButtonInterface,
                          AvatarToolbarButtonInterface::Observer>
      scoped_observation_{this};
};

class AvatarToolbarButtonTestAccessor {
 public:
  using ButtonVariant =
      std::variant<AvatarToolbarButton*, WebUIAvatarToolbarButton*>;

  explicit AvatarToolbarButtonTestAccessor(BrowserWindowInterface* browser);
  ~AvatarToolbarButtonTestAccessor();
  void WaitForAvatarButton();
  bool WaitForText(const std::u16string& text);
  bool WaitForTextNotEqual(const std::u16string& text);
  bool WaitForState(AvatarToolbarButtonState state);
  std::unique_ptr<AvatarButtonUpdateWaiter> CreateUpdateWaiter();
  AvatarToolbarButtonState GetState();
  bool WaitForRenderedTooltipText(const std::u16string& text);
  bool WaitForAccessibilityLabel(const std::u16string& text);
  bool WaitForAccessibilityDescription(const std::u16string& text);
  bool GetEnabled();
  bool GetVisible();
  std::u16string GetText();
  views::Widget* GetWidget();
  gfx::ImageSkia GetImage(views::Button::ButtonState state);
  std::string GetImageUrl();
  std::u16string GetRenderedTooltipText(const gfx::Point& p);
  std::u16string GetAccessibilityLabel();
  std::u16string GetAccessibilityDescription();
  void Click();
  void SetAnnounceCallbackForTesting(
      base::OnceCallback<void(std::u16string)> callback);

 private:
  content::WebContents* GetWebContents();
  AvatarToolbarButtonInterface* GetInterface();
  bool ShouldUseCppFallback(WebUIAvatarToolbarButton* button);
  ButtonVariant GetButton();

  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TEST_UTILS_H_
