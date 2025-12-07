// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_WEBUI_COMPOSEBOX_PIXEL_TEST_H_
#define CHROME_TEST_DATA_WEBUI_WEBUI_COMPOSEBOX_PIXEL_TEST_H_

#include <string>

#include "base/i18n/rtl.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/native_theme/mock_os_settings_provider.h"

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

// Helper JS to disable animations in the side panel. This is used to prevent
// flakiness in pixel tests.
const char kDisableAnimationsJs[] = R"((el) => {
  function disableAnimationsInShadowRoots(root) {
    // Find all elements that have a shadow root
    const shadowHosts = root.querySelectorAll('*');

    for (const host of shadowHosts) {
      if (host.shadowRoot) {
        // Inject the animation-disabling style into the shadow root
        const style = document.createElement('style');
        style.textContent = `
          *, *::before, *::after {
            transition: none !important;
            transition-delay: 0s !important;
            transition-duration: 0s !important;
            animation-delay: -0.0001s !important;
            animation-duration: 0.0001s !important;
            animation: none !important;
          }
        `;
        host.shadowRoot.appendChild(style);

        // Recursively check for nested shadow roots
        disableAnimationsInShadowRoots(host.shadowRoot);
      }
    }
  }
  disableAnimationsInShadowRoots(el.parentElement);
})";

// Base class for WebUI ComposeBox pixel tests.
class WebUIComposeBoxPixelTest : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override;

  // Sets up the environment in the active tab. Disables animations
  // to prevent flakiness. Must pass in a ElementIdentifier for the tab to
  // instrument so it is accessible from the test body.
  InteractiveTestApi::MultiStep SetupWebUIEnvironment(
      ui::ElementIdentifier tab_id,
      const GURL& url,
      const DeepQuery& root_element) {
    // Set the browser size to mimic the side panel size.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetSize(
        {400, 1200});

    return Steps(InstrumentTab(tab_id), NavigateWebContents(tab_id, url),
                 WaitForWebContentsReady(tab_id, url),
                 WaitForWebContentsPainted(tab_id),
                 ExecuteJsAt(tab_id, root_element, kDisableAnimationsJs),
                 FocusElement(tab_id));
  }

 protected:
  // Sets the RTL mode for the test.
  void SetRTL(bool rtl) { rtl_ = rtl; }

  // Sets the dark mode for the test.
  void SetDarkMode(bool dark_mode) { dark_mode_ = dark_mode; }

  ui::MockOsSettingsProvider os_settings_provider_;

 private:
  // Whether the test is running in RTL mode.
  bool rtl_ = false;
  // Whether the test is running in dark mode.
  bool dark_mode_ = false;
};

// Struct for ComposeBox pixel test params.
struct ComposeBoxPixelTestParams {
  bool focused = false;
  bool dark_mode = false;
  bool rtl = false;
  bool with_text = false;

  std::string ToString() const {
    std::string name;
    name += focused ? "Focused" : "Unfocused";
    if (dark_mode) {
      name += "_Dark";
    }
    if (rtl) {
      name += "_RTL";
    }
    if (with_text) {
      name += "_WithText";
    }
    return name;
  }
};

#endif  // CHROME_TEST_DATA_WEBUI_WEBUI_COMPOSEBOX_PIXEL_TEST_H_
