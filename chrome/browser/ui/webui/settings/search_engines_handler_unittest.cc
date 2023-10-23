// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

class SearchEnginesHandlerTest : public testing::Test {
 public:
  SearchEnginesHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    handler_ = std::make_unique<SearchEnginesHandler>(profile_);
    web_ui_.set_web_contents(web_contents_factory_.CreateWebContents(profile_));

    handler_->set_web_ui(&web_ui_);
    handler()->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  SearchEnginesHandler* handler() const { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  content::TestWebUI web_ui_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<SearchEnginesHandler> handler_;
};

TEST_F(SearchEnginesHandlerTest, ChangeInTemplateUrlDataTriggersCallback) {
  EXPECT_EQ(0U, web_ui()->call_data().size());
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());

  TemplateURLData default_search_engine;
  default_search_engine.SetShortName(u"foo.com");
  default_search_engine.SetURL("http://foo.com/url?bar={searchTerms}");
  default_search_engine.alternate_urls.push_back(
      "http://foo.com/alt#quux={searchTerms}");

  TemplateURL* template_url = template_url_service->Add(
      std::make_unique<TemplateURL>(default_search_engine));
  EXPECT_EQ(1U, web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("search-engines-changed", call_data.arg1()->GetString());

  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  EXPECT_EQ(2U, web_ui()->call_data().size());
  const content::TestWebUI::CallData& second_call_data =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", second_call_data.function_name());
  EXPECT_EQ("search-engines-changed", second_call_data.arg1()->GetString());
}

}  // namespace settings
