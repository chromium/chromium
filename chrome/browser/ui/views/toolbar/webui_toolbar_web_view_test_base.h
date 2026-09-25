// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_

#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace ui {
class TrackedElement;
}

class BrowserWindowInterface;
class ToolbarView;
class WebUIToolbarWebView;

// Base test fixture providing common feature setup and helper methods for
// WebUIToolbarWebView browser tests and interactive UI tests.
class WebUIToolbarWebViewTestBase : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewTestBase();
  ~WebUIToolbarWebViewTestBase() override;

  void SetUpOnMainThread() override;

  // For all of these, if `browser_instance` is nullptr, browser() is used.
  ToolbarView* GetToolbarView(
      BrowserWindowInterface* browser_instance = nullptr);
  WebUIToolbarWebView* GetWebUIToolbar(
      BrowserWindowInterface* browser_instance = nullptr);
  content::WebContents* GetWebUIWebContents(
      BrowserWindowInterface* browser_instance = nullptr);

  // Sets the size of a test-only element on the toolbar-app with the provided
  // size, which should cause responsive controls to be asynchronously laid out
  // to accommodate it. Does not wait for that layout to occur. Adds the element
  // on the first call, and resizes it on subsequent calls.
  //
  // This is more flexible than resizing the window due to the window having a
  // min size. It also provides test coverage that adding/sizing
  // non-ResponsiveControls to the toolbar-app correctly causes
  // layoutResponsiveControls() to be invoked.
  //
  // Note that first adding the spacer will likely add some extra
  // margins/padding in addition to `width`.
  [[nodiscard]] content::EvalJsResult SetSpacerWidth(
      int width,
      BrowserWindowInterface* browser_instance = nullptr);

  // Gets the specified tracked element.
  ui::TrackedElement* GetTrackedElement(
      ui::ElementIdentifier id,
      BrowserWindowInterface* browser_instance = nullptr);

  // Waits until all `visible` elements are visible and all `hidden` elements
  // are hidden.
  [[nodiscard]] bool WaitForTrackedElements(
      const std::vector<ui::ElementIdentifier>& visible,
      const std::vector<ui::ElementIdentifier>& hidden = {},
      BrowserWindowInterface* browser_instance = nullptr);

  // Waits until the specified tracked element is visible. Returns null on
  // failure, or if it's detected as visible, but is then hidden before the
  // function returns.
  ui::TrackedElement* WaitForTrackedElementVisible(
      ui::ElementIdentifier id,
      BrowserWindowInterface* browser_instance = nullptr);

  // Waits until the specified tracked element is hidden (or destroyed).
  [[nodiscard]] bool WaitForTrackedElementHidden(
      ui::ElementIdentifier id,
      BrowserWindowInterface* browser_instance = nullptr);

  // Enables battery saver mode and waits until the button is visible.
  void EnableBatterySaverButton(
      content::WebContents* webui_web_contents = nullptr);

  // Returns whether the media button is supported by the WebUI toolbar. The
  // media button is not available on all platforms, so tests that depend on it
  // should check this at runtime, rather than hardcoding the list of supported
  // platforms, which could otherwise silently go stale.
  bool IsMediaButtonSupported();

  // Shows the media button and waits until the button is visible, as observed
  // by the ElementTracker. Calls directly into the media button to show it,
  // rather than by playing audio. The media button must be supported on the
  // current platform.
  void ShowMediaButton();

  // Disables (that is, greys out, rather than hides) the media button and waits
  // until the WebUI renderer process's DOM element is updated to be disabled.
  // Requires the media button to already be visible. Calls directly into the
  // media button to disable it, as opposed to by opening a dialog.
  void DisableMediaButton();

 protected:
  WebUIToolbarWebViewTestBase(
      const std::vector<base::test::FeatureRef>& enabled,
      const std::vector<base::test::FeatureRef>& disabled);

  void SimulateDropOnToolbar(content::WebContents* web_contents,
                             const std::string& text);

  void SimulateUriListDropOnToolbar(content::WebContents* web_contents,
                                    const std::string& url);

  scoped_refptr<const extensions::Extension> LoadAndPinExtension(
      WebUIToolbarWebView* webui_toolbar_view,
      base::ScopedTempDir& temp_dir,
      bool has_background_script = false,
      bool has_popup = false);

  scoped_refptr<const extensions::Extension> LoadExtension(
      base::ScopedTempDir& temp_dir,
      bool has_background_script = false,
      bool has_popup = false);

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_TEST_BASE_H_
