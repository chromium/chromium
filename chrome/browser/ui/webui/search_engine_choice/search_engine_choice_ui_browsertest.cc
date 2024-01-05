// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/test/pixel_test_configuration_mixin.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/any_widget_observer.h"

#if !BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
#error Platform not supported
#endif

// Tests for the chrome://search-engine-choice WebUI page.
namespace {
// This is the maximum dialog height for pixel tests on Windows.
constexpr int kMaximumHeight = 620;

// Class that mocks `SearchEngineChoiceService`.
class MockSearchEngineChoiceService : public SearchEngineChoiceService {
 public:
  explicit MockSearchEngineChoiceService(Profile* profile)
      : SearchEngineChoiceService(
            *profile,
            *TemplateURLServiceFactory::GetForProfile(profile)) {
    ON_CALL(*this, GetSearchEngines).WillByDefault([]() {
      std::vector<std::unique_ptr<TemplateURL>> choices;
      auto choice = TemplateURLData();

      for (int i = 0; i < 12; i++) {
        const std::u16string kShortName = u"Test" + base::NumberToString16(i);
        // Start from 1 because a `prepopulate_id` of 0 is for custom search
        // engines.
        choice.prepopulate_id = i + 1;
        choice.SetShortName(kShortName);
        choices.push_back(std::make_unique<TemplateURL>(choice));
      }
      return choices;
    });
  }
  ~MockSearchEngineChoiceService() override = default;

  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return std::make_unique<testing::NiceMock<MockSearchEngineChoiceService>>(
        Profile::FromBrowserContext(context));
  }

  MOCK_METHOD(std::vector<std::unique_ptr<TemplateURL>>,
              GetSearchEngines,
              (),
              (override));
};

struct TestParam {
  std::string test_suffix;
  bool use_dark_theme = false;
  bool use_right_to_left_language = false;
  bool show_search_engine_omnibox = false;
  bool with_marketing_snippets = false;
  gfx::Size dialog_dimensions = gfx::Size(988, 900);
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported parameters.
const TestParam kTestParams[] = {
#if BUILDFLAG(IS_WIN)
    // TODO(crbug.com/1515948): Fix and re-add test with
    // "ShowSearchEngineOmnibox" parameter.
    {.test_suffix = "Default"},
    {.test_suffix = "WithMarketingSnippets", .with_marketing_snippets = true},
    {.test_suffix = "WithMarketingSnippetsDarkTheme",
     .use_dark_theme = true,
     .with_marketing_snippets = true},
    {.test_suffix = "RightToLeft", .use_right_to_left_language = true},
    {.test_suffix = "WithMarketingSnippetsRightToLeft",
     .use_right_to_left_language = true,
     .with_marketing_snippets = true},
    {.test_suffix = "MediumSize", .dialog_dimensions = gfx::Size(800, 700)},
    {.test_suffix = "NarrowSize", .dialog_dimensions = gfx::Size(300, 900)},
#endif
    // We enable the test on platforms other than Windows with the smallest
    // height due to a small maximum window height set by the operating system.
    // The test will crash if we exceed that height.
    {.test_suffix = "ShortSize", .dialog_dimensions = gfx::Size(988, 376)},
};

class SearchEngineChoiceNavigationObserver
    : public content::TestNavigationObserver {
 public:
  explicit SearchEngineChoiceNavigationObserver(GURL url)
      : content::TestNavigationObserver(url) {}

  void NavigationOfInterestDidFinish(
      content::NavigationHandle* navigation_handle) override {
    web_contents_ = navigation_handle->GetWebContents();
  }

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

// Click on a search engine to display the search engine omnibox.
const char kShowSearchEngineOmniboxJsString[] =
    "(() => {"
    "  const app = document.querySelector('search-engine-choice-app');"
    "  const searchEngineList = app.shadowRoot.querySelectorAll("
    "      'cr-radio-button');"
    "  searchEngineList[0].click();"
    "  return true;"
    "})();";
}  // namespace

class SearchEngineChoiceUIPixelTest
    : public TestBrowserDialog,
      public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  SearchEngineChoiceUIPixelTest()
      : scoped_chrome_build_override_(SearchEngineChoiceServiceFactory::
                                          ScopedChromeBuildOverrideForTesting(
                                              /*force_chrome_build=*/true)),
        pixel_test_mixin_(&mixin_host_,
                          GetParam().use_dark_theme,
                          GetParam().use_right_to_left_language) {
    const std::string featureParam =
        GetParam().with_marketing_snippets ? "true" : "false";
    feature_list_.InitAndEnableFeatureWithParameters(
        switches::kSearchEngineChoice,
        {{"with-marketing-snippets", featureParam}});
  }

  ~SearchEngineChoiceUIPixelTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  SearchEngineChoiceServiceFactory::GetInstance()
                      ->SetTestingFactoryAndUse(
                          context, base::BindRepeating(
                                       &MockSearchEngineChoiceService::Create));
                }));
  }

  // TestBrowserDialog
  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    SearchEngineChoiceService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);

    GURL url = GURL(chrome::kChromeUISearchEngineChoiceURL);
    SearchEngineChoiceNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{}, "SearchEngineChoiceDialogView");

    double zoom_factor = 1;
    double requested_width = GetParam().dialog_dimensions.width();
    double requested_height = GetParam().dialog_dimensions.height();
    double dialog_width = requested_width;
    double dialog_height = requested_height;

    // Override the default zoom factor for the Search Engine Choice dialog.
    // We can't modify the dialog's height because of the very small max-height
    // set by the pixel test window. Instead we change the content zoom factor
    // to simulate having multiple height variants. We then calculate the width
    // to be passed based on that zoom factor;
    if (requested_height > kMaximumHeight) {
      zoom_factor = kMaximumHeight / requested_height;
      dialog_width = requested_width * zoom_factor;
      dialog_height = kMaximumHeight;
    }

    ShowSearchEngineChoiceDialog(
        *browser(), gfx::Size(dialog_width, dialog_height), zoom_factor);
    widget_waiter.WaitIfNeededAndGet();
    content::WebContents* web_contents = observer.web_contents();
    CHECK(web_contents);

    if (GetParam().show_search_engine_omnibox) {
      EXPECT_EQ(true, content::EvalJs(web_contents,
                                      kShowSearchEngineOmniboxJsString));
    }

    observer.Wait();
  }

 private:
  base::AutoReset<bool> scoped_chrome_build_override_;
  base::test::ScopedFeatureList feature_list_;
  PixelTestConfigurationMixin pixel_test_mixin_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_P(SearchEngineChoiceUIPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SearchEngineChoiceUIPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
