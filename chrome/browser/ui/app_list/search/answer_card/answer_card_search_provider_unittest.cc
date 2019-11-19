// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_result.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

namespace {

constexpr char kQueryBase[] = "http://beasts.org/search";
constexpr char kSomeParam[] = "&some_param=some_value";
constexpr char kDogQuery[] = "dog";
constexpr char kSharkQuery[] = "shark";
constexpr char kDogSearchUrl[] =
    "https://www.google.com/search?q=dog&sourceid=chrome&ie=UTF-8";
constexpr char kSharkSearchUrl[] =
    "https://www.google.com/search?q=shark&sourceid=chrome&ie=UTF-8";

GURL GetAnswerCardUrl(const std::string& query) {
  return GURL(
      base::StringPrintf("%s?q=%s%s", kQueryBase, query.c_str(), kSomeParam));
}

std::unique_ptr<KeyedService> CreateTemplateURLService(
    content::BrowserContext* context) {
  return std::make_unique<TemplateURLService>(nullptr, 0);
}

}  // namespace

class AnswerCardSearchProviderTest : public AppListTestBase {
 public:
  AnswerCardSearchProviderTest() = default;

  FakeAppListModelUpdater* GetModelUpdater() const {
    return model_updater_.get();
  }

  const SearchProvider::Results& results() { return provider()->results(); }

  AnswerCardSearchProvider* provider() const { return provider_.get(); }

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    model_updater_->SetSearchEngineIsGoogle(true);

    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();

    // Set up card server URL.
    base::FieldTrialParams params;
    params["ServerUrl"] = kQueryBase;
    params["QuerySuffix"] = kSomeParam;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        app_list_features::kEnableAnswerCard, params);

    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&CreateTemplateURLService));
    provider_ = std::make_unique<AnswerCardSearchProvider>(
        profile_.get(), model_updater_.get(), nullptr);
  }

  GURL GetStrippedSearchUrl(const GURL& search_result_url) {
    return AutocompleteMatch::GURLToStrippedGURL(
        GURL(search_result_url), AutocompleteInput(),
        TemplateURLServiceFactory::GetForProfile(profile_.get()),
        base::string16() /* keyword */);
  }

 private:
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<AnswerCardSearchProvider> provider_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardSearchProviderTest);
};

// Basic usage. |Start()| immediately populates an appropriate search result to
// be used by the client. A subsequent |Start()| call replaces the previous
// result.
TEST_F(AnswerCardSearchProviderTest, Start) {
  provider()->Start(base::UTF8ToUTF16(kDogQuery));
  ASSERT_EQ(1u, results().size());
  AnswerCardResult* result = static_cast<AnswerCardResult*>(results()[0].get());
  EXPECT_EQ(GURL(kDogSearchUrl), result->search_result_url());
  EXPECT_EQ(GetStrippedSearchUrl(GURL(kDogSearchUrl)),
            result->equivalent_result_id().value());
  EXPECT_EQ(GetAnswerCardUrl(kDogQuery), result->query_url()->spec());

  provider()->Start(base::UTF8ToUTF16(kSharkQuery));
  ASSERT_EQ(1u, results().size());
  result = static_cast<AnswerCardResult*>(results()[0].get());
  EXPECT_EQ(GURL(kSharkSearchUrl), result->search_result_url());
  EXPECT_EQ(GetStrippedSearchUrl(GURL(kSharkSearchUrl)),
            result->equivalent_result_id().value());
  EXPECT_EQ(GetAnswerCardUrl(kSharkQuery), result->query_url()->spec());
}

// Queries to non-Google search engines are ignored.
TEST_F(AnswerCardSearchProviderTest, NotGoogle) {
  GetModelUpdater()->SetSearchEngineIsGoogle(false);
  provider()->Start(base::UTF8ToUTF16(kDogQuery));
  EXPECT_EQ(0u, results().size());
}

// Escaping a query with a special character produces a well-formed query URL.
TEST_F(AnswerCardSearchProviderTest, QueryEscaping) {
  provider()->Start(base::UTF8ToUTF16("cat&dog"));
  ASSERT_EQ(1u, results().size());
  EXPECT_EQ(GetAnswerCardUrl("cat%26dog"), results()[0]->query_url()->spec());
}

}  // namespace test
}  // namespace app_list
