// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

using ::testing::_;
using ::testing::Optional;

namespace {

class MockBrowsingDataCounter : public browsing_data::BrowsingDataCounter {
 public:
  MockBrowsingDataCounter() {
    ON_CALL(*this, SetBeginTime).WillByDefault([this](base::Time begin_time) {
      browsing_data::BrowsingDataCounter::SetBeginTime(begin_time);
    });
  }
  ~MockBrowsingDataCounter() override = default;

  MOCK_METHOD(void, Count, ());
  MOCK_METHOD(void, SetBeginTime, (base::Time));

  const char* GetPrefName() const override {
    return browsing_data::prefs::kDeleteBrowsingHistory;
  }
};

}  // namespace

class TestingClearBrowsingDataHandler
    : public settings::ClearBrowsingDataHandler {
 public:
  using settings::ClearBrowsingDataHandler::AllowJavascript;
  using settings::ClearBrowsingDataHandler::set_web_ui;
  using settings::ClearBrowsingDataHandler::UpdateSyncState;

  TestingClearBrowsingDataHandler(content::WebUI* webui, Profile* profile)
      : ClearBrowsingDataHandler(webui, profile) {
    AddCounter(std::make_unique<MockBrowsingDataCounter>());
  }

  void HandleRestartCounters(const base::ListValue& args) {
    settings::ClearBrowsingDataHandler::HandleRestartCounters(args);
  }

  MockBrowsingDataCounter* counter() const {
    return static_cast<MockBrowsingDataCounter*>(counters_[0].get());
  }

  // Some services initialized in |OnJavascriptAllowed()| don't have test
  // versions, hence are not available in unittests. For this reason we only
  // initialize services needed by the tests below.
  void OnJavascriptAllowed() override {
    dse_service_observation_.Observe(
        TemplateURLServiceFactory::GetForProfile(profile_));
  }
};

class ClearBrowsingDataHandlerBrowserTest : public InProcessBrowserTest {
 public:
  ClearBrowsingDataHandlerBrowserTest() {
    feature_list_.InitWithFeatures({toast_features::kClearBrowsingDataToast},
                                   {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    test_web_ui_.set_web_contents(
        browser()->GetTabStripModel()->GetActiveWebContents());
    test_web_ui_.ClearTrackedCalls();

    template_url_service_ =
        TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());

    handler_ = std::make_unique<TestingClearBrowsingDataHandler>(
        &test_web_ui_, browser()->GetProfile());
    handler_->set_web_ui(&test_web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascript();
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    template_url_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void VerifySearchHistoryWebUIUpdate(
      const bool expected_is_non_google_dse,
      const std::u16string& expected_non_google_search_history_string) {
    // Verify the latest update if multiple, so iterate from the end.
    const std::vector<std::unique_ptr<content::TestWebUI::CallData>>&
        call_data = test_web_ui_.call_data();
    for (int i = call_data.size() - 1; i >= 0; --i) {
      const content::TestWebUI::CallData& data = *(call_data[i]);
      if (data.function_name() != "cr.webUIListenerCallback") {
        continue;
      }
      const std::string* event = data.arg1()->GetIfString();
      if (!event || *event != "update-sync-state") {
        continue;
      }
      const base::DictValue* arg2_dict = data.arg2()->GetIfDict();
      if (!arg2_dict) {
        continue;
      }
      ASSERT_THAT(arg2_dict->FindBool("isNonGoogleDse"),
                  Optional(expected_is_non_google_dse));
      if (expected_is_non_google_dse) {
        std::u16string actual_non_google_search_history_string =
            base::UTF8ToUTF16(
                *arg2_dict->FindString("nonGoogleSearchHistoryString"));
        ASSERT_EQ(expected_non_google_search_history_string,
                  actual_non_google_search_history_string);
      }
      return;
    }
    NOTREACHED();
  }

  TemplateURL* AddSearchEngine(const std::u16string& short_name,
                               const GURL& searchable_url,
                               int prepopulate_id,
                               bool set_default) {
    TemplateURLData data;
    data.SetShortName(short_name);
    data.SetKeyword(short_name);
    data.SetURL(searchable_url.possibly_invalid_spec());
    data.favicon_url = TemplateURL::GenerateFaviconURL(searchable_url);
    data.prepopulate_id = prepopulate_id;
    TemplateURL* url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    if (set_default) {
      template_url_service_->SetUserSelectedDefaultSearchProvider(url);
    }
    return url;
  }

 protected:
  content::TestWebUI test_web_ui_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingClearBrowsingDataHandler> handler_;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;

  const content::TestWebUI::CallData& GetCallData() {
    return *test_web_ui_.call_data().back();
  }
};

IN_PROC_BROWSER_TEST_F(ClearBrowsingDataHandlerBrowserTest,
                       ClearBrowsingData_EmitsDeleteMetrics) {
  base::HistogramTester histogram_tester;
  base::ListValue args;

  args.Append("fooCallback");
  args.Append(base::ListValue());
  args.Append(1);

  test_web_ui_.HandleReceivedMessage("clearBrowsingData", args);

  const content::TestWebUI::CallData& call_data = GetCallData();
  ASSERT_EQ(3u, call_data.args().size());

  histogram_tester.ExpectBucketCount(
      "Privacy.DeleteBrowsingData.Action",
      browsing_data::DeleteBrowsingDataAction::kClearBrowsingDataDialog, 1);
}

IN_PROC_BROWSER_TEST_F(ClearBrowsingDataHandlerBrowserTest,
                       ClearBrowsingData_ShowsToast) {
  EXPECT_FALSE(ToastController::From(browser())->IsShowingToast());

  base::ListValue args;
  args.Append("fooCallback");
  args.Append(base::ListValue());
  args.Append(1);
  test_web_ui_.HandleReceivedMessage("clearBrowsingData", args);

  EXPECT_TRUE(ToastController::From(browser())->IsShowingToast());
}

IN_PROC_BROWSER_TEST_F(ClearBrowsingDataHandlerBrowserTest,
                       UpdateSyncState_GoogleDse) {
  handler_->UpdateSyncState();
  VerifySearchHistoryWebUIUpdate(false, u"");
}

IN_PROC_BROWSER_TEST_F(ClearBrowsingDataHandlerBrowserTest,
                       UpdateSyncState_NonGoogleDsePrepopulated) {
  // Prepopulated search engines have an ID > 0.
  AddSearchEngine(u"SomeSE", GURL("https://somese.com?q={searchTerms}"), 1001,
                  true);

  // DSE changes should update the handler, no need to call
  // |UpdateSyncState()|.
  VerifySearchHistoryWebUIUpdate(
      true, l10n_util::GetStringFUTF16(
                IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_PREPOPULATED_DSE,
                u"SomeSE"));
}

IN_PROC_BROWSER_TEST_F(ClearBrowsingDataHandlerBrowserTest,
                       UpdateSyncState_NonGoogleDseNotPrepopulated) {
  // Custom search engines have a prepopulated ID of 0.
  AddSearchEngine(u"SomeSE", GURL("https://somese.com?q={searchTerms}"), 0,
                  true);

  // DSE changes should update the handler, no need to call
  // |UpdateSyncState()|.
  VerifySearchHistoryWebUIUpdate(
      true,
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_NON_PREPOPULATED_DSE));
}

IN_PROC_BROWSER_TEST_F(ClearBrowsingDataHandlerBrowserTest,
                       HandleRestartCounters) {
  base::ListValue args;
  args.Append(static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));

  EXPECT_CALL(*(handler_->counter()), Count());
  EXPECT_CALL(*(handler_->counter()), SetBeginTime(_));

  handler_->HandleRestartCounters(args);

  // Test a different combination of parameters.
  testing::Mock::VerifyAndClearExpectations(handler_->counter());

  args.clear();
  args.Append(static_cast<int>(browsing_data::TimePeriod::ALL_TIME));

  EXPECT_CALL(*(handler_->counter()), Count());
  EXPECT_CALL(*(handler_->counter()),
              SetBeginTime(browsing_data::CalculateBeginDeleteTime(
                  browsing_data::TimePeriod::ALL_TIME)));

  handler_->HandleRestartCounters(args);
}

}  // namespace settings
