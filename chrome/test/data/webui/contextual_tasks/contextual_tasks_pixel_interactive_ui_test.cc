// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base class for Contextual Tasks pixel tests.
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
// follow the pattern `ContextualTasks*PixelTest*`. If not, the test needs to
// manually be added to `testing/buildbot/filters/pixel_tests.filter`.

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/data/webui/webui_composebox_pixel_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

class FakeContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  explicit FakeContextualTasksUiService(
      Profile* profile,
      AimEligibilityService* aim_eligibility_service)
      : contextual_tasks::ContextualTasksUiService(
            profile,
            std::make_unique<testing::NiceMock<
                contextual_tasks::MockContextualTasksUiServiceDelegate>>(),
            /*contextual_tasks_service=*/nullptr,
            /*identity_manager=*/nullptr,
            aim_eligibility_service,
            /*eligibility_manager=*/nullptr,
            /*cookie_synchronizer=*/nullptr) {}
  GURL GetDefaultAiPageUrl() override { return GURL(url::kAboutBlankURL); }

  bool IsAiUrl(const GURL& url) override { return true; }
};

class ContextualTasksPixelTestBase : public WebUIComposeBoxPixelTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{contextual_tasks::kContextualTasks,
          {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {contextual_tasks::kContextualTasksContextMenu, {}}},
        /*disabled_features=*/
        {contextual_tasks::kContextualTasksAnimatedCaret,
         // TODO(crbug.com/452061489): Fix tests that fail when the WebUI
         // Omnibox is enabled and then remove these two Features.
         omnibox::internal::kWebUIOmniboxPopup,
         omnibox::internal::kWebUIOmniboxAimPopup});
    WebUIComposeBoxPixelTest::SetUp();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto service =
              std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
                  *Profile::FromBrowserContext(context)->GetPrefs(), nullptr,
                  nullptr, nullptr);
          ON_CALL(*service, IsAimEligible())
              .WillByDefault(testing::Return(true));

          static base::NoDestructor<omnibox::AimEligibilityResponse> response;
          response->set_is_eligible(true);
          response->set_is_fusebox_eligible(true);
          response->set_is_cobrowse_eligible(true);
          auto* config = response->mutable_searchbox_config();
          auto* tool_config = config->add_tool_configs();
          tool_config->set_tool(omnibox::TOOL_MODE_DEEP_SEARCH);
          tool_config->mutable_rule()->set_allow_all_input_types(true);

          auto* input_config1 = config->add_input_type_configs();
          input_config1->set_input_type(omnibox::INPUT_TYPE_LENS_IMAGE);
          auto* input_config2 = config->add_input_type_configs();
          input_config2->set_input_type(omnibox::INPUT_TYPE_LENS_FILE);
          auto* input_config3 = config->add_input_type_configs();
          input_config3->set_input_type(omnibox::INPUT_TYPE_BROWSER_TAB);

          ON_CALL(*service, GetMostRecentResponse())
              .WillByDefault(testing::ReturnRef(*response));
          ON_CALL(*service, GetSearchboxConfig())
              .WillByDefault(
                  testing::Return(response->mutable_searchbox_config()));
          return service;
        }));

    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeContextualTasksUiService>(
                  Profile::FromBrowserContext(context),
                  AimEligibilityServiceFactory::GetForProfile(
                      Profile::FromBrowserContext(context)));
            }));
  }

  void SetUpOnMainThread() override {
    WebUIComposeBoxPixelTest::SetUpOnMainThread();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());

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

// TODO(http://crbug.com/542250614): Fix and reenable.
IN_PROC_BROWSER_TEST_P(ContextualTasksComposeBoxPixelTest,
                       DISABLED_Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const DeepQuery kApp = {"contextual-tasks-app"};

  // DeepQuery needed to target elements with injected JS.
  const DeepQuery kComposebox = {"contextual-tasks-app",
                                 "contextual-tasks-composebox", "#composebox"};
  const DeepQuery kComposeBoxInput = {
      "contextual-tasks-app", "contextual-tasks-composebox", "#composebox",
      "cr-composebox-input", "textarea"};
  const DeepQuery kAiPageWebView = {"contextual-tasks-app", "webview"};

  RunTestSequence(
      SetupWebUIEnvironment(kActiveTab,
                            GURL(chrome::kChromeUIContextualTasksURL),
                            {"contextual-tasks-app"}),

      // Ensure the composebox exists.
      EnsurePresent(kActiveTab, kComposebox),

      ExecuteJsAt(kActiveTab, kApp,
                  base::StringPrintf(
                      R"((el) => {
                el.isAiPage_ = %s;
                el.isAimEligible_ = true;
                el.isShownInTab_ = false;
                el.isZeroState_ = true;
                el.isInputHidden_ = false;
                el.isComposeboxHidden_ = () => false;
                if (el.requestUpdate) el.requestUpdate();

                const inputState = {
                  allowedModels: [],
                  allowedTools: [1],
                  allowedInputTypes: [1, 2, 3],
                  disabledModels: [],
                  disabledTools: [],
                  disabledInputTypes: [],
                  activeModel: 0,
                  activeTool: 0,
                  toolConfigs: [],
                  modelConfigs: [],
                  inputTypeConfigs: [],
                  hintText: '',
                  maxInputsByType: {},
                  maxTotalInputs: 10,
                  isCanvasQuerySubmitted: false,
                };

                const composebox = el.shadowRoot ? el.shadowRoot.querySelector('contextual-tasks-composebox') : null;
                if (composebox) {
                  composebox.inputState_ = inputState;
                  composebox.removeAttribute('hidden');
                  composebox.style.cssText += '; display: flex !important; opacity: 1 !important; visibility: visible !important;';
                  if (composebox.requestUpdate) composebox.requestUpdate();
                }

                const inner = composebox && composebox.shadowRoot ? composebox.shadowRoot.querySelector('#composebox') : null;
                if (inner) {
                  inner.inputState = inputState;
                  inner.style.cssText += '; display: block !important; opacity: 1 !important; visibility: visible !important;';
                  if (inner.requestUpdate) inner.requestUpdate();
                }
              })",
                      GetParam().is_ai_page ? "true" : "false")),
      WaitForWebContentsPainted(kActiveTab),

      // Ensure the AI page webview is loaded with about:blank if is_ai_page is
      // true.
      If([]() { return GetParam().is_ai_page; },
         Then(CheckJsResultAt(kActiveTab, kAiPageWebView, "(el) => el.src",
                              url::kAboutBlankURL))),

      // Apply focus or blur according to test parameter.
      If([]() { return GetParam().focused; },
         Then(ExecuteJsAt(kActiveTab, kComposeBoxInput, "(el) => el.focus()")),
         Else(ExecuteJsAt(kActiveTab, kComposeBoxInput, "(el) => el.blur()"))),

      // Set the composebox text if specified.
      If([]() { return GetParam().with_text; },
         Then(ExecuteJsAt(kActiveTab, kComposeBoxInput,
                          R"((el) => {
                           el.value = 'some text';
                           el.dispatchEvent(new Event('input', {bubbles:
                           true, composed: true}));
                         })"))),

      // Disable the blinking caret to reduce flakiness.
      HideCaret(kActiveTab, kComposeBoxInput),

      // Disable animations, enforce static glow states, and await Lit updates
      // before screenshot.
      ExecuteJsAt(kActiveTab, kApp, R"(async (el) => {
        const sheet = new CSSStyleSheet();
        sheet.replaceSync(`
          *, *::before, *::after {
            transition: none !important;
            animation: none !important;
          }
          .gradient, .double-gradient, .glow-container {
            display: none !important;
            opacity: 0 !important;
            visibility: hidden !important;
            animation: none !important;
          }
        `);

        async function prepareAndAwait(root) {
          if (!root) return;
          if (root.adoptedStyleSheets && !root.adoptedStyleSheets.includes(sheet)) {
            root.adoptedStyleSheets = [...root.adoptedStyleSheets, sheet];
          }
          if (root.host) {
            if ('animationState' in root.host) root.host.animationState = 'NONE';
            if ('glifAnimationState' in root.host) root.host.glifAnimationState = 'INELIGIBLE';
            if ('energyEffectAnimationEnabled' in root.host) root.host.energyEffectAnimationEnabled = false;
            if (root.host.updateComplete) await root.host.updateComplete;
          }
          const children = root.querySelectorAll('*');
          for (const child of children) {
            if (child.shadowRoot) {
              await prepareAndAwait(child.shadowRoot);
            }
          }
        }
        await prepareAndAwait(document);
        await prepareAndAwait(el.shadowRoot || el);
      })"),
      WaitForWebContentsPainted(kActiveTab),

      // This step is needed to prevent test from failing on platforms that
      // don't support screenshots.
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshots not captured on this platform."),

      // Take a screenshot of the composebox.
      ScreenshotWebUi(kActiveTab, kComposebox,
                      /*screenshot_name=*/"ContextualTasksComposebox",
                      /*baseline_cl=*/"8142019"));
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

// TODO(crbug.com/499019938): Fix and reenable.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Screenshots DISABLED_Screenshots
#else
#define MAYBE_Screenshots Screenshots
#endif
IN_PROC_BROWSER_TEST_P(ContextualTasksAppPixelTest, MAYBE_Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  const DeepQuery kApp = {"contextual-tasks-app"};
  const DeepQuery kAiPageWebView = {"contextual-tasks-app", "webview"};
  const DeepQuery kComposeBoxInput = {
      "contextual-tasks-app", "contextual-tasks-composebox", "#composebox",
      "cr-composebox-input", "textarea"};
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
                      /*baseline_cl=*/"7620222"));
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
        {
            .menu_open = true,
            .is_ai_page = true,
        },
        {.dark_mode = true, .menu_open = true, .is_ai_page = true},
    }),
    [](const testing::TestParamInfo<ToolbarPixelTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(ContextualTasksToolbarPixelTest, Screenshots) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DeepQuery app = {"contextual-tasks-app"};
  DeepQuery toolbar = app + "top-toolbar";
  DeepQuery moreButton = toolbar + "#overflowMenuButton";
  DeepQuery menu =
      toolbar + "contextual-tasks-overflow-menu" + "cr-action-menu" + "dialog";

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
                              /*baseline_cl=*/"7620222")),
         Else(WaitForWebContentsPainted(kActiveTab),
              SetOnIncompatibleAction(
                  OnIncompatibleAction::kIgnoreAndContinue,
                  "Screenshots not captured on this platform."),
              ScreenshotWebUi(kActiveTab, toolbar, "ContextualTasksToolbar",
                              /*baseline_cl=*/"7620222"))));
}
