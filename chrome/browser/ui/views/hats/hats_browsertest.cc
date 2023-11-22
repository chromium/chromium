// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hats/hats_next_web_dialog.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "url/gurl.h"

namespace {

// The product specific data expected by the test survey. The boolean values are
// checked in hats_next_mock.html.
const SurveyBitsData kHatsNextTestSurveyProductSpecificBitsData{
    {"Test Field 1", true},
    {"Test Field 2", false}};

const SurveyStringData kHatsNextTestSurveyProductSpecificStringData{
    {"Test Field 3", "Test value"}};

// The locale expected by the test survey. This value is checked in
// hats_next_mock.html for tests that expect a loaded response.
const char kTestLocale[] = "lt";

}  // namespace

class MockHatsNextWebDialog : public HatsNextWebDialog {
 public:
  MockHatsNextWebDialog(Browser* browser,
                        const std::string& trigger_id,
                        const GURL& hats_survey_url,
                        const base::TimeDelta& timeout,
                        base::OnceClosure success_callback,
                        base::OnceClosure failure_callback,
                        const SurveyBitsData& product_specific_bits_data,
                        const SurveyStringData& product_specific_string_data)
      : HatsNextWebDialog(browser,
                          trigger_id,
                          hats_survey_url,
                          timeout,
                          std::move(success_callback),
                          std::move(failure_callback),
                          product_specific_bits_data,
                          product_specific_string_data) {}

  MOCK_METHOD(void, ShowWidget, (), (override));
  MOCK_METHOD(void, CloseWidget, (), (override));
  MOCK_METHOD(void, UpdateWidgetSize, (), (override));

  void WaitForClose() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, CloseWidget).WillOnce([&]() {
      widget_->Close();
      run_loop.Quit();
    });
    run_loop.Run();
  }

  void WaitForUpdateWidgetSize() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, UpdateWidgetSize).WillOnce(testing::Invoke([&run_loop] {
      run_loop.Quit();
    }));
    run_loop.Run();
  }
};

class HatsNextWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  }

  // Open a blank tab in the main browser, inspect it, and return the devtools
  // Browser for the undocked devtools window.
  Browser* OpenUndockedDevToolsWindow() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

    const bool is_docked = false;
    DevToolsWindow* devtools_window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), is_docked);
    return devtools_window->browser_;
  }

  MockHatsService* hats_service() { return hats_service_; }

  base::OnceClosure GetSuccessClosure() {
    return base::BindLambdaForTesting([&]() { ++success_count; });
  }

  base::OnceClosure GetFailureClosure() {
    return base::BindLambdaForTesting([&]() { ++failure_count; });
  }

  int success_count = 0;
  int failure_count = 0;

 private:
  raw_ptr<MockHatsService, DanglingUntriaged> hats_service_;
};

// Test that the web dialog correctly receives change to history state that
// indicates a survey is ready to be shown.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyLoaded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Use the preference path constants defined in hats_service.cc.
  const std::string kLastSurveyStartedTime =
      std::string(kHatsSurveyTriggerTesting) + ".last_survey_started_time";
  const std::string kLastMajorVersion =
      std::string(kHatsSurveyTriggerTesting) + ".last_major_version";

  ScopedBrowserLocale browser_locale(kTestLocale);

  auto* dialog = new MockHatsNextWebDialog(
      browser(), kHatsNextSurveyTriggerIDTesting,
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), GetSuccessClosure(), GetFailureClosure(),
      kHatsNextTestSurveyProductSpecificBitsData,
      kHatsNextTestSurveyProductSpecificStringData);

  // Check that no record of a survey being shown is present.
  {
    const base::Value::Dict& pref_data =
        browser()->profile()->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);
    absl::optional<base::Time> last_survey_started_time =
        base::ValueToTime(pref_data.FindByDottedPath(kLastSurveyStartedTime));
    absl::optional<int> last_major_version =
        pref_data.FindIntByDottedPath(kLastMajorVersion);
    ASSERT_FALSE(last_survey_started_time.has_value());
    ASSERT_FALSE(last_major_version.has_value());
  }

  // The hats_next_mock.html will provide a state update to the dialog to
  // indicate that the survey has been loaded.
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, ShowWidget)
      .WillOnce(testing::Invoke([dialog, &run_loop]() {
        EXPECT_FALSE(dialog->IsWaitingForSurveyForTesting());
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(1, success_count);
  EXPECT_EQ(0, failure_count);

  // Check that a record of the survey being shown has been recorded.
  {
    const base::Value::Dict& pref_data =
        browser()->profile()->GetPrefs()->GetDict(prefs::kHatsSurveyMetadata);
    absl::optional<base::Time> last_survey_started_time =
        base::ValueToTime(pref_data.FindByDottedPath(kLastSurveyStartedTime));
    absl::optional<int> last_major_version =
        pref_data.FindIntByDottedPath(kLastMajorVersion);
    ASSERT_TRUE(last_survey_started_time.has_value());
    ASSERT_TRUE(last_major_version.has_value());
    ASSERT_EQ(static_cast<uint32_t>(*last_major_version),
              version_info::GetVersion().components()[0]);
  }
}

// Test that the web dialog correctly receives change to history state that
// indicates the survey window should be closed.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyClosed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "close_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), GetSuccessClosure(), GetFailureClosure(), {}, {});

  // The hats_next_mock.html will provide a state update to the dialog to
  // indicate that the survey window should be closed.
  dialog->WaitForClose();

  EXPECT_EQ(0, success_count);
  EXPECT_EQ(1, failure_count);

  // Because no loaded state was provided, only a rejection should be recorded.
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoRejectedByHatsService, 1);
}

// Test that a survey which first reports as loaded, then reports closure, only
// logs that the survey was shown.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyLoadedThenClosed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  ScopedBrowserLocale browser_locale(kTestLocale);

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), kHatsNextSurveyTriggerIDTesting,
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), GetSuccessClosure(), GetFailureClosure(),
      kHatsNextTestSurveyProductSpecificBitsData,
      kHatsNextTestSurveyProductSpecificStringData);
  dialog->WaitForClose();

  EXPECT_EQ(1, success_count);
  EXPECT_EQ(0, failure_count);

  // The only recorded sample should indicate that the survey was shown.
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kYes, 1);
}

// Test that if the survey does not indicate it is ready for display before the
// timeout the widget is closed.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, SurveyTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "invalid_test",
      embedded_test_server()->GetURL("/hats/non_existent.html"),
      base::Milliseconds(1), GetSuccessClosure(), GetFailureClosure(), {}, {});

  dialog->WaitForClose();

  EXPECT_EQ(0, success_count);
  EXPECT_EQ(1, failure_count);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoSurveyUnreachable, 1);
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, UnknownURLFragment) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Check that providing an unknown URL fragment results in the dialog being
  // closed.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "invalid_url_fragment_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), GetSuccessClosure(), GetFailureClosure(), {}, {});

  dialog->WaitForClose();
  EXPECT_EQ(0, success_count);
  EXPECT_EQ(1, failure_count);
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, NewWebContents) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* dialog = new MockHatsNextWebDialog(
      browser(), "open_new_web_contents_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), base::DoNothing(), base::DoNothing(), {}, {});

  // The mock hats dialog will push a close state after it has attempted to
  // open another web contents.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  dialog->WaitForClose();

  // Check that a tab with http://foo.com (defined in hats_next_mock.html) has
  // been opened in the regular browser and is active.
  EXPECT_EQ(
      GURL("http://foo.com"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

// The devtools browser for undocked devtools has no tab strip and can't open
// new tabs. Instead it should open new WebContents in the main browser.
IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest,
                       NewWebContentsForDevtoolsBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Browser* devtools_browser = OpenUndockedDevToolsWindow();

  auto* dialog = new MockHatsNextWebDialog(
      devtools_browser, "open_new_web_contents_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), base::DoNothing(), base::DoNothing(), {}, {});

  // The mock hats dialog will push a close state after it has attempted to
  // open another web contents.
  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  dialog->WaitForClose();

  // Check that a tab with http://foo.com (defined in hats_next_mock.html) has
  // been opened in the regular browser and is active.
  EXPECT_EQ(
      GURL("http://foo.com"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, DialogResize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* dialog = new MockHatsNextWebDialog(
      browser(), "resize_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), base::DoNothing(), base::DoNothing(), {}, {});

  // Check that the dialog reports a preferred size the same as the size defined
  // in hats_next_mock.html.
  constexpr auto kTargetSize = gfx::Size(70, 300);

  // Depending on renderer warm-up, an initial empty size may additionally be
  // reported before hats_next_mock.html has had a chance to resize.
  dialog->WaitForUpdateWidgetSize();
  auto size = dialog->CalculatePreferredSize();
  EXPECT_TRUE(size == kTargetSize || size == dialog->kMinSize);
  if (size != kTargetSize) {
    dialog->WaitForUpdateWidgetSize();
    EXPECT_EQ(kTargetSize, dialog->CalculatePreferredSize());
  }
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, MaximumSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_CALL(*hats_service(), HatsNextDialogClosed);
  auto* dialog = new MockHatsNextWebDialog(
      browser(), "resize_to_large_for_testing",
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), base::DoNothing(), base::DoNothing(), {}, {});

  // Check that the maximum size of the dialog is bounded appropriately by the
  // dialogs maximum size. Depending on renderer warm-up, an initial empty size
  // may additionally be reported before hats_next_mock.html has had a chance
  // to resize.
  dialog->WaitForUpdateWidgetSize();
  auto size = dialog->CalculatePreferredSize();
  EXPECT_TRUE(size == HatsNextWebDialog::kMaxSize || size == dialog->kMinSize);
  if (size != HatsNextWebDialog::kMaxSize) {
    dialog->WaitForUpdateWidgetSize();
    EXPECT_EQ(HatsNextWebDialog::kMaxSize, dialog->CalculatePreferredSize());
  }
}

IN_PROC_BROWSER_TEST_F(HatsNextWebDialogBrowserTest, ZoomLevel) {
  // Ensure that the dialog correctly resets the zoom level to default.
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(
      blink::PageZoomFactorToZoomLevel(5.0f));

  ScopedBrowserLocale browser_locale(kTestLocale);

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* dialog = new MockHatsNextWebDialog(
      browser(), kHatsNextSurveyTriggerIDTesting,
      embedded_test_server()->GetURL("/hats/hats_next_mock.html"),
      base::Seconds(100), GetSuccessClosure(), GetFailureClosure(),
      kHatsNextTestSurveyProductSpecificBitsData,
      kHatsNextTestSurveyProductSpecificStringData);

  // Allow the dialog to open before checking the zoom level of the contents.
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, ShowWidget).WillOnce(testing::Invoke([&run_loop]() {
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_TRUE(blink::PageZoomValuesEqual(
      content::HostZoomMap::GetDefaultForBrowserContext(dialog->otr_profile_)
          ->GetZoomLevel(dialog->web_view_->GetWebContents()),
      blink::PageZoomFactorToZoomLevel(1.0f)));
}
