// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/suggest_entity_image_url_fetcher.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/page_image_service/metrics_util.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

namespace page_image_service {
namespace {

// Build real Suggest metadata so these tests exercise parsing as well as
// selecting an image from the parsed results.
std::string MakeResponse(
    std::initializer_list<std::pair<std::string, std::string>> suggestions) {
  base::ListValue matches;
  base::ListValue details;
  for (const auto& [text, image_url] : suggestions) {
    matches.Append(text);
    omnibox::SuggestTemplateInfo info;
    info.mutable_image()->set_url(image_url);
    details.Append(
        base::DictValue().Set("google:suggesttemplate",
                              base::Base64Encode(info.SerializeAsString())));
  }
  base::ListValue response;
  response.Append("santa monica");
  response.Append(std::move(matches));
  response.Append(base::ListValue());
  response.Append(base::ListValue());
  response.Append(
      base::DictValue().Set("google:suggestdetail", std::move(details)));
  return base::WriteJson(response).value();
}

class SuggestEntityImageURLFetcherTest : public testing::Test {
 protected:
  void SetUp() override {
    TemplateURLData data;
    data.SetShortName(u"Test search");
    data.SetKeyword(u"test");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    data.suggestions_url = "https://www.google.com/suggest?q={searchTerms}";
    template_url_ = std::make_unique<TemplateURL>(data);
  }

  std::unique_ptr<SuggestEntityImageURLFetcher> Start(
      const std::u16string& query = u"santa monica") {
    auto fetcher = std::make_unique<SuggestEntityImageURLFetcher>(
        scheme_classifier_, mojom::ClientId::Journeys, query, std::string());
    fetcher->Start(template_url_.get(), search_terms_data_,
                   &remote_suggestions_service_,
                   base::BindLambdaForTesting([this](const GURL& url) {
                     ++callback_count_;
                     result_ = url;
                   }));
    return fetcher;
  }

  void Respond(const std::string& body,
               net::HttpStatusCode status = net::HTTP_OK) {
    ASSERT_EQ(url_loader_factory_.NumPending(), 1);
    const GURL url = url_loader_factory_.GetPendingRequest(0)->request.url;
    url_loader_factory_.AddResponse(url.spec(), body, status);
    ASSERT_TRUE(base::test::RunUntil([&] { return callback_count_ > 0; }));
  }

  void ExpectResult(const GURL& url, PageImageServiceResult result) {
    EXPECT_EQ(callback_count_, 1);
    EXPECT_EQ(result_, url);
    histogram_tester_.ExpectUniqueSample(kBackendSuggestResultHistogramName,
                                         result, 1);
    histogram_tester_.ExpectUniqueSample(
        "PageImageService.Backend.Suggest.Result.Journeys", result, 1);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory url_loader_factory_;
  RemoteSuggestionsService remote_suggestions_service_{
      /*document_suggestions_service=*/nullptr,
      /*enterprise_search_aggregator_suggestions_service=*/nullptr,
      url_loader_factory_.GetSafeWeakWrapper()};
  TestSchemeClassifier scheme_classifier_;
  SearchTermsData search_terms_data_;
  std::unique_ptr<TemplateURL> template_url_;
  base::HistogramTester histogram_tester_;
  int callback_count_ = 0;
  GURL result_;
};

TEST_F(SuggestEntityImageURLFetcherTest, MatchesQueryIgnoringCase) {
  auto fetcher = Start(u"SaNtA MoNiCa");
  EXPECT_EQ(callback_count_, 0);
  Respond(MakeResponse({{"SANTA MONICA", "https://example.com/image.png"}}));
  ExpectResult(GURL("https://example.com/image.png"),
               PageImageServiceResult::kSuccess);
}

TEST_F(SuggestEntityImageURLFetcherTest, SkipsNonMatchingAndInvalidImages) {
  auto fetcher = Start();
  Respond(MakeResponse({{"los angeles", "https://example.com/other.png"},
                        {"santa monica", "not a URL"},
                        {"santa monica", "https://example.com/matching.png"}}));
  ExpectResult(GURL("https://example.com/matching.png"),
               PageImageServiceResult::kSuccess);
}

TEST_F(SuggestEntityImageURLFetcherTest, NoMatchingImage) {
  auto fetcher = Start();
  Respond(MakeResponse({{"los angeles", "https://example.com/other.png"},
                        {"santa monica", ""}}));
  ExpectResult(GURL(), PageImageServiceResult::kNoImage);
}

TEST_F(SuggestEntityImageURLFetcherTest, NoImageMetadata) {
  auto fetcher = Start();
  Respond(R"(["santa monica", ["santa monica"]])");
  ExpectResult(GURL(), PageImageServiceResult::kNoImage);
}

TEST_F(SuggestEntityImageURLFetcherTest, HttpError) {
  auto fetcher = Start();
  Respond(MakeResponse({{"santa monica", "https://example.com/image.png"}}),
          net::HTTP_INTERNAL_SERVER_ERROR);
  ExpectResult(GURL(), PageImageServiceResult::kResponseMissing);
}

class SuggestEntityImageURLFetcherMalformedResponseTest
    : public SuggestEntityImageURLFetcherTest,
      public testing::WithParamInterface<const char*> {};

TEST_P(SuggestEntityImageURLFetcherMalformedResponseTest, RejectsResponse) {
  auto fetcher = Start();
  Respond(GetParam());
  ExpectResult(GURL(), PageImageServiceResult::kResponseMalformed);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SuggestEntityImageURLFetcherMalformedResponseTest,
                         testing::Values("",
                                         "not json",
                                         "[]",
                                         R"(["santa monica", 123])",
                                         R"(["different query", []])"));

TEST_F(SuggestEntityImageURLFetcherTest,
       NoSuggestionsUrlCompletesAsynchronously) {
  TemplateURLData data;
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  template_url_ = std::make_unique<TemplateURL>(data);
  auto fetcher = Start();
  EXPECT_EQ(callback_count_, 0);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
  ASSERT_TRUE(base::test::RunUntil([&] { return callback_count_ > 0; }));
  ExpectResult(GURL(), PageImageServiceResult::kResponseMissing);
}

TEST_F(SuggestEntityImageURLFetcherTest, CallbackCanDestroyFetcher) {
  auto fetcher = std::make_unique<SuggestEntityImageURLFetcher>(
      scheme_classifier_, mojom::ClientId::Journeys, u"santa monica",
      std::string());
  fetcher->Start(template_url_.get(), search_terms_data_,
                 &remote_suggestions_service_,
                 base::BindLambdaForTesting([&](const GURL& url) {
                   ++callback_count_;
                   result_ = url;
                   fetcher.reset();
                 }));
  Respond(MakeResponse({{"santa monica", "https://example.com/image.png"}}));
  EXPECT_FALSE(fetcher);
  ExpectResult(GURL("https://example.com/image.png"),
               PageImageServiceResult::kSuccess);
}

TEST_F(SuggestEntityImageURLFetcherTest, DestructionCancelsPendingRequest) {
  auto fetcher = Start();
  ASSERT_EQ(url_loader_factory_.NumPending(), 1);
  base::test::TestFuture<void> disconnected;
  url_loader_factory_.GetPendingRequest(0)->client.set_disconnect_handler(
      disconnected.GetCallback());
  fetcher.reset();
  ASSERT_TRUE(disconnected.Wait());
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(callback_count_, 0);
  histogram_tester_.ExpectTotalCount(kBackendSuggestResultHistogramName, 0);
}

}  // namespace
}  // namespace page_image_service
