// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pixel tests for the default search engine picker dialog, opened from
// chrome://settings/search.
//
// The tests replace the prepopulated search engines with artificial ones, so
// that the screenshots don't change when the production lists of search
// engines are updated.
//
// To generate the screenshots locally, run:
//   out/Default/browser_tests \
//       --gtest_filter='SearchEnginePickerPixelBrowserTest.*' \
//       --browser-ui-tests-verify-pixels --enable-pixel-output-in-tests \
//       --bypass-skia-gold-functionality \
//       --skia-gold-local-png-write-directory=/tmp/pixel_test_output

#include <cstddef>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/test/pixel_test_configuration_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_switches.h"
#include "components/search_engines/search_engine_type.h"
#include "content/public/test/browser_test.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "url/gurl.h"

namespace {

using ::TemplateURLPrepopulateData::PrepopulatedEngine;
using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTabId);

// Baseline Gerrit CL number of the most recent CL that modified the UI of the
// default search engine picker.
constexpr char kPickerScreenshotBaselineCL[] = "8641569";

// Artificial prepopulated search engines, which replace the production ones
// through `ScopedPrepopulatedEnginesOverride`. They have no favicon URL, so
// they are all shown with the generic favicon.
//
// Since prepopulated engines are only looked up among the overriding ones,
// their IDs may also be used by production engines. Google's ID is avoided
// though, as it is special-cased: e.g. Google is preferred as the fallback
// default search engine, which is otherwise the first regional engine. The IDs
// also don't exceed `kMaxPrepopulatedEngineID`, so that the engines are handled
// like regular prepopulated engines.
constexpr PrepopulatedEngine kAlpha = {
    .name = u"Alpha",
    .keyword = u"alpha.test",
    .search_url = "https://alpha.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 4,
};

constexpr PrepopulatedEngine kBravo = {
    .name = u"Bravo",
    .keyword = u"bravo.test",
    .search_url = "https://bravo.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 5,
};

constexpr PrepopulatedEngine kCharlie = {
    .name = u"Charlie",
    .keyword = u"charlie.test",
    .search_url = "https://charlie.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 6,
};

constexpr PrepopulatedEngine kDelta = {
    .name = u"Delta",
    .keyword = u"delta.test",
    .search_url = "https://delta.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 7,
};

constexpr PrepopulatedEngine kEcho = {
    .name = u"Echo",
    .keyword = u"echo.test",
    .search_url = "https://echo.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 8,
};

constexpr PrepopulatedEngine kFoxtrot = {
    .name = u"Foxtrot",
    .keyword = u"foxtrot.test",
    .search_url = "https://foxtrot.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 9,
};

constexpr PrepopulatedEngine kGolf = {
    .name = u"Golf",
    .keyword = u"golf.test",
    .search_url = "https://golf.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 10,
};

constexpr PrepopulatedEngine kHotel = {
    .name = u"Hotel",
    .keyword = u"hotel.test",
    .search_url = "https://hotel.test/search?q={searchTerms}",
    .encoding = "UTF-8",
    .type = SEARCH_ENGINE_OTHER,
    .id = 11,
};

// Number of engines in the regional list, which are the ones listed in the
// picker.
constexpr size_t kRegionalEngineCount = 5;

// The search page of chrome://settings, from which the picker is opened.
const DeepQuery kSearchPage = {"settings-ui", "settings-main",
                               "settings-search-page-index",
                               "settings-search-page"};
const DeepQuery kPickerDialog =
    kSearchPage + "settings-search-engine-list-dialog";

// Format of a function returning whether the dialog it is called on is open and
// lists the number of engines given as format argument.
constexpr char kIsDialogShownJsFormat[] = R"js(
  (el) => {
    const dialog = el.shadowRoot.querySelector('cr-dialog');
    const engines = el.shadowRoot.querySelectorAll('cr-radio-button');
    return !!dialog && dialog.open && engines.length === %zu;
  })js";

// Variants in which each screenshot is taken.
struct RenderingVariant {
  const char* name;
  bool use_dark_theme = false;
  bool use_right_to_left_language = false;
};

constexpr RenderingVariant kRenderingVariants[] = {
    {.name = "Light"},
    {.name = "Dark", .use_dark_theme = true},
    {.name = "Rtl", .use_right_to_left_language = true},
};

}  // namespace

class SearchEnginePickerPixelBrowserTest
    : public InteractiveBrowserTestMixin<MixinBasedInProcessBrowserTest>,
      public testing::WithParamInterface<RenderingVariant> {
 protected:
  // Waits until `dialog` is open and lists `engine_count` engines.
  auto WaitForDialog(const DeepQuery& dialog, size_t engine_count) {
    return WaitForJsResultAt(
        kSettingsTabId, dialog,
        base::StringPrintf(kIsDialogShownJsFormat, engine_count));
  }

  // Navigates to chrome://settings/search and opens the picker.
  auto OpenPicker() {
    const DeepQuery kOpenPickerButton = kSearchPage + "#openDialogButton";
    return Steps(
        InstrumentTab(kSettingsTabId),
        NavigateWebContents(kSettingsTabId,
                            GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                               chrome::kSearchSubPage}))),
        WaitForElementVisible(kSettingsTabId, kOpenPickerButton),
        ClickElement(kSettingsTabId, kOpenPickerButton),
        WaitForDialog(kPickerDialog, kRegionalEngineCount));
  }

  // Takes a screenshot of `dialog`.
  auto ScreenshotDialog(const DeepQuery& dialog,
                        const std::string& screenshot_name,
                        const std::string& baseline_cl) {
    return Steps(
        SetOnIncompatibleAction(
            OnIncompatibleAction::kIgnoreAndContinue,
            "Screenshots are not supported in all testing environments."),
        ScreenshotWebUi(kSettingsTabId, dialog + "cr-dialog" + "dialog",
                        screenshot_name, baseline_cl));
  }

 private:
  PixelTestConfigurationMixin pixel_test_mixin_{
      &mixin_host_, GetParam().use_dark_theme,
      GetParam().use_right_to_left_language};

  // The dialogs are tested as they are shown with this feature, which is set
  // explicitly so that the tests don't depend on field trial configs.
  base::test::ScopedFeatureList feature_list_{
      switches::kSearchSettingsWithMoreEngines};

  regional_capabilities::ScopedPrepopulatedEnginesOverride
      prepopulated_engines_override_ =
          regional_capabilities::SetPrepopulatedEnginesOverrideForTesting(
              /*regional_engines=*/{&kAlpha, &kBravo, &kCharlie, &kDelta,
                                    &kEcho},
              /*other_known_engines=*/{&kFoxtrot, &kGolf, &kHotel});
};

IN_PROC_BROWSER_TEST_P(SearchEnginePickerPixelBrowserTest, Picker) {
  RunTestSequence(OpenPicker(),
                  ScreenshotDialog(kPickerDialog, "SearchEnginePicker",
                                   kPickerScreenshotBaselineCL));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEnginePickerPixelBrowserTest,
    testing::ValuesIn(kRenderingVariants),
    [](const testing::TestParamInfo<RenderingVariant>& info) {
      return std::string(info.param.name);
    });
