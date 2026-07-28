// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/http_rpc_based_ephemeral_key_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/model/crypto/agile_symmetric_key.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "components/sync_tab_context/proto/ephemeral_key_service.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_tab_context {
namespace {

using ::testing::NotNull;

const char kTestServerUrl[] = "https://example.com/generate_key";

class HttpRpcBasedEphemeralKeyFetcherTest : public ::testing::Test {
 protected:
  HttpRpcBasedEphemeralKeyFetcherTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        fetcher_(identity_test_env_.identity_manager(),
                 GetUrlLoaderFactoryGetter(),
                 GURL(kTestServerUrl)) {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  // Returns a `UrlLoaderFactoryGetter` callback that returns
  // `test_shared_loader_factory_`.
  HttpRpcBasedEphemeralKeyFetcher::UrlLoaderFactoryGetter
  GetUrlLoaderFactoryGetter() const {
    return base::BindRepeating(
        [](scoped_refptr<network::SharedURLLoaderFactory> factory) {
          return factory;
        },
        test_shared_loader_factory_);
  }

  // Synchronously fetches an ephemeral key using `fetcher_`, simulating an
  // HTTP response with `status_code` and `response_body`.
  std::optional<EphemeralKeyFetcher::Result> FetchEphemeralKey(
      net::HttpStatusCode status_code,
      const std::string& response_body) {
    base::test::TestFuture<std::optional<EphemeralKeyFetcher::Result>> future;
    fetcher_.FetchEphemeralKey(future.GetCallback());
    if (test_url_loader_factory_.IsPending(kTestServerUrl)) {
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          kTestServerUrl, response_body, status_code);
    }
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  HttpRpcBasedEphemeralKeyFetcher fetcher_;
};

TEST_F(HttpRpcBasedEphemeralKeyFetcherTest,
       ShouldReturnNulloptWhenServerUrlIsInvalid) {
  HttpRpcBasedEphemeralKeyFetcher fetcher(identity_test_env_.identity_manager(),
                                          GetUrlLoaderFactoryGetter(),
                                          GURL("invalid-url"));
  base::test::TestFuture<std::optional<EphemeralKeyFetcher::Result>> future;
  fetcher.FetchEphemeralKey(future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(HttpRpcBasedEphemeralKeyFetcherTest,
       ShouldReturnNulloptWhenAccessTokenFetchFails) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);
  base::test::TestFuture<std::optional<EphemeralKeyFetcher::Result>> future;
  fetcher_.FetchEphemeralKey(future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));
  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(HttpRpcBasedEphemeralKeyFetcherTest,
       ShouldReturnNulloptWhenHttpErrorOccurs) {
  EXPECT_EQ(FetchEphemeralKey(net::HTTP_INTERNAL_SERVER_ERROR, ""),
            std::nullopt);
}

TEST_F(HttpRpcBasedEphemeralKeyFetcherTest,
       ShouldSuccessfullyFetchAndParseEphemeralKey) {
  GenerateEphemeralKeyResponse response_proto;
  response_proto.set_server_token("test_server_token_123");

  auto key = syncer::AgileSymmetricKey::CreateRandom();
  sync_pb::AgileSymmetricKeySet* key_set_proto =
      response_proto.mutable_agile_symmetric_key_set();
  key_set_proto->set_primary_key_id(1);
  sync_pb::AgileSymmetricKeySet::Key* key_entry = key_set_proto->add_key();
  key_entry->set_key_id(1);
  *key_entry->mutable_key_data() = key->ToProto();

  std::optional<EphemeralKeyFetcher::Result> result =
      FetchEphemeralKey(net::HTTP_OK, response_proto.SerializeAsString());

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->server_token, "test_server_token_123");
  ASSERT_THAT(result->ephemeral_key, NotNull());
  EXPECT_EQ(result->ephemeral_key->size(), 1u);
}

TEST_F(HttpRpcBasedEphemeralKeyFetcherTest, ShouldSupportConcurrentRequests) {
  base::test::TestFuture<std::optional<EphemeralKeyFetcher::Result>> future1;
  base::test::TestFuture<std::optional<EphemeralKeyFetcher::Result>> future2;

  fetcher_.FetchEphemeralKey(future1.GetCallback());
  fetcher_.FetchEphemeralKey(future2.GetCallback());

  EXPECT_EQ(fetcher_.ongoing_operations_count_for_testing(), 2u);

  GenerateEphemeralKeyResponse response1;
  response1.set_server_token("token_server_1");
  auto key1 = syncer::AgileSymmetricKey::CreateRandom();
  response1.mutable_agile_symmetric_key_set()->set_primary_key_id(1);
  sync_pb::AgileSymmetricKeySet::Key* key_entry1 =
      response1.mutable_agile_symmetric_key_set()->add_key();
  key_entry1->set_key_id(1);
  *key_entry1->mutable_key_data() = key1->ToProto();

  GenerateEphemeralKeyResponse response2;
  response2.set_server_token("token_server_2");
  auto key2 = syncer::AgileSymmetricKey::CreateRandom();
  response2.mutable_agile_symmetric_key_set()->set_primary_key_id(2);
  sync_pb::AgileSymmetricKeySet::Key* key_entry2 =
      response2.mutable_agile_symmetric_key_set()->add_key();
  key_entry2->set_key_id(2);
  *key_entry2->mutable_key_data() = key2->ToProto();

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestServerUrl, response1.SerializeAsString());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTestServerUrl, response2.SerializeAsString());

  std::optional<EphemeralKeyFetcher::Result> res1 = future1.Take();
  std::optional<EphemeralKeyFetcher::Result> res2 = future2.Take();

  ASSERT_TRUE(res1.has_value());
  ASSERT_TRUE(res2.has_value());
  EXPECT_EQ(res1->server_token, "token_server_1");
  EXPECT_EQ(res2->server_token, "token_server_2");
  EXPECT_EQ(fetcher_.ongoing_operations_count_for_testing(), 0u);
}

TEST_F(HttpRpcBasedEphemeralKeyFetcherTest,
       ShouldFetchEphemeralKeyWithLazyUrlLoaderFactoryGetter) {
  testing::NiceMock<base::MockRepeatingCallback<
      scoped_refptr<network::SharedURLLoaderFactory>()>>
      mock_factory_getter;

  EXPECT_CALL(mock_factory_getter, Run()).Times(0);

  HttpRpcBasedEphemeralKeyFetcher fetcher(identity_test_env_.identity_manager(),
                                          mock_factory_getter.Get(),
                                          GURL(kTestServerUrl));

  EXPECT_CALL(mock_factory_getter, Run())
      .WillOnce(testing::Return(test_shared_loader_factory_));

  base::test::TestFuture<std::optional<EphemeralKeyFetcher::Result>> future;
  fetcher.FetchEphemeralKey(future.GetCallback());
}

TEST_F(HttpRpcBasedEphemeralKeyFetcherTest,
       ShouldReturnNulloptWhenUrlLoaderFactoryGetterReturnsNull) {
  HttpRpcBasedEphemeralKeyFetcher fetcher(
      identity_test_env_.identity_manager(),
      base::BindRepeating(
          []() -> scoped_refptr<network::SharedURLLoaderFactory> {
            return nullptr;
          }),
      GURL(kTestServerUrl));

  base::test::TestFuture<std::optional<EphemeralKeyFetcher::Result>> future;
  fetcher.FetchEphemeralKey(future.GetCallback());
  EXPECT_EQ(future.Take(), std::nullopt);
}

}  // namespace
}  // namespace sync_tab_context
