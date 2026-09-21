// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/data/webui/webui_composebox_pixel_test.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/common/omnibox_features.h"

// Test fixture for ComposeBox pixel tests.
class LensSidePanelComposeBoxPixelTest
    : public WebUIComposeBoxPixelTest,
      public testing::WithParamInterface<ComposeBoxPixelTestParams> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay, {}},
         {lens::features::kLensSearchAimM3,
          {{"use-aim-eligibility-service", "false"},
           {"enable-client-side-header", "true"},
           {"contextualize-on-focus", "false"}}},
         {omnibox::kAimEnabled, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled});
    WebUIComposeBoxPixelTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SetRTL(GetParam().rtl);
    SetDarkMode(GetParam().dark_mode);
    WebUIComposeBoxPixelTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Instantiating the tests.
INSTANTIATE_TEST_SUITE_P(
    All,
    LensSidePanelComposeBoxPixelTest,
    testing::ValuesIn<ComposeBoxPixelTestParams>({
        // Testing in dark mode.
        {},
        {.dark_mode = true},
        // Testing with text.
        {.with_text = true},
        // Testing RTL with and without text.
        {.rtl = true},
        {.rtl = true, .with_text = true},
    }),
    [](const testing::TestParamInfo<ComposeBoxPixelTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(LensSidePanelComposeBoxPixelTest, Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  // DeepQuery needed to target elements with injected JS.
  const DeepQuery kComposebox = {"lens-side-panel-app", "#composebox"};
  const DeepQuery kComposeBoxInput = {"lens-side-panel-app", "#composebox",
                                      "cr-composebox-input", "textarea"};

  RunTestSequence(
      SetupWebUIEnvironment(kActiveTab,
                            GURL(chrome::kChromeUILensUntrustedSidePanelAPIURL),
                            {"lens-side-panel-app"}),

      // Ensure the composebox exists.
      EnsurePresent(kActiveTab, kComposebox),

      // Disable the blinking caret to reduce flakiness.
      ExecuteJsAt(kActiveTab, kComposeBoxInput,
                  R"((el) => {el.style.caretColor = 'transparent'})"),

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
                      /*baseline_cl=*/"8254217"));
}
