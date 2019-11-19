// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/json_request.h"

#include <set>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/961023): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_BuildRequestAuthenticated DISABLED_BuildRequestAuthenticated
#else
#define MAYBE_BuildRequestAuthenticated BuildRequestAuthenticated
#endif

namespace ntp_snippets {

namespace internal {

namespace {

using testing::_;
using testing::Eq;
using testing::Not;
using testing::NotNull;
using testing::StrEq;

MATCHER_P(EqualsJSON, json, "equals JSON") {
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(json);
  if (!expected) {
    *result_listener << "INTERNAL ERROR: couldn't parse expected JSON";
    return false;
  }

  std::string err_msg;
  int err_line, err_col;
  std::unique_ptr<base::Value> actual =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          arg, base::JSON_PARSE_RFC, nullptr, &err_msg, &err_line, &err_col);
  if (!actual) {
    *result_listener << "input:" << err_line << ":" << err_col << ": "
                     << "parse error: " << err_msg;
    return false;
  }
  return *expected == *actual;
}

}  // namespace

class JsonRequestTest : public testing::Test {
 public:
  JsonRequestTest()
      : pref_service_(std::make_unique<TestingPrefServiceSimple>()),
        mock_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_runner_handle_(
            std::make_unique<base::ThreadTaskRunnerHandle>(mock_task_runner_)),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kArticleSuggestionsFeature,
        {{"send_top_languages", "true"}, {"send_user_class", "true"}});
    language::UrlLanguageHistogram::RegisterProfilePrefs(
        pref_service_->registry());
  }

  std::unique_ptr<language::UrlLanguageHistogram> MakeLanguageHistogram(
      const std::set<std::string>& codes) {
    std::unique_ptr<language::UrlLanguageHistogram> language_histogram =
        std::make_unique<language::UrlLanguageHistogram>(pref_service_.get());
    // There must be at least 10 visits before the top languages are defined.
    for (int i = 0; i < 10; i++) {
      for (const std::string& code : codes) {
        language_histogram->OnPageVisited(code);
      }
    }
    return language_histogram;
  }

  JsonRequest::Builder CreateMinimalBuilder() {
    JsonRequest::Builder builder;
    builder.SetUrl(GURL("http://valid-url.test"))
        .SetClock(mock_task_runner_->GetMockClock())
        .SetUrlLoaderFactory(test_shared_loader_factory_);
    return builder;
  }

  std::unique_ptr<base::test::ScopedFeatureList> ForceOptionalImagesSupport(
      bool supported) {
    auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
    if (supported) {
      feature_list->InitWithFeatures({kOptionalImagesEnabledFeature}, {});
    } else {
      feature_list->InitWithFeatures({}, {kOptionalImagesEnabledFeature});
    }
    return feature_list;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  std::unique_ptr<base::ThreadTaskRunnerHandle> mock_runner_handle_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(JsonRequestTest);
};

TEST_F(JsonRequestTest, MAYBE_BuildRequestAuthenticated) {
  JsonRequest::Builder builder = CreateMinimalBuilder();
  RequestParams params;
  params.excluded_ids = {"1234567890"};
  params.count_to_fetch = 25;
  params.interactive_request = false;
  builder.SetParams(params)
      .SetUrl(GURL("http://valid-url.test"))
      .SetAuthentication("headerstuff")
      .SetUserClassForTesting("ACTIVE_NTP_USER")
      .Build();

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "Authorization: headerstuff\r\n"
                    "\r\n"));
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"excludedSuggestionIds\": ["
                         "    \"1234567890\""
                         "  ],"
                         "  \"userActivenessClass\": \"ACTIVE_NTP_USER\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestUnauthenticated) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = true;
  params.count_to_fetch = 10;
  builder.SetParams(params).SetUserClassForTesting("ACTIVE_NTP_USER");

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "\r\n"));
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"USER_ACTION\","
                         "  \"excludedSuggestionIds\": [],"
                         "  \"userActivenessClass\": \"ACTIVE_NTP_USER\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestDisplayCapabilityDisabledByFeature) {
  auto optional_images_feature_list = ForceOptionalImagesSupport(false);

  JsonRequest::Builder builder;
  builder.SetOptionalImagesCapability(true);

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "\r\n"));

  // The JSON should not contain any mention of displayCapability.
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"excludedSuggestionIds\": [],"
                         "  \"priority\": \"BACKGROUND_PREFETCH\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestDisplayCapabilityUnspecified) {
  auto optional_images_feature_list = ForceOptionalImagesSupport(true);

  JsonRequest::Builder builder;
  builder.SetOptionalImagesCapability(false);

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "\r\n"));
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"excludedSuggestionIds\": [],"
                         "  \"priority\": \"BACKGROUND_PREFETCH\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestOptionalImages) {
  auto optional_images_feature_list = ForceOptionalImagesSupport(true);

  JsonRequest::Builder builder;
  builder.SetOptionalImagesCapability(true);

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "\r\n"));
  EXPECT_THAT(
      builder.PreviewRequestBodyForTesting(),
      EqualsJSON("{"
                 "  \"displayCapability\": \"CAPABILITY_OPTIONAL_IMAGES\","
                 "  \"excludedSuggestionIds\": [],"
                 "  \"priority\": \"BACKGROUND_PREFETCH\""
                 "}"));
}

TEST_F(JsonRequestTest, ShouldNotTruncateExcludedIdsList) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = false;
  for (int i = 0; i < 200; ++i) {
    params.excluded_ids.insert(base::StringPrintf("%03d", i));
  }
  builder.SetParams(params).SetUserClassForTesting("ACTIVE_NTP_USER");

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"excludedSuggestionIds\": ["
                         "    \"000\", \"001\", \"002\", \"003\", \"004\","
                         "    \"005\", \"006\", \"007\", \"008\", \"009\","
                         "    \"010\", \"011\", \"012\", \"013\", \"014\","
                         "    \"015\", \"016\", \"017\", \"018\", \"019\","
                         "    \"020\", \"021\", \"022\", \"023\", \"024\","
                         "    \"025\", \"026\", \"027\", \"028\", \"029\","
                         "    \"030\", \"031\", \"032\", \"033\", \"034\","
                         "    \"035\", \"036\", \"037\", \"038\", \"039\","
                         "    \"040\", \"041\", \"042\", \"043\", \"044\","
                         "    \"045\", \"046\", \"047\", \"048\", \"049\","
                         "    \"050\", \"051\", \"052\", \"053\", \"054\","
                         "    \"055\", \"056\", \"057\", \"058\", \"059\","
                         "    \"060\", \"061\", \"062\", \"063\", \"064\","
                         "    \"065\", \"066\", \"067\", \"068\", \"069\","
                         "    \"070\", \"071\", \"072\", \"073\", \"074\","
                         "    \"075\", \"076\", \"077\", \"078\", \"079\","
                         "    \"080\", \"081\", \"082\", \"083\", \"084\","
                         "    \"085\", \"086\", \"087\", \"088\", \"089\","
                         "    \"090\", \"091\", \"092\", \"093\", \"094\","
                         "    \"095\", \"096\", \"097\", \"098\", \"099\","
                         "    \"100\", \"101\", \"102\", \"103\", \"104\","
                         "    \"105\", \"106\", \"107\", \"108\", \"109\","
                         "    \"110\", \"111\", \"112\", \"113\", \"114\","
                         "    \"115\", \"116\", \"117\", \"118\", \"119\","
                         "    \"120\", \"121\", \"122\", \"123\", \"124\","
                         "    \"125\", \"126\", \"127\", \"128\", \"129\","
                         "    \"130\", \"131\", \"132\", \"133\", \"134\","
                         "    \"135\", \"136\", \"137\", \"138\", \"139\","
                         "    \"140\", \"141\", \"142\", \"143\", \"144\","
                         "    \"145\", \"146\", \"147\", \"148\", \"149\","
                         "    \"150\", \"151\", \"152\", \"153\", \"154\","
                         "    \"155\", \"156\", \"157\", \"158\", \"159\","
                         "    \"160\", \"161\", \"162\", \"163\", \"164\","
                         "    \"165\", \"166\", \"167\", \"168\", \"169\","
                         "    \"170\", \"171\", \"172\", \"173\", \"174\","
                         "    \"175\", \"176\", \"177\", \"178\", \"179\","
                         "    \"180\", \"181\", \"182\", \"183\", \"184\","
                         "    \"185\", \"186\", \"187\", \"188\", \"189\","
                         "    \"190\", \"191\", \"192\", \"193\", \"194\","
                         "    \"195\", \"196\", \"197\", \"198\", \"199\""
                         "  ],"
                         "  \"userActivenessClass\": \"ACTIVE_NTP_USER\""
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestNoUserClass) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = false;
  builder.SetParams(params);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"excludedSuggestionIds\": []"
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestWithTwoLanguages) {
  JsonRequest::Builder builder;
  std::unique_ptr<language::UrlLanguageHistogram> language_histogram =
      MakeLanguageHistogram({"de", "en"});
  RequestParams params;
  params.interactive_request = true;
  params.language_code = "en";
  builder.SetParams(params).SetLanguageHistogram(language_histogram.get());

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"USER_ACTION\","
                         "  \"uiLanguage\": \"en\","
                         "  \"excludedSuggestionIds\": [],"
                         "  \"topLanguages\": ["
                         "    {"
                         "      \"language\" : \"en\","
                         "      \"frequency\" : 0.5"
                         "    },"
                         "    {"
                         "      \"language\" : \"de\","
                         "      \"frequency\" : 0.5"
                         "    }"
                         "  ]"
                         "}"));
}

TEST_F(JsonRequestTest, BuildRequestWithUILanguageOnly) {
  JsonRequest::Builder builder;
  std::unique_ptr<language::UrlLanguageHistogram> language_histogram =
      MakeLanguageHistogram({"en"});
  RequestParams params;
  params.interactive_request = true;
  params.language_code = "en";
  builder.SetParams(params).SetLanguageHistogram(language_histogram.get());

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "  \"priority\": \"USER_ACTION\","
                         "  \"uiLanguage\": \"en\","
                         "  \"excludedSuggestionIds\": [],"
                         "  \"topLanguages\": [{"
                         "    \"language\" : \"en\","
                         "    \"frequency\" : 1.0"
                         "  }]"
                         "}"));
}

TEST_F(JsonRequestTest,
       ShouldPropagateCountToFetchWhenExclusiveCategoryPresent) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = true;
  params.language_code = "en";
  params.exclusive_category =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  params.count_to_fetch = 25;
  builder.SetParams(params);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(), EqualsJSON(R"(
                              {
                                "priority": "USER_ACTION",
                                "uiLanguage": "en",
                                "excludedSuggestionIds": [],
                                "categoryParameters": [{
                                  "id": 1,
                                  "numSuggestions": 25
                                }]
                              }
                            )"));
}

// TODO(vitaliii): Propagate count to fetch in this case as well and delete this
// test. Currently the server does not support this.
TEST_F(JsonRequestTest,
       ShouldNotPropagateCountToFetchWhenExclusiveCategoryNotPresent) {
  JsonRequest::Builder builder;
  RequestParams params;
  params.interactive_request = true;
  params.language_code = "en";
  params.count_to_fetch = 10;
  builder.SetParams(params);

  EXPECT_THAT(builder.PreviewRequestBodyForTesting(), EqualsJSON(R"(
                              {
                                "priority": "USER_ACTION",
                                "uiLanguage": "en",
                                "excludedSuggestionIds": []
                              }
                            )"));
}

}  // namespace internal

}  // namespace ntp_snippets
