// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
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

// Base class for Lens Side Panel pixel tests.
// These tests are intended to be used to verify subtle visual appearance
// differences that are hard to verify via Mocha tests.  Note, the
// screenshots are only setup to be captured on win-rel, and should be skipped
// on other platforms via `SetOnIncompatibleAction()` step.

// To debug locally, you can run the test via:
// `out/Default/interactive_ui_tests
// --gtest_filter="*<TEST_NAME>*" --test-launcher-interactive`. The
// `--test-launcher-interactive` flag will pause the test at the very end, after
// the screenshot would've been taken, allowing you to inspect the UI and debug.
//
// To generate an actual screenshot locally, you can run the test with
// `out/Default/interactive_ui_tests
// --gtest_filter="*<TEST_NAME>*" --browser-ui-tests-verify-pixels
// --enable-pixel-output-in-tests --test-launcher-retry-limit=0
// --ui-test-action-timeout=100000
// --skia-gold-local-png-write-directory="/tmp/pixel_test_output"
// --bypass-skia-gold-functionality`. The PNG of the screenshot will be saved to
// the `/tmp/pixel_test_output` directory.

// Additionally, for the pixel tests to be run on try bots, there name must
// follow the pattern `LensSidePanel*PixelTest*`. If not, the test needs to
// manually be added to `testing/buildbot/filters/pixel_tests.filter`.

class LensSidePanelPixelTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    SetupFeatureList();
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    if (rtl_) {
      base::i18n::SetRTLForTesting(true);
    }
    if (dark_mode_) {
      os_settings_provider_.SetPreferredColorScheme(
          ui::NativeTheme::PreferredColorScheme::kDark);
    }
  }

  // Sets up the side panel environment in the active tab. Disables animations
  // to prevent flakiness. Must pass in a ElementIdentifier for the tab to
  // instrument so it is accessible from the test body.
  InteractiveTestApi::MultiStep SetupSidePanelEnvironment(
      ui::ElementIdentifier tab_id) {
    // Set the browser size to mimic the side panel size.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetSize(
        {400, 1200});

    return Steps(
        InstrumentTab(tab_id),
        NavigateWebContents(
            tab_id, GURL(chrome::kChromeUILensUntrustedSidePanelAPIURL)),
        WaitForWebContentsReady(
            tab_id, GURL(chrome::kChromeUILensUntrustedSidePanelAPIURL)),
        WaitForWebContentsPainted(tab_id),
        ExecuteJsAt(tab_id, {"lens-side-panel-app"}, kDisableAnimationsJs),
        FocusElement(tab_id));
  }

 protected:
  virtual void SetupFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay, {}},
         {lens::features::kLensSearchAimM3,
          {{"use-aim-eligibility-service", "false"},
           {"enable-client-side-header", "true"},
          {"contextualize-on-focus", "false"}}}},
        /*disabled_features=*/{});
  }

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
  base::test::ScopedFeatureList feature_list_;
};

// Struct for ComposeBox pixel test params.
struct LensComposeBoxPixelTestParams {
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

// Test fixture for ComposeBox pixel tests.
class LensSidePanelComposeBoxPixelTest
    : public LensSidePanelPixelTest,
      public testing::WithParamInterface<LensComposeBoxPixelTestParams> {
 public:
  void SetUpOnMainThread() override {
    SetRTL(GetParam().rtl);
    SetDarkMode(GetParam().dark_mode);
    LensSidePanelPixelTest::SetUpOnMainThread();
  }
};

// Instantiating the tests.
INSTANTIATE_TEST_SUITE_P(
    All,
    LensSidePanelComposeBoxPixelTest,
    testing::ValuesIn<LensComposeBoxPixelTestParams>({
        // Testing focused vs unfocused in dark mode.
        {},
        {.focused = true},
        {.dark_mode = true},
        {.focused = true, .dark_mode = true},
        // Testing focused vs unfocused with text.
        {.with_text = true},
        {.focused = true, .with_text = true},
        // Testing RTL with and without text, without and without focus.
        {.rtl = true},
        {.focused = true, .rtl = true},
        {.focused = true, .rtl = true, .with_text = true},
        {.rtl = true, .with_text = true},
    }),
    [](const testing::TestParamInfo<LensComposeBoxPixelTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(LensSidePanelComposeBoxPixelTest, Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kComposeBoxExpanded);

  // DeepQuery needed to target elements with injected JS.
  const DeepQuery kComposebox = {"lens-side-panel-app", "#composebox"};
  const DeepQuery kComposeBoxInput = {"lens-side-panel-app", "#composebox",
                                      "textarea"};

  RunTestSequence(
      SetupSidePanelEnvironment(kActiveTab),

      // Ensure the composebox exists.
      EnsurePresent(kActiveTab, kComposebox),

      // Disable the blinking caret to reduce flakiness.
      ExecuteJsAt(kActiveTab, kComposeBoxInput,
                  R"((el) => {el.style.caretColor = 'transparent'})"),

      // Focus the composebox if specified. Waits for the composebox to expand
      // before continuing.
      If([]() { return GetParam().focused; },
         Then(
             ExecuteJsAt(kActiveTab, kComposebox,
                         "(el) => { window.initialComposeboxHeight = "
                         "el.getBoundingClientRect().height; }"),
             ExecuteJsAt(kActiveTab, kComposeBoxInput, "(el) => el.focus()"),
             [=]() {
               WebContentsInteractionTestUtil::StateChange compose_box_expanded;
               compose_box_expanded.event = kComposeBoxExpanded;
               compose_box_expanded.where = kComposebox;
               compose_box_expanded.type = WebContentsInteractionTestUtil::
                   StateChange::Type::kExistsAndConditionTrue;
               compose_box_expanded.test_function =
                   "(el) => { return el.getBoundingClientRect().height > "
                   "window.initialComposeboxHeight; }";
               return WaitForStateChange(kActiveTab, compose_box_expanded);
             }())),

      // Set the composebox text if specified.
      If([]() { return GetParam().with_text; },
         Then(ExecuteJsAt(kActiveTab, kComposeBoxInput,
                          R"((el) => {
                           el.value = 'some text';
                           el.dispatchEvent(new Event('input', {bubbles:
                           true, composed: true}));
                         })"))),

      // This step is needed to prevent test from failing on platforms that
      // don't support screenshots.
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshots not captured on this platform."),

      // Take a screenshot of the composebox.
      ScreenshotWebUi(kActiveTab, kComposebox,
                      /*screenshot_name=*/"LensComposebox",
                      /*baseline_cl=*/"7018205"));
}
