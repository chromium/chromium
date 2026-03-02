// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/data/webui/webui_composebox_pixel_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

class FakeContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  explicit FakeContextualTasksUiService(Profile* profile)
      : contextual_tasks::ContextualTasksUiService(profile,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr) {}
  GURL GetDefaultAiPageUrl() override { return GURL(url::kAboutBlankURL); }

  static std::unique_ptr<KeyedService> BuildFakeService(
      content::BrowserContext* context) {
    return std::make_unique<FakeContextualTasksUiService>(
        Profile::FromBrowserContext(context));
  }
};

class ContextualTasksPixelTestBase : public WebUIComposeBoxPixelTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility},
        {contextual_tasks::kContextualTasksExpandButton});
    WebUIComposeBoxPixelTest::SetUp();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating(
                         &FakeContextualTasksUiService::BuildFakeService));
  }

  void SetUpOnMainThread() override {
    WebUIComposeBoxPixelTest::SetUpOnMainThread();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

    // Set up a fake identity to get an OAuth token, which allows the <webview>
    // to load the AI page correctly.
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@gmail.com",
                                      signin::ConsentLevel::kSignin);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

 protected:
  auto HideCaret(const ui::ElementIdentifier& web_contents_id,
                 const DeepQuery& query) {
    return ExecuteJsAt(web_contents_id, query,
                       R"((el) => { el.style.caretColor = 'transparent'; })");
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;

 private:
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode_ =
      gfx::ScopedAnimationDurationScaleMode(
          gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
};

struct ContextualTasksComposeBoxPixelTestParams {
  bool focused = false;
  bool dark_mode = false;
  bool rtl = false;
  bool with_text = false;
  bool is_ai_page = false;

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
    if (is_ai_page) {
      name += "_AiPage";
    }
    return name;
  }
};

// Test fixture for ComposeBox pixel tests.
class ContextualTasksComposeBoxPixelTest
    : public ContextualTasksPixelTestBase,
      public testing::WithParamInterface<
          ContextualTasksComposeBoxPixelTestParams> {
  void SetUpOnMainThread() override {
    SetRTL(GetParam().rtl);
    SetDarkMode(GetParam().dark_mode);
    ContextualTasksPixelTestBase::SetUpOnMainThread();
  }
};

// Instantiating the tests.
INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksComposeBoxPixelTest,
    testing::ValuesIn<ContextualTasksComposeBoxPixelTestParams>({
        // Testing focused vs unfocused in dark mode.
        {},
        {.focused = true},
        {.dark_mode = true},
        {.focused = true, .dark_mode = true},
        {.dark_mode = true, .with_text = true},
        {.dark_mode = true, .is_ai_page = true},
        // Testing focused vs unfocused with text.
        {.with_text = true},
        {.focused = true, .with_text = true},
        // Testing RTL with and without text, without and without focus.
        {.rtl = true},
        {.focused = true, .rtl = true},
        {.focused = true, .rtl = true, .with_text = true},
        {.rtl = true, .with_text = true},
    }),
    [](const testing::TestParamInfo<ContextualTasksComposeBoxPixelTestParams>&
           info) { return info.param.ToString(); });

IN_PROC_BROWSER_TEST_P(ContextualTasksComposeBoxPixelTest, Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const DeepQuery kApp = {"contextual-tasks-app"};

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

      ExecuteJsAt(kActiveTab, kApp,
                  base::StringPrintf("(el) => { "
                                     "  el.isAiPage_ = %s; "
                                     "  el.requestUpdate(); "
                                     "}",
                                     GetParam().is_ai_page ? "true" : "false")),
      WaitForWebContentsPainted(kActiveTab),

      // Ensure the AI page webview is loaded with about:blank.
      CheckJsResultAt(kActiveTab, kAiPageWebView, "(el) => el.src",
                      url::kAboutBlankURL),

      // Disable the blinking caret to reduce flakiness.
      HideCaret(kActiveTab, kComposeBoxInput),

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
                      /*baseline_cl=*/"7519825"));
}

struct AppPixelTestParams {
  bool dark_mode = false;
  bool is_side_panel = false;
  bool is_zero_state = false;
  bool is_ai_page = false;
  bool is_ghost_loader = false;

  std::string ToString() const {
    std::string name;
    name += dark_mode ? "Dark" : "Light";
    name += is_side_panel ? "_SidePanel" : "_Tab";
    if (is_zero_state) {
      name += "_ZeroState";
    }
    if (is_ai_page) {
      name += "_AiPage";
    }
    if (is_ghost_loader) {
      name += "_GhostLoader";
    }
    return name;
  }
};

class ContextualTasksAppPixelTest
    : public ContextualTasksPixelTestBase,
      public testing::WithParamInterface<AppPixelTestParams> {
  void SetUpOnMainThread() override {
    SetDarkMode(GetParam().dark_mode);
    ContextualTasksPixelTestBase::SetUpOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksAppPixelTest,
    testing::ValuesIn<AppPixelTestParams>({
        // Light mode
        {.dark_mode = false, .is_side_panel = false, .is_zero_state = false},
        {.dark_mode = false, .is_side_panel = true, .is_zero_state = false},
        {.dark_mode = false, .is_side_panel = false, .is_zero_state = true},
        {.dark_mode = false,
         .is_side_panel = true,
         .is_zero_state = false,
         .is_ghost_loader = true},
        // Dark mode
        {.dark_mode = true, .is_side_panel = false, .is_zero_state = false},
        {.dark_mode = true, .is_side_panel = true, .is_zero_state = false},
        {.dark_mode = true, .is_side_panel = false, .is_zero_state = true},
        {.dark_mode = true,
         .is_side_panel = false,
         .is_zero_state = false,
         .is_ai_page = true},
        {.dark_mode = true,
         .is_side_panel = true,
         .is_zero_state = false,
         .is_ghost_loader = true},
    }),
    [](const testing::TestParamInfo<AppPixelTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(ContextualTasksAppPixelTest, Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const DeepQuery kApp = {"contextual-tasks-app"};
  const DeepQuery kAiPageWebView = {"contextual-tasks-app", "webview"};
  const DeepQuery kComposeBoxInput = {"contextual-tasks-app",
                                      "contextual-tasks-composebox",
                                      "#composebox", "textarea"};
  const DeepQuery kGhostLoader = {"contextual-tasks-app", "ghost-loader"};

  RunTestSequence(
      SetupWebUIEnvironment(kActiveTab,
                            GURL(chrome::kChromeUIContextualTasksURL),
                            {"contextual-tasks-app"}),
      Do([this]() {
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetSize(
            {360, 600});
      }),
      WaitForWebContentsPainted(kActiveTab),
      ExecuteJsAt(
          kActiveTab, kApp,
          base::StringPrintf("(el) => { "
                             "  el.isShownInTab_ = %s; "
                             "  el.isZeroState_ = %s; "
                             "  el.isAiPage_ = %s; "
                             "  el.isGhostLoaderVisible_ = %s; "
                             "  el.requestUpdate(); "
                             "}",
                             GetParam().is_side_panel ? "false" : "true",
                             GetParam().is_zero_state ? "true" : "false",
                             GetParam().is_ai_page ? "true" : "false",
                             GetParam().is_ghost_loader ? "true" : "false")),
      WaitForWebContentsPainted(kActiveTab),
      // Give the webview a green border to make it obvious where its bounds
      // are.
      ExecuteJsAt(kActiveTab, kAiPageWebView,
                  "(el) => { el.style.border = '1px solid green'; }"),
      // Disable the blinking caret to reduce flakiness.
      HideCaret(kActiveTab, kComposeBoxInput),
      // Modify ghost loader animation to avoid flakiness.
      ExecuteJsAt(
          kActiveTab, kGhostLoader,
          "(el) => { el.style.setProperty('--animation-delay', '120s'); }"),
      WaitForWebContentsPainted(kActiveTab),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshots not captured on this platform."),
      ScreenshotWebUi(kActiveTab, kApp, "ContextualTasksApp",
                      /*baseline_cl=*/"7519825"));
}

enum class TitleType { kNone, kShort, kLong };

struct ToolbarPixelTestParams {
  bool dark_mode = false;
  TitleType title_type = TitleType::kNone;
  bool menu_open = false;
  bool is_ai_page = true;
  bool rtl = false;

  std::string ToString() const {
    std::string name;
    name += dark_mode ? "Dark" : "Light";
    if (rtl) {
      name += "_RTL";
    }
    switch (title_type) {
      case TitleType::kNone:
        name += "_NoTitle";
        break;
      case TitleType::kShort:
        name += "_ShortTitle";
        break;
      case TitleType::kLong:
        name += "_LongTitle";
        break;
    }
    if (menu_open) {
      name += "_MenuOpen";
    }
    if (is_ai_page) {
      name += "_AiPage";
    }
    return name;
  }
};

class ContextualTasksToolbarPixelTest
    : public ContextualTasksPixelTestBase,
      public testing::WithParamInterface<ToolbarPixelTestParams> {
  void SetUpOnMainThread() override {
    SetRTL(GetParam().rtl);
    SetDarkMode(GetParam().dark_mode);
    ContextualTasksPixelTestBase::SetUpOnMainThread();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksToolbarPixelTest,
    testing::ValuesIn<ToolbarPixelTestParams>({
        // Light mode title variations
        {.title_type = TitleType::kNone},
        {.title_type = TitleType::kShort},
        {.title_type = TitleType::kLong},

        // Dark mode title variations
        {.dark_mode = true, .title_type = TitleType::kNone},
        {.dark_mode = true, .title_type = TitleType::kShort},
        {.dark_mode = true, .title_type = TitleType::kLong},

        // Test non AI page color scheme
        {.dark_mode = true, .is_ai_page = false},

        // RTL variations
        {.title_type = TitleType::kShort, .rtl = true},
        {.dark_mode = true, .title_type = TitleType::kLong, .rtl = true},

        // Open menu.
        {.menu_open = true},
        {.dark_mode = true, .menu_open = true},
    }),
    [](const testing::TestParamInfo<ToolbarPixelTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(ContextualTasksToolbarPixelTest, Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DeepQuery app = {"contextual-tasks-app"};
  DeepQuery toolbar = app + "top-toolbar";
  DeepQuery moreButton = toolbar + "#more";
  DeepQuery menu = toolbar + "cr-action-menu" + "dialog";

  RunTestSequence(
      SetupWebUIEnvironment(kActiveTab,
                            GURL(chrome::kChromeUIContextualTasksURL),
                            {"contextual-tasks-app"}),

      // The toolbar is only shown when the app is in side panel mode.
      ExecuteJsAt(kActiveTab, app,
                  base::StringPrintf("(el) => { "
                                     "  el.isShownInTab_ = false; "
                                     "  el.isAiPage_ = %s; "
                                     "  el.requestUpdate(); "
                                     "}",
                                     GetParam().is_ai_page ? "true" : "false")),
      WaitForWebContentsPainted(kActiveTab), EnsurePresent(kActiveTab, toolbar),
      ExecuteJsAt(kActiveTab, toolbar,
                  base::StringPrintf("(el) => { el.title = '%s'; }",
                                     GetParam().title_type == TitleType::kShort
                                         ? "Short Title"
                                     : GetParam().title_type == TitleType::kLong
                                         ? "A really long title that should "
                                           "ellipsize because it is too long"
                                         : "")),
      If([]() { return GetParam().menu_open; },
         Then(ExecuteJsAt(kActiveTab, moreButton, "(el) => el.click()"),
              EnsurePresent(kActiveTab, menu),
              WaitForWebContentsPainted(kActiveTab),
              SetOnIncompatibleAction(
                  OnIncompatibleAction::kIgnoreAndContinue,
                  "Screenshots not captured on this platform."),
              ScreenshotWebUi(kActiveTab, menu, "ContextualTasksToolbarMenu",
                              /*baseline_cl=*/"7519825")),
         Else(WaitForWebContentsPainted(kActiveTab),
              SetOnIncompatibleAction(
                  OnIncompatibleAction::kIgnoreAndContinue,
                  "Screenshots not captured on this platform."),
              ScreenshotWebUi(kActiveTab, toolbar, "ContextualTasksToolbar",
                              /*baseline_cl=*/"7519825"))));
}
