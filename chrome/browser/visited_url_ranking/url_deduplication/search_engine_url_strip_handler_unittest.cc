// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/url_deduplication/search_engine_url_strip_handler.h"

#include "base/functional/callback.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url_deduplication {

class SearchEngineURLStripHandlerTest : public ::testing::Test {
 public:
  SearchEngineURLStripHandlerTest() = default;

  void SetUp() override {
    factory_util_.VerifyLoad();
    handler_ = std::make_unique<SearchEngineURLStripHandler>(
        template_url_service(), true, true, u"");
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile& profile() { return profile_; }
  TemplateURLService* template_url_service() { return factory_util_.model(); }

  SearchEngineURLStripHandler* Handler() { return handler_.get(); }

 private:
  TestingProfile profile_;
  std::unique_ptr<SearchEngineURLStripHandler> handler_;
  TemplateURLServiceFactoryTestUtil factory_util_{&profile_};
};

TEST_F(SearchEngineURLStripHandlerTest, StripURL) {
  TemplateURLData data;
  data.SetShortName(u"Sherlock");
  data.SetKeyword(u"sherlock");
  data.SetURL("https://sherlock.example/?q={searchTerms}");
  TemplateURL* search_engine =
      template_url_service()->Add(std::make_unique<TemplateURL>(data));
  template_url_service()->SetUserSelectedDefaultSearchProvider(search_engine);

  GURL full_url = GURL("https://sherlock.example/?q=test&oq=test");
  GURL stripped_url = Handler()->StripExtraParams(full_url);
  ASSERT_EQ("https://sherlock.example/?q=test", stripped_url.spec());
}

TEST_F(SearchEngineURLStripHandlerTest, StripURLNonTemplateURL) {
  TemplateURLData data;
  data.SetShortName(u"Sherlock");
  data.SetKeyword(u"sherlock");
  data.SetURL("https://sherlock.example/?q={searchTerms}");
  TemplateURL* search_engine =
      template_url_service()->Add(std::make_unique<TemplateURL>(data));
  template_url_service()->SetUserSelectedDefaultSearchProvider(search_engine);

  GURL full_url = GURL("https://notsearch.example/?q=test&oq=test");
  GURL stripped_url = Handler()->StripExtraParams(full_url);
  ASSERT_EQ("https://notsearch.example/?q=test&oq=test", stripped_url.spec());
}

}  // namespace url_deduplication
