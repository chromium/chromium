// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/recovery_key_store_connection_impl.h"
#include "components/trusted_vault/test/fake_trusted_vault_access_token_fetcher.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::Eq;
using testing::IsNull;
using testing::NotNull;

constexpr char kPasskeysApplicationKeyName[] =
    "security_domain_member_key_encrypted_locally";

class RecoveryKeyStoreConnectionImplTest : public testing::Test {
 protected:
  static constexpr char kUpdateVaultUrl[] =
      "https://cryptauthvault.googleapis.com/v1/vaults/0?alt=proto";

  RecoveryKeyStoreConnectionImplTest() = default;
  ~RecoveryKeyStoreConnectionImplTest() override = default;

  RecoveryKeyStoreConnection* connection() { return connection_.get(); }

  std::unique_ptr<RecoveryKeyStoreConnection::Request> UpdateRecoveryKeyStore(
      const CoreAccountInfo& account_info,
      const trusted_vault_pb::Vault& request,
      RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback callback) {
    return connection()->UpdateRecoveryKeyStore(account_info, request,
                                                std::move(callback));
  }

  trusted_vault_pb::Vault MakeRequest() {
    trusted_vault_pb::Vault vault;
    vault.add_application_keys()->set_key_name(kPasskeysApplicationKeyName);
    vault.mutable_chrome_os_metadata()->set_device_id("test device id");
    return vault;
  }

  bool SimulateUpdateRecoveryKeyStoreResponse(
      net::HttpStatusCode response_http_code) {
    // Allow request to reach `test_url_loader_factory_`.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        kUpdateVaultUrl, /*content=*/"", response_http_code);
  }

  bool SimulateUpdateRecoveryKeyStoreNetworkError(net::Error err) {
    // Allow request to reach `test_url_loader_factory_`.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kUpdateVaultUrl), network::URLLoaderCompletionStatus(err),
        network::mojom::URLResponseHead::New(),
        /*content=*/"");
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingHTTPRequest() {
    // Allow request to reach `test_url_loader_factory_`.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.GetPendingRequest(/*index=*/0);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<RecoveryKeyStoreConnection> connection_ =
      std::make_unique<RecoveryKeyStoreConnectionImpl>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)
              ->Clone(),
          std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
              signin::AccessTokenInfo(
                  "test access token",
                  /*expiration_time_param=*/base::Time::Max(),
                  /*id_token=*/"")));
};

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldUpdateRecoveryKeyStoreAndHandleSuccess) {
  const trusted_vault_pb::Vault request_proto = MakeRequest();

  base::MockCallback<RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      UpdateRecoveryKeyStore(CoreAccountInfo(), request_proto, callback.Get());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("PATCH"));
  EXPECT_THAT(resource_request.url, Eq(kUpdateVaultUrl));

  trusted_vault_pb::Vault deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(network::GetUploadData(resource_request),
              Eq(request_proto.SerializeAsString()));

  EXPECT_CALL(callback, Run(Eq(UpdateRecoveryKeyStoreStatus::kSuccess)));
  SimulateUpdateRecoveryKeyStoreResponse(net::HttpStatusCode::HTTP_OK);
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleAccessTokenFetchFailureDuringUpdateRecoveryKeyStore) {
  // Simulate a persistent failure during access token fetching when making an
  // RecoveryKeyStoreConnection requests.
  auto connection = std::make_unique<RecoveryKeyStoreConnectionImpl>(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_)
          ->Clone(),
      std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
          base::unexpected(TrustedVaultAccessTokenFetcher::FetchingError::
                               kPersistentAuthError)));

  base::MockCallback<RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(Eq(UpdateRecoveryKeyStoreStatus::kPersistentAccessTokenFetchError)));

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->UpdateRecoveryKeyStore(CoreAccountInfo(), MakeRequest(),
                                         callback.Get());
  ASSERT_THAT(request, NotNull());

  // No network requests should be made.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleFailedUpdateRecoveryKeyStoreRequest) {
  for (const net::HttpStatusCode http_status :
       {net::HTTP_BAD_REQUEST, net::HTTP_NOT_FOUND, net::HTTP_CONFLICT,
        net::HTTP_INTERNAL_SERVER_ERROR}) {
    base::MockCallback<
        RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback>
        callback;
    std::unique_ptr<TrustedVaultConnection::Request> request =
        connection()->UpdateRecoveryKeyStore(CoreAccountInfo(), MakeRequest(),
                                             callback.Get());
    ASSERT_THAT(request, NotNull());

    EXPECT_CALL(callback, Run(Eq(UpdateRecoveryKeyStoreStatus::kOtherError)));
    SimulateUpdateRecoveryKeyStoreResponse(http_status);
  }
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleNetworkErrorDuringUpdateRecoveryKeyStore) {
  base::MockCallback<RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback>
      callback;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->UpdateRecoveryKeyStore(CoreAccountInfo(), MakeRequest(),
                                           callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(UpdateRecoveryKeyStoreStatus::kNetworkError)));
  SimulateUpdateRecoveryKeyStoreNetworkError(net::ERR_FAILED);
}

}  // namespace

}  // namespace trusted_vault
