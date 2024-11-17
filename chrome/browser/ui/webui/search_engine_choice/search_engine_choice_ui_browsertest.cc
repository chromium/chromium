// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/test/pixel_test_configuration_mixin.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/any_widget_observer.h"

// Tests for the chrome://search-engine-choice WebUI page.
namespace {
// This is the maximum dialog height for pixel tests on Windows.
constexpr int kMaximumHeight = 620;

// Class that mocks `SearchEngineChoiceDialogService`.
class MockSearchEngineChoiceDialogService
    : public SearchEngineChoiceDialogService {
 public:
  explicit MockSearchEngineChoiceDialogService(Profile* profile)
      : SearchEngineChoiceDialogService(
            *profile,
            *search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
                profile),
            *TemplateURLServiceFactory::GetForProfile(profile)) {
    ON_CALL(*this, GetSearchEngines).WillByDefault([&]() {
      TemplateURL::TemplateURLVector choices;
      for (auto& choice : GetSearchEnginesInternal()) {
        choices.push_back(choice.get());
      }
      return choices;
    });
  }
  ~MockSearchEngineChoiceDialogService() override = default;

  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return std::make_unique<
        testing::NiceMock<MockSearchEngineChoiceDialogService>>(
        Profile::FromBrowserContext(context));
  }

  MOCK_METHOD(TemplateURL::TemplateURLVector, GetSearchEngines, (), (override));

 private:
  const TemplateURL::OwnedTemplateURLVector& GetSearchEnginesInternal() {
    if (choices_.empty()) {
      auto choice = TemplateURLData();

      // Current design is built around having 8 items, but the max is defined
      // acknowledging that we have some exceptions where there are more items.
      const size_t kItemsCount = 8;
      static_assert(kItemsCount <=
                    TemplateURLPrepopulateData::kMaxEeaPrepopulatedEngines);

      for (size_t i = 0; i < kItemsCount; i++) {
        const std::u16string kShortName = u"Test" + base::NumberToString16(i);
        // Start from 1 because a `prepopulate_id` of 0 is for custom search
        // engines.
        choice.prepopulate_id = i + 1;
        choice.SetShortName(kShortName);
        if (i % 2 == 0) {
          // The bing icon should be bundled with Chrome.
          choice.SetKeyword(TemplateURLPrepopulateData::bing.keyword);
        } else {
          // Uses the default generic favicon.
          choice.SetKeyword(TemplateURLPrepopulateData::incredibar.keyword);
        }
        choices_.push_back(std::make_unique<TemplateURL>(choice));
      }
    }

    return choices_;
  }

  TemplateURL::OwnedTemplateURLVector choices_;
};

struct TestParam {
  std::string test_suffix;
  bool use_dark_theme = false;
  bool use_right_to_left_language = false;
  bool select_first_search_engine = false;
  bool first_snippet_text_larger = false;
  bool display_info_dialog = false;
  bool wait_for_banners_displayed = true;
  bool is_guest_session = false;
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
    {.test_suffix = "Default"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
    {.test_suffix = "RightToLeft", .use_right_to_left_language = true},
    {.test_suffix = "MediumSize",
     .wait_for_banners_displayed = false,
     .dialog_dimensions = gfx::Size(800, 700)},
    {.test_suffix = "NarrowSize",
     .wait_for_banners_displayed = false,
     .dialog_dimensions = gfx::Size(300, 900)},
    {.test_suffix = "ShortAndNarrowSize",
     .wait_for_banners_displayed = false,
     .dialog_dimensions = gfx::Size(500, 500)},
    {.test_suffix = "LargerFirstEngineSnippet",
     .first_snippet_text_larger = true},
    // TODO(b/360286412): This test case is flaky.
    // {.test_suffix = "FirstEngineSelectedWithLargerSnippet",
    //  .select_first_search_engine = true,
    //  .first_snippet_text_larger = true},
    {.test_suffix = "InfoDialog", .display_info_dialog = true},
    {.test_suffix = "InfoDialogDarkTheme",
     .use_dark_theme = true,
     .display_info_dialog = true},
    {.test_suffix = "Guest", .is_guest_session = true},
    {.test_suffix = "GuestRtl",
     .use_right_to_left_language = true,
     .is_guest_session = true},
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

const char kSelectFirstSearchEngineJsString[] =
    "(() => {"
    "  const app = document.querySelector('search-engine-choice-app');"
    "  const searchEngineList = app.shadowRoot.querySelectorAll("
    "      'cr-radio-button');"
    "  searchEngineList[0].click();"
    "  return true;"
    "})();";

const char kMakeFirstSnippetLargerJsString[] =
    "(() => {"
    "const app = document.querySelector('search-engine-choice-app');"
    "const marketingSnippet = "
    "app.shadowRoot.querySelectorAll('.marketing-snippet');"
    "marketingSnippet[0].textContent = "
    "marketingSnippet[0].textContent.repeat(3);"
    "return true;"
    "})();";

const char kDisplayInfoDialogJsString[] =
    "(() => {"
    "const app = document.querySelector('search-engine-choice-app');"
    "app.shadowRoot.querySelector('#infoLink').click();"
    "return true;"
    "})();";

// We remove the hover property to prevent the test from being flaky.
const char kRemoveHoverPropertyJsString[] =
    "(() => {"
    "const app = document.querySelector('search-engine-choice-app');"
    "const radioButtons = app.shadowRoot.querySelectorAll('cr-radio-button');"
    "radioButtons.forEach(button => button.classList.remove('hoverable'));"
    "return true;"
    "})();";

const char kAreBannersDisplayedJsString[] =
    "(() => {"
    "const app = document.querySelector('search-engine-choice-app');"
    "const leftBannerStyle = "
    "getComputedStyle(app.shadowRoot.querySelector('#leftBanner'));"
    "const rightBannerStyle = "
    "getComputedStyle(app.shadowRoot.querySelector('#rightBanner'));"
    "return rightBannerStyle.display === 'block' && leftBannerStyle.display "
    "=== 'block';"
    "})();";

void WaitForBannersDisplayed(content::WebContents* web_contents,
                             base::OnceClosure quit_closure) {
  if (content::EvalJs(web_contents, kAreBannersDisplayedJsString) == true) {
    std::move(quit_closure).Run();
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitForBannersDisplayed, web_contents,
                     std::move(quit_closure)),
      TestTimeouts::tiny_timeout());
}

}  // namespace

class SearchEngineChoiceUIPixelTest
    : public TestBrowserDialog,
      public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  SearchEngineChoiceUIPixelTest()
      : scoped_chrome_build_override_(SearchEngineChoiceDialogServiceFactory::
                                          ScopedChromeBuildOverrideForTesting(
                                              /*force_chrome_build=*/true)),
        pixel_test_mixin_(&mixin_host_,
                          GetParam().use_dark_theme,
                          GetParam().use_right_to_left_language) {}

  ~SearchEngineChoiceUIPixelTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    if (GetParam().is_guest_session) {
      ui_test_utils::BrowserChangeObserver browser_added_observer(
          nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

      CreateGuestBrowser();
      Browser* new_browser = browser_added_observer.Wait();
      ASSERT_TRUE(new_browser);
      ASSERT_NE(new_browser, browser());
      ASSERT_TRUE(new_browser->profile()->IsGuestSession());

      CloseBrowserSynchronously(browser());
      SelectFirstBrowser();
      ASSERT_EQ(new_browser, browser());
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  SearchEngineChoiceDialogServiceFactory::GetInstance()
                      ->SetTestingFactoryAndUse(
                          context,
                          base::BindRepeating(
                              &MockSearchEngineChoiceDialogService::Create));
                }));
  }

  // TestBrowserDialog
  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
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

    EXPECT_EQ(true,
              content::EvalJs(web_contents, kRemoveHoverPropertyJsString));

    if (GetParam().select_first_search_engine) {
      EXPECT_EQ(true, content::EvalJs(web_contents,
                                      kSelectFirstSearchEngineJsString));
    }

    if (GetParam().first_snippet_text_larger) {
      EXPECT_EQ(true,
                content::EvalJs(web_contents, kMakeFirstSnippetLargerJsString));
    }

    if (GetParam().display_info_dialog) {
      EXPECT_EQ(true,
                content::EvalJs(web_contents, kDisplayInfoDialogJsString));
    }

    if (GetParam().wait_for_banners_displayed) {
      base::RunLoop run_loop;
      WaitForBannersDisplayed(web_contents, run_loop.QuitClosure());
      run_loop.Run();
    }

    observer.Wait();
  }

 private:
  base::AutoReset<bool> scoped_chrome_build_override_;
  base::test::ScopedFeatureList feature_list_{
      switches::kSearchEngineChoiceGuestExperience};
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
