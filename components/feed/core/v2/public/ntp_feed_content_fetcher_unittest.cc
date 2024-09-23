// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/ntp_feed_content_fetcher.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/feed/core/proto/v2/wire/data_operation.pb.h"
#include "components/feed/core/proto/v2/wire/feed_response.pb.h"
#include "components/feed/core/proto/v2/wire/payload_metadata.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/proto/v2/wire/stream_structure.pb.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace feed::test {
namespace {

const char* kExampleUrl = "http://example.com/";
const char* kEmail = "user@example.com";

feedwire::Response MakeFeedResponse(int count) {
  feedwire::Response response;
  response.set_response_version(feedwire::Response::FEED_RESPONSE);
  feedwire::FeedResponse* feed_response = response.mutable_feed_response();
  for (int i = 0; i < count; ++i) {
    feedwire::DataOperation* op = feed_response->add_data_operation();
    op->set_operation(feedwire::DataOperation::UPDATE_OR_APPEND);
    feedwire::PrefetchMetadata* metadata =
        op->mutable_feature()->mutable_content()->add_prefetch_metadata();
    std::string number = base::NumberToString(i);
    metadata->set_uri(base::StrCat({kExampleUrl, number}));
    metadata->set_title(base::StrCat({"Article ", number}));
    metadata->set_favicon_url(
        base::StrCat({kExampleUrl, number, "/favicon.ico"}));
    metadata->set_image_url(
        base::StrCat({kExampleUrl, number, "/thumbnail.jpg"}));
    metadata->set_publisher(base::StrCat({"Publisher ", number}));
  }
  return response;
}

}  // namespace

class NtpFeedContentFetcherTest : public testing::Test {
 public:
  NtpFeedContentFetcherTest() {
    identity_test_env_.SetPrimaryAccount(kEmail, signin::ConsentLevel::kSync);
  }
  NtpFeedContentFetcherTest(NtpFeedContentFetcherTest&) = delete;
  NtpFeedContentFetcherTest& operator=(const NtpFeedContentFetcherTest&) =
      delete;
  ~NtpFeedContentFetcherTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    ntp_feed_content_fetcher_ = std::make_unique<NtpFeedContentFetcher>(
        identity_test_env_.identity_manager(), shared_url_loader_factory_,
        "dummy_api_key", &profile_prefs_);

    auto feed_network = std::make_unique<TestFeedNetwork>();
    feed_network->SendResponsesOnCommand(false);
    feed_network_ = feed_network.get();
    ntp_feed_content_fetcher_->SetFeedNetworkForTesting(
        std::move(feed_network));
  }

  void TearDown() override { ntp_feed_content_fetcher_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<NtpFeedContentFetcher> ntp_feed_content_fetcher_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<TestFeedNetwork, DanglingUntriaged> feed_network_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  TestingPrefServiceSimple profile_prefs_;
};

TEST_F(NtpFeedContentFetcherTest, FetchFollowingFeedArticles) {
  std::vector<NtpFeedContentFetcher::Article> actual_articles;
  base::MockOnceCallback<void(std::vector<NtpFeedContentFetcher::Article>)>
      callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::vector<NtpFeedContentFetcher::Article> articles) {
            actual_articles = std::move(articles);
          }));

  // Inject a response of 5 articles. We should only get 3.
  feed_network_->InjectApiResponse<WebFeedListContentsDiscoverApi>(
      MakeFeedResponse(5));
  ntp_feed_content_fetcher_->FetchFollowingFeedArticles(callback.Get());

  EXPECT_EQ(kEmail, feed_network_->last_account_info.email);
  std::optional<feedwire::Request> sent_request =
      feed_network_->GetApiRequestSent<WebFeedListContentsDiscoverApi>();
  ASSERT_TRUE(sent_request.has_value());
  // TODO(crbug.com/40842320): Add a Chrome Desktop client type.
  EXPECT_EQ(feedwire::ClientInfo::ANDROID_ID,
            sent_request->feed_request().client_info().platform_type());
  EXPECT_EQ(feedwire::ClientInfo::CHROME_ANDROID,
            sent_request->feed_request().client_info().app_type());

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3ul, actual_articles.size());
  auto& article = actual_articles.front();
  EXPECT_EQ("http://example.com/0", article.url);
  EXPECT_EQ("Article 0", article.title);
  EXPECT_EQ("Publisher 0", article.publisher);
  EXPECT_EQ("http://example.com/0/favicon.ico", article.favicon_url);
  EXPECT_EQ("http://example.com/0/thumbnail.jpg", article.thumbnail_url);
}

}  // namespace feed::test
