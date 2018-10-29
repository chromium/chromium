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
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace app_list {
namespace test {

namespace {

constexpr char kQueryBase[] = "http://beasts.org/search";
constexpr char kSomeParam[] = "&some_param=some_value";
constexpr char kCatQuery[] = "cat";
constexpr char kDogQuery[] = "dog";
constexpr char kSharkQuery[] = "shark";
constexpr char kCatCardId[] =
    "https://www.google.com/search?q=cat&sourceid=chrome&ie=UTF-8";
constexpr char kDogCardId[] =
    "https://www.google.com/search?q=dog&sourceid=chrome&ie=UTF-8";
constexpr char kSharkCardId[] =
    "https://www.google.com/search?q=shark&sourceid=chrome&ie=UTF-8";
constexpr char kCatCardTitle[] = "Cat is a furry beast.";
constexpr char kDogCardTitle[] = "Dog is a friendly beast.";
constexpr char kSharkCardTitle[] = "Shark is a scary beast.";

GURL GetSearchUrl(const std::string& query) {
  return GURL(
      base::StringPrintf("%s?q=%s%s", kQueryBase, query.c_str(), kSomeParam));
}

class MockAnswerCardContents : public AnswerCardContents {
 public:
  MockAnswerCardContents() {}

  // AnswerCardContents overrides:
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_CONST_METHOD0(GetToken, const base::UnguessableToken&());
  MOCK_CONST_METHOD0(GetPreferredSize, gfx::Size());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAnswerCardContents);
};

std::unique_ptr<KeyedService> CreateTemplateURLService(
    content::BrowserContext* context) {
  return std::make_unique<TemplateURLService>(nullptr, 0);
}

}  // namespace

class AnswerCardSearchProviderTest : public AppListTestBase {
 public:
  AnswerCardSearchProviderTest() : field_trial_list_(nullptr) {}

  void TestDidFinishNavigation(int contents_number,
                               bool has_error,
                               bool has_answer_card,
                               const std::string& title,
                               const std::string& issued_query,
                               std::size_t expected_result_count) {
    MockAnswerCardContents* const contents =
        contents_number == 0 ? contents0_ : contents1_;
    EXPECT_CALL(*contents, LoadURL(GetSearchUrl(kCatQuery)));
    provider()->Start(base::UTF8ToUTF16(kCatQuery));

    provider()->DidFinishNavigation(contents, GetSearchUrl(kCatQuery),
                                    has_error, has_answer_card, title,
                                    issued_query);

    provider()->OnContentsReady(contents);

    EXPECT_EQ(expected_result_count, results().size());

    testing::Mock::VerifyAndClearExpectations(contents);
  }

  void VerifyResult(const std::string& message,
                    const std::string& id,
                    const base::UnguessableToken& token,
                    const std::string& title) {
    SCOPED_TRACE(message);

    EXPECT_EQ(1UL, results().size());
    ChromeSearchResult* result = results()[0].get();
    EXPECT_EQ(ash::SearchResultDisplayType::kCard, result->display_type());
    EXPECT_EQ(id, result->id());
    EXPECT_EQ(1, result->relevance());
    EXPECT_EQ(token, result->answer_card_contents_token());
    EXPECT_EQ(base::UTF8ToUTF16(title), result->title());
  }

  FakeAppListModelUpdater* GetModelUpdater() const {
    return model_updater_.get();
  }

  const SearchProvider::Results& results() { return provider()->results(); }

  MockAnswerCardContents* contents0() const { return contents0_; }
  MockAnswerCardContents* contents1() const { return contents1_; }

  AnswerCardSearchProvider* provider() const { return provider_.get(); }

  const base::UnguessableToken& token0() const { return token0_; }
  const base::UnguessableToken& token1() const { return token1_; }

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    model_updater_->SetSearchEngineIsGoogle(true);

    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();

    // Set up card server URL.
    std::map<std::string, std::string> params;
    params["ServerUrl"] = kQueryBase;
    params["QuerySuffix"] = kSomeParam;
    base::AssociateFieldTrialParams("TestTrial", "TestGroup", params);
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial("TestTrial", "TestGroup");
    std::unique_ptr<base::FeatureList> feature_list =
        std::make_unique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        app_list_features::kEnableAnswerCard.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

    contents0_ = new MockAnswerCardContents;
    contents1_ = new MockAnswerCardContents;
    std::unique_ptr<AnswerCardContents> contents0(contents0_);
    std::unique_ptr<AnswerCardContents> contents1(contents1_);
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&CreateTemplateURLService));
    // Provider will own the MockAnswerCardContents instance.
    provider_ = std::make_unique<AnswerCardSearchProvider>(
        profile_.get(), model_updater_.get(), nullptr, std::move(contents0),
        std::move(contents1));

    token0_ = base::UnguessableToken::Create();
    token1_ = base::UnguessableToken::Create();

    ON_CALL(*contents0_, GetToken()).WillByDefault(ReturnRef(token0()));
    ON_CALL(*contents1_, GetToken()).WillByDefault(ReturnRef(token1()));
  }

 private:
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<AnswerCardSearchProvider> provider_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  MockAnswerCardContents* contents0_ = nullptr;  // Unowned.
  MockAnswerCardContents* contents1_ = nullptr;  // Unowned.
  base::FieldTrialList field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::UnguessableToken token0_;
  base::UnguessableToken token1_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardSearchProviderTest);
};

// Basic event sequence.
TEST_F(AnswerCardSearchProviderTest, Basic) {
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl(kCatQuery)));
  provider()->Start(base::UTF8ToUTF16(kCatQuery));
  provider()->DidFinishNavigation(contents1(), GetSearchUrl(kCatQuery), false,
                                  true, kCatCardTitle, kCatQuery);
  provider()->OnContentsReady(contents1());

  VerifyResult("Basic Result", kCatCardId, token1(), kCatCardTitle);

  // Now an empty query.
  EXPECT_CALL(*contents0(), LoadURL(_)).Times(0);
  provider()->Start(base::UTF8ToUTF16(""));
  EXPECT_EQ(0UL, results().size());
}

// Queries to non-Google search engines are ignored.
TEST_F(AnswerCardSearchProviderTest, NotGoogle) {
  GetModelUpdater()->SetSearchEngineIsGoogle(false);
  EXPECT_CALL(*contents1(), LoadURL(_)).Times(0);
  provider()->Start(base::UTF8ToUTF16(kCatQuery));
}

// Three queries in a row.
TEST_F(AnswerCardSearchProviderTest, ThreeQueries) {
  // 1. Fetch for cat.
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl(kCatQuery)));
  provider()->Start(base::UTF8ToUTF16(kCatQuery));
  provider()->DidFinishNavigation(contents1(), GetSearchUrl(kCatQuery), false,
                                  true, kCatCardTitle, kCatQuery);
  provider()->OnContentsReady(contents1());

  VerifyResult("Cat Result 1", kCatCardId, token1(), kCatCardTitle);

  // 2. Fetch for dog.
  // Starting another (dog) search doesn't dismiss the cat card.
  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl(kDogQuery)));
  provider()->Start(base::UTF8ToUTF16(kDogQuery));

  VerifyResult("Cat Result 2", kCatCardId, token1(), kCatCardTitle);

  provider()->DidFinishNavigation(contents0(), GetSearchUrl(kDogQuery), false,
                                  true, kDogCardTitle, kDogQuery);

  // The cat still stays.
  VerifyResult("Cat Result 3", kCatCardId, token1(), kCatCardTitle);

  provider()->OnContentsReady(contents0());

  // Once the dog finishes loading, it replaces the cat.
  VerifyResult("Dog Result 1", kDogCardId, token0(), kDogCardTitle);

  // 3. Fetch for shark.
  // The third query will use contents1/token1 again.
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl(kSharkQuery)));
  provider()->Start(base::UTF8ToUTF16(kSharkQuery));

  VerifyResult("Dog Result 2", kDogCardId, token0(), kDogCardTitle);

  provider()->DidFinishNavigation(contents1(), GetSearchUrl(kSharkQuery), false,
                                  true, kSharkCardTitle, kSharkQuery);

  VerifyResult("Dog Result 3", kDogCardId, token0(), kDogCardTitle);

  provider()->OnContentsReady(contents1());

  VerifyResult("Shark Result", kSharkCardId, token1(), kSharkCardTitle);
}

// Three queries in a row, second one fails due to an error.
TEST_F(AnswerCardSearchProviderTest, ThreeQueriesSecondErrors) {
  // 1. Fetch for cat.
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl(kCatQuery)));
  provider()->Start(base::UTF8ToUTF16(kCatQuery));
  provider()->DidFinishNavigation(contents1(), GetSearchUrl(kCatQuery), false,
                                  true, kCatCardTitle, kCatQuery);
  provider()->OnContentsReady(contents1());

  VerifyResult("Cat Result 1", kCatCardId, token1(), kCatCardTitle);

  // 2. Fetch for dog. This will fail with an error.
  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl(kDogQuery)));
  provider()->Start(base::UTF8ToUTF16(kDogQuery));

  VerifyResult("Cat Result 2", kCatCardId, token1(), kCatCardTitle);

  provider()->DidFinishNavigation(contents0(), GetSearchUrl(kDogQuery), true,
                                  false, "", "");

  EXPECT_EQ(0UL, results().size());

  provider()->OnContentsReady(contents0());

  EXPECT_EQ(0UL, results().size());

  // 3. Fetch for shark.
  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl(kSharkQuery)));
  provider()->Start(base::UTF8ToUTF16(kSharkQuery));

  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(contents0(), GetSearchUrl(kSharkQuery), false,
                                  true, kSharkCardTitle, kSharkQuery);

  EXPECT_EQ(0UL, results().size());

  provider()->OnContentsReady(contents0());

  VerifyResult("Shark Result", kSharkCardId, token0(), kSharkCardTitle);
}

// Three queries in a row, second one fails because the server responds with no
// card.
TEST_F(AnswerCardSearchProviderTest, ThreeQueriesSecondNoCard) {
  // 1. Fetch for cat.
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl(kCatQuery)));
  provider()->Start(base::UTF8ToUTF16(kCatQuery));
  provider()->DidFinishNavigation(contents1(), GetSearchUrl(kCatQuery), false,
                                  true, kCatCardTitle, kCatQuery);
  provider()->OnContentsReady(contents1());

  VerifyResult("Cat Result 1", kCatCardId, token1(), kCatCardTitle);

  // 2. Fetch for dog. This will fail with an error.
  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl(kDogQuery)));
  provider()->Start(base::UTF8ToUTF16(kDogQuery));

  VerifyResult("Cat Result 2", kCatCardId, token1(), kCatCardTitle);

  provider()->DidFinishNavigation(contents0(), GetSearchUrl(kDogQuery), false,
                                  false, "", "");

  EXPECT_EQ(0UL, results().size());

  provider()->OnContentsReady(contents0());

  EXPECT_EQ(0UL, results().size());

  // 3. Fetch for shark.
  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl(kSharkQuery)));
  provider()->Start(base::UTF8ToUTF16(kSharkQuery));

  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(contents0(), GetSearchUrl(kSharkQuery), false,
                                  true, kSharkCardTitle, kSharkQuery);

  EXPECT_EQ(0UL, results().size());

  provider()->OnContentsReady(contents0());

  VerifyResult("Shark Result", kSharkCardId, token0(), kSharkCardTitle);
}

// User enters a query character by character, so that each next query generates
// a web request while the previous one is still in progress. Only the last
// query should produce a result.
TEST_F(AnswerCardSearchProviderTest, InterruptedRequest) {
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl("c")));
  provider()->Start(base::UTF8ToUTF16("c"));
  EXPECT_EQ(0UL, results().size());

  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl("ca")));
  provider()->Start(base::UTF8ToUTF16("ca"));
  EXPECT_EQ(0UL, results().size());

  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl(kCatQuery)));
  provider()->Start(base::UTF8ToUTF16(kCatQuery));
  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(contents1(), GetSearchUrl("c"), false, true,
                                  "Title c", "c");
  provider()->OnContentsReady(contents1());
  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(contents1(), GetSearchUrl("ca"), false, true,
                                  "Title ca", "ca");
  provider()->OnContentsReady(contents1());
  EXPECT_EQ(0UL, results().size());

  provider()->DidFinishNavigation(contents1(), GetSearchUrl(kCatQuery), false,
                                  true, kCatCardTitle, kCatQuery);
  provider()->OnContentsReady(contents1());

  VerifyResult("Cat Result", kCatCardId, token1(), kCatCardTitle);
}

// After seeing a result, the user enters a query character by character. The
// result will stay until we get an uninterrupted answer.
TEST_F(AnswerCardSearchProviderTest, InterruptedRequestAfterResult) {
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl(kCatQuery)));
  provider()->Start(base::UTF8ToUTF16(kCatQuery));
  provider()->DidFinishNavigation(contents1(), GetSearchUrl(kCatQuery), false,
                                  true, kCatCardTitle, kCatQuery);
  provider()->OnContentsReady(contents1());

  VerifyResult("Cat Result 1", kCatCardId, token1(), kCatCardTitle);

  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl("d")));
  provider()->Start(base::UTF8ToUTF16("d"));

  VerifyResult("Cat Result 2", kCatCardId, token1(), kCatCardTitle);

  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl("do")));
  provider()->Start(base::UTF8ToUTF16("do"));

  VerifyResult("Cat Result 3", kCatCardId, token1(), kCatCardTitle);

  EXPECT_CALL(*contents0(), LoadURL(GetSearchUrl(kDogQuery)));
  provider()->Start(base::UTF8ToUTF16(kDogQuery));

  VerifyResult("Cat Result 4", kCatCardId, token1(), kCatCardTitle);

  provider()->DidFinishNavigation(contents0(), GetSearchUrl("d"), false, true,
                                  "Title d", "d");
  provider()->OnContentsReady(contents0());

  VerifyResult("Cat Result 5", kCatCardId, token1(), kCatCardTitle);

  provider()->DidFinishNavigation(contents0(), GetSearchUrl("do"), false, true,
                                  "Title do", "do");
  provider()->OnContentsReady(contents0());

  VerifyResult("Cat Result 5", kCatCardId, token1(), kCatCardTitle);

  provider()->DidFinishNavigation(contents0(), GetSearchUrl(kDogQuery), false,
                                  true, kDogCardTitle, kDogQuery);
  provider()->OnContentsReady(contents0());

  VerifyResult("Dog Result", kDogCardId, token0(), kDogCardTitle);
}

// Various values for DidFinishNavigation params.
TEST_F(AnswerCardSearchProviderTest, DidFinishNavigation) {
  TestDidFinishNavigation(1, false, true, kCatCardTitle, kCatQuery, 1UL);
  TestDidFinishNavigation(0, true, true, kCatCardTitle, kCatQuery, 0UL);
  TestDidFinishNavigation(0, false, false, kCatCardTitle, "", 0UL);
}

// Escaping a query with a special character.
TEST_F(AnswerCardSearchProviderTest, QueryEscaping) {
  EXPECT_CALL(*contents1(), LoadURL(GetSearchUrl("cat%26dog")));
  provider()->Start(base::UTF8ToUTF16("cat&dog"));
}

}  // namespace test
}  // namespace app_list
