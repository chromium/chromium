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
#include "components/sync/base/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace feed::test {
namespace {

const char* kEmail = "user@example.com";

}  // namespace

class NtpFeedContentFetcherTest : public testing::Test {
 public:
  NtpFeedContentFetcherTest() {
    identity_test_env_.SetPrimaryAccount(
        kEmail, syncer::IsReplaceSyncPromosWithSignInPromosEnabled()
                    ? signin::ConsentLevel::kSignin
                    : signin::ConsentLevel::kSync);
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

}  // namespace feed::test
