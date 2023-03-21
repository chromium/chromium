// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Optional;

namespace safe_browsing {

namespace {
constexpr char kTestOhttpKey[] = "TestOhttpKey";
constexpr char kExpectedKeyFetchServerUrl[] =
    "https://safebrowsingohttpgateway.googleapis.com/key";
}  // namespace

class OhttpKeyServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    ohttp_key_service_ =
        std::make_unique<OhttpKeyService>(test_shared_loader_factory_);
  }

 protected:
  void SetupSuccessResponse() {
    test_url_loader_factory_->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& resource_request) {
          ASSERT_EQ(kExpectedKeyFetchServerUrl, resource_request.url.spec());
          ASSERT_EQ(network::mojom::CredentialsMode::kOmit,
                    resource_request.credentials_mode);
        }));
    test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                          kTestOhttpKey);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<OhttpKeyService> ohttp_key_service_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(OhttpKeyServiceTest, GetOhttpKey_Success) {
  SetupSuccessResponse();
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  EXPECT_CALL(response_callback, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();

  absl::optional<OhttpKeyService::OhttpKeyAndExpiration> ohttp_key =
      ohttp_key_service_->get_ohttp_key_for_testing();
  EXPECT_TRUE(ohttp_key.has_value());
  EXPECT_EQ(ohttp_key.value().expiration, base::Time::Now() + base::Days(30));
  EXPECT_EQ(ohttp_key.value().key, kTestOhttpKey);
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_Failure) {
  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        kTestOhttpKey, net::HTTP_FORBIDDEN);
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  EXPECT_CALL(response_callback, Run(Eq(absl::nullopt))).Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();

  absl::optional<OhttpKeyService::OhttpKeyAndExpiration> ohttp_key =
      ohttp_key_service_->get_ohttp_key_for_testing();
  // The key should not be cached if key fetch fails.
  EXPECT_FALSE(ohttp_key.has_value());
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_MultipleRequests) {
  base::MockCallback<OhttpKeyService::Callback> response_callback1;
  base::MockCallback<OhttpKeyService::Callback> response_callback2;
  EXPECT_CALL(response_callback1, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);
  EXPECT_CALL(response_callback2, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback1.Get());
  ohttp_key_service_->GetOhttpKey(response_callback2.Get());
  task_environment_.RunUntilIdle();

  SetupSuccessResponse();
  task_environment_.RunUntilIdle();
  // url_loader should only send one request
  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_WithValidCache) {
  SetupSuccessResponse();
  ohttp_key_service_->set_ohttp_key_for_testing(
      {"OldOhttpKey", base::Time::Now() + base::Hours(1)});

  base::MockCallback<OhttpKeyService::Callback> response_callback;
  // Should return the old key because it has not expired.
  EXPECT_CALL(response_callback, Run(Optional(std::string("OldOhttpKey"))))
      .Times(1);
  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(OhttpKeyServiceTest, GetOhttpKey_WithExpiredCache) {
  SetupSuccessResponse();
  ohttp_key_service_->set_ohttp_key_for_testing(
      {"OldOhttpKey", base::Time::Now() - base::Hours(1)});

  base::MockCallback<OhttpKeyService::Callback> response_callback1;
  // The new key should be fetched because the old key has expired.
  EXPECT_CALL(response_callback1, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service_->GetOhttpKey(response_callback1.Get());
  task_environment_.RunUntilIdle();

  test_url_loader_factory_->AddResponse(kExpectedKeyFetchServerUrl,
                                        "NewOhttpKey");
  task_environment_.FastForwardBy(base::Days(20));
  base::MockCallback<OhttpKeyService::Callback> response_callback2;
  // The new key should not be fetched because the old key has not expired.
  EXPECT_CALL(response_callback2, Run(Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service_->GetOhttpKey(response_callback2.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(OhttpKeyServiceTest, Shutdown) {
  base::MockCallback<OhttpKeyService::Callback> response_callback;
  // Pending callbacks should be run during shutdown.
  EXPECT_CALL(response_callback, Run(Eq(absl::nullopt))).Times(1);

  ohttp_key_service_->GetOhttpKey(response_callback.Get());
  ohttp_key_service_->Shutdown();
  task_environment_.RunUntilIdle();
}

}  // namespace safe_browsing
