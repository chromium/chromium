// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

using ::testing::_;
using ::testing::Optional;

static const char* kTestingDatatypePref = "counter.testing.datatype";

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

  const char* GetPrefName() const override { return kTestingDatatypePref; }
};

}  // namespace

class TestingClearBrowsingDataHandler
    : public settings::ClearBrowsingDataHandler {
 public:
  using settings::ClearBrowsingDataHandler::AllowJavascript;
  using settings::ClearBrowsingDataHandler::set_web_ui;

  TestingClearBrowsingDataHandler(content::WebUI* webui, Profile* profile)
      : ClearBrowsingDataHandler(webui, profile) {
    AddCounter(std::make_unique<MockBrowsingDataCounter>(),
               browsing_data::ClearBrowsingDataTab::BASIC);
    AddCounter(std::make_unique<MockBrowsingDataCounter>(),
               browsing_data::ClearBrowsingDataTab::ADVANCED);
  }

  void HandleRestartCounters(const base::Value::List& args) {
    settings::ClearBrowsingDataHandler::HandleRestartCounters(args);
  }

  MockBrowsingDataCounter* basic_counter() const {
    return static_cast<MockBrowsingDataCounter*>(counters_basic_[0].get());
  }

  MockBrowsingDataCounter* advanced_counter() const {
    return static_cast<MockBrowsingDataCounter*>(counters_advanced_[0].get());
  }

  // Some services initialized in |OnJavascriptAllowed()| don't have test
  // versions, hence are not available in unittests. For this reason we only
  // initialize services needed by the unittests below.
  void OnJavascriptAllowed() override {
    dse_service_observation_.Observe(
        TemplateURLServiceFactory::GetForProfile(profile_));
  }
};

class ClearBrowsingDataHandlerUnitTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;
  void VerifySearchHistoryWebUIUpdate(
      const bool expected_is_non_google_dse,
      const std::u16string& expected_non_google_search_history_strin);
  TemplateURL* AddSearchEngine(const std::u16string& short_name,
                               const GURL& searchable_url,
                               int prepopulate_id,
                               bool set_default);

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI test_web_ui_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingClearBrowsingDataHandler> handler_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil> dse_factory_util_;
  raw_ptr<TemplateURLService> template_url_service;

  const content::TestWebUI::CallData& GetCallData() {
    return *test_web_ui_.call_data().back();
  }

  Browser* browser() { return browser_.get(); }
};

void ClearBrowsingDataHandlerUnitTest::SetUp() {
  feature_list_.InitWithFeatures({toast_features::kToastFramework,
                                  toast_features::kClearBrowsingDataToast},
                                 {});

  TestingProfile::Builder builder;
  profile_ = builder.Build();

  profile_->GetTestingPrefService()->registry()->RegisterBooleanPref(
      kTestingDatatypePref, true);

  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile_.get(), /*user_gesture*/ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(Browser::Create(params));

  std::unique_ptr<tabs::TabModel> tab_model = std::make_unique<tabs::TabModel>(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get())),
      browser()->GetTabStripModel());
  browser()->GetTabStripModel()->AppendTab(std::move(tab_model), true);

  test_web_ui_.set_web_contents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  test_web_ui_.ClearTrackedCalls();

  dse_factory_util_ =
      std::make_unique<TemplateURLServiceFactoryTestUtil>(profile_.get());
  dse_factory_util_->VerifyLoad();
  template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_.get());

  handler_ = std::make_unique<TestingClearBrowsingDataHandler>(&test_web_ui_,
                                                               profile_.get());
  handler_->set_web_ui(&test_web_ui_);
  handler_->RegisterMessages();
  handler_->AllowJavascript();

  browser_task_environment_.RunUntilIdle();
}

void ClearBrowsingDataHandlerUnitTest::TearDown() {
  dse_factory_util_.reset();
  browser_->tab_strip_model()->CloseAllTabs();
  browser_ = nullptr;
  browser_window_ = nullptr;
}

void ClearBrowsingDataHandlerUnitTest::VerifySearchHistoryWebUIUpdate(
    const bool expected_is_non_google_dse,
    const std::u16string& expected_non_google_search_history_string) {
  // Verify the latest update if multiple, so iterate from the end.
  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      test_web_ui_.call_data();
  for (int i = call_data.size() - 1; i >= 0; --i) {
    const content::TestWebUI::CallData& data = *(call_data[i]);
    if (data.function_name() != "cr.webUIListenerCallback") {
      continue;
    }
    const std::string* event = data.arg1()->GetIfString();
    if (!event || *event != "update-sync-state")
      continue;
    const base::Value::Dict* arg2_dict = data.arg2()->GetIfDict();
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
  NOTREACHED_IN_MIGRATION();
}

TemplateURL* ClearBrowsingDataHandlerUnitTest::AddSearchEngine(
    const std::u16string& short_name,
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
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  if (set_default)
    template_url_service->SetUserSelectedDefaultSearchProvider(url);
  return url;
}

TEST_F(ClearBrowsingDataHandlerUnitTest,
       ClearBrowsingData_EmmitsDeleteMetrics) {
  base::HistogramTester histogram_tester;
  base::Value::List args;

  args.Append("fooCallback");
  args.Append(base::Value::List());
  args.Append(1);

  test_web_ui_.HandleReceivedMessage("clearBrowsingData", args);

  const content::TestWebUI::CallData& call_data = GetCallData();
  ASSERT_EQ(3u, call_data.args().size());

  histogram_tester.ExpectBucketCount(
      "Privacy.DeleteBrowsingData.Action",
      browsing_data::DeleteBrowsingDataAction::kClearBrowsingDataDialog, 1);
}

TEST_F(ClearBrowsingDataHandlerUnitTest, ClearBrowsingData_ShowsToast) {
  EXPECT_FALSE(browser()->GetFeatures().toast_controller()->IsShowingToast());

  base::Value::List args;
  args.Append("fooCallback");
  args.Append(base::Value::List());
  args.Append(1);
  test_web_ui_.HandleReceivedMessage("clearBrowsingData", args);

  EXPECT_TRUE(browser()->GetFeatures().toast_controller()->IsShowingToast());
}

TEST_F(ClearBrowsingDataHandlerUnitTest, UpdateSyncState_GoogleDse) {
  handler_->UpdateSyncState();
  VerifySearchHistoryWebUIUpdate(false, u"");
}

TEST_F(ClearBrowsingDataHandlerUnitTest,
       UpdateSyncState_NonGoogleDsePrepopulated) {
  // Prepopulated search engines have an ID > 0.
  AddSearchEngine(u"SomeSE", GURL("https://somese.com?q={searchTerms}"), 1001,
                  true);

  // DSE changes should update the handler, no need to call |UpdateSyncState()|.
  VerifySearchHistoryWebUIUpdate(
      true, l10n_util::GetStringFUTF16(
                IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_PREPOPULATED_DSE,
                u"SomeSE"));
}

TEST_F(ClearBrowsingDataHandlerUnitTest,
       UpdateSyncState_NonGoogleDseNotPrepopulated) {
  // Custom search engines have a prepopulated ID of 0.
  AddSearchEngine(u"SomeSE", GURL("https://somese.com?q={searchTerms}"), 0,
                  true);

  // DSE changes should update the handler, no need to call |UpdateSyncState()|.
  VerifySearchHistoryWebUIUpdate(
      true,
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_NON_PREPOPULATED_DSE));
}

TEST_F(ClearBrowsingDataHandlerUnitTest, HandleRestartCounters) {
  base::Value::List basic_args;
  basic_args.Append(true /* basic */);
  basic_args.Append(static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));

  EXPECT_CALL(*(handler_->basic_counter()), Count());
  EXPECT_CALL(*(handler_->basic_counter()), SetBeginTime(_));

  EXPECT_CALL(*(handler_->advanced_counter()), Count()).Times(0);
  EXPECT_CALL(*(handler_->advanced_counter()), SetBeginTime(_)).Times(0);

  handler_->HandleRestartCounters(basic_args);

  // Test a different combination of parameters.
  testing::Mock::VerifyAndClearExpectations(handler_->basic_counter());
  testing::Mock::VerifyAndClearExpectations(handler_->advanced_counter());

  base::Value::List advanced_args;
  advanced_args.Append(false /* basic */);
  advanced_args.Append(static_cast<int>(browsing_data::TimePeriod::ALL_TIME));

  EXPECT_CALL(*(handler_->basic_counter()), Count()).Times(0);
  EXPECT_CALL(*(handler_->basic_counter()), SetBeginTime(_)).Times(0);

  EXPECT_CALL(*(handler_->advanced_counter()), Count());
  EXPECT_CALL(*(handler_->advanced_counter()),
              SetBeginTime(browsing_data::CalculateBeginDeleteTime(
                  browsing_data::TimePeriod::ALL_TIME)));

  handler_->HandleRestartCounters(advanced_args);
}

}  // namespace settings
