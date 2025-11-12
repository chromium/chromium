// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using metrics::OmniboxEventProto;
using ui_test_utils::WaitForAutocompleteDone;

class OmniboxMetricsTest : public InProcessBrowserTest {
 public:
  OmniboxMetricsTest() = default;
  ~OmniboxMetricsTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // The omnibox suggestion results depend on the TemplateURLService being
    // loaded. Make sure it is loaded so that the autocomplete results are
    // consistent.
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));

    // Prevent the stop timer from killing the hints fetch early, which might
    // cause test flakiness due to timeout.
    controller()->config_.stop_timer_duration = base::Seconds(30);
  }

 protected:
  AutocompleteController* controller() {
    return browser()
        ->window()
        ->GetLocationBar()
        ->GetOmniboxController()
        ->autocomplete_controller();
  }

  OmniboxEditModel* model() {
    return browser()
        ->window()
        ->GetLocationBar()
        ->GetOmniboxController()
        ->edit_model();
  }
};

IN_PROC_BROWSER_TEST_F(OmniboxMetricsTest, LogSearchEngineUsed) {
  AutocompleteInput input(
      u"z", metrics::OmniboxEventProto::NTP,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  controller()->Start(input);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(controller()->done());

  const AutocompleteResult& result = controller()->result();
  ASSERT_FALSE(result.empty());
  const AutocompleteMatch& match = result.match_at(0);
  EXPECT_EQ(u"google.com", match.keyword);
  EXPECT_EQ(u"z", match.fill_into_edit);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            result.match_at(0).type);

  base::HistogramTester histogram_tester;
  model()->SetPopupSelection(OmniboxPopupSelection(0));
  model()->OpenSelectionForTesting(base::TimeTicks(),
                                   WindowOpenDisposition::CURRENT_TAB);
  WaitForAutocompleteDone(browser());
  EXPECT_TRUE(controller()->done());

  histogram_tester.ExpectUniqueSample("Omnibox.SearchEngineType",
                                      SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchEngineType.Fallback.DefaultSearchProvider",
      SEARCH_ENGINE_GOOGLE, 1);
}
