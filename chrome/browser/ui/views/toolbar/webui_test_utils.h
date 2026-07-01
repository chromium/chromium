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
