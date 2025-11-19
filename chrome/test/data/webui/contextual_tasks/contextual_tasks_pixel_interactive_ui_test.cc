// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/data/webui/webui_composebox_pixel_test.h"
#include "components/contextual_tasks/public/features.h"
#include "url/url_constants.h"

class FakeContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  explicit FakeContextualTasksUiService(Profile* profile)
      : contextual_tasks::ContextualTasksUiService(profile, nullptr) {}
  GURL GetDefaultAiPageUrl() override { return GURL(url::kAboutBlankURL); }

  static std::unique_ptr<KeyedService> BuildFakeService(
      content::BrowserContext* context) {
    return std::make_unique<FakeContextualTasksUiService>(
        Profile::FromBrowserContext(context));
  }
};

// Test fixture for ComposeBox pixel tests.
class ContextualTasksComposeBoxPixelTest
    : public WebUIComposeBoxPixelTest,
      public testing::WithParamInterface<ComposeBoxPixelTestParams> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures({contextual_tasks::kContextualTasks}, {});
    WebUIComposeBoxPixelTest::SetUp();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating(
                         &FakeContextualTasksUiService::BuildFakeService));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Instantiating the tests.
INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksComposeBoxPixelTest,
    testing::ValuesIn<ComposeBoxPixelTestParams>({
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
    [](const testing::TestParamInfo<ComposeBoxPixelTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(ContextualTasksComposeBoxPixelTest, Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  // DeepQuery needed to target elements with injected JS.
  const DeepQuery kComposebox = {"contextual-tasks-app",
                                 "contextual-tasks-composebox", "#composebox"};
  const DeepQuery kComposeBoxInput = {"contextual-tasks-app",
                                      "contextual-tasks-composebox",
                                      "#composebox", "textarea"};
  const DeepQuery kAiPageWebView = {"contextual-tasks-app", "webview"};

  RunTestSequence(
      SetupWebUIEnvironment(kActiveTab,
                            GURL(chrome::kChromeUIContextualTasksURL),
                            {"contextual-tasks-app"}),

      // Ensure the composebox exists.
      EnsurePresent(kActiveTab, kComposebox),

      // Ensure the AI page webview is loaded with about:blank.
      CheckJsResultAt(kActiveTab, kAiPageWebView, "(el) => el.src",
                      url::kAboutBlankURL),

      // Disable the blinking caret to reduce flakiness.
      ExecuteJsAt(kActiveTab, kComposeBoxInput,
                  R"((el) => {el.style.caretColor = 'transparent'})"),

      // Focus the composebox if specified.
      If([]() { return GetParam().focused; },
         Then(ExecuteJsAt(kActiveTab, kComposeBoxInput, "(el) => el.focus()"))),

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
                      /*screenshot_name=*/"ContextualTasksComposebox",
                      /*baseline_cl=*/"7163438"));
}
