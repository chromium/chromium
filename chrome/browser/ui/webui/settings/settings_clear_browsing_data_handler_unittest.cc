// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

using ::testing::Optional;

class TestingClearBrowsingDataHandler
    : public settings::ClearBrowsingDataHandler {
 public:
  using settings::ClearBrowsingDataHandler::AllowJavascript;
  using settings::ClearBrowsingDataHandler::set_web_ui;

  TestingClearBrowsingDataHandler(content::WebUI* webui, Profile* profile)
      : ClearBrowsingDataHandler(webui, profile) {}

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
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI test_web_ui_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingClearBrowsingDataHandler> handler_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil> dse_factory_util_;
  raw_ptr<TemplateURLService> template_url_service;
};

void ClearBrowsingDataHandlerUnitTest::SetUp() {
  TestingProfile::Builder builder;
  profile_ = builder.Build();

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_.get()));

  test_web_ui_.set_web_contents(web_contents_.get());
  test_web_ui_.ClearTrackedCalls();

  dse_factory_util_ =
      std::make_unique<TemplateURLServiceFactoryTestUtil>(profile_.get());
  dse_factory_util_->VerifyLoad();
  template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_.get());

  handler_ = std::make_unique<TestingClearBrowsingDataHandler>(&test_web_ui_,
                                                               profile_.get());
  handler_->set_web_ui(&test_web_ui_);
  handler_->AllowJavascript();

  browser_task_environment_.RunUntilIdle();
}

void ClearBrowsingDataHandlerUnitTest::TearDown() {
  dse_factory_util_.reset();
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
  NOTREACHED();
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

}  // namespace settings
