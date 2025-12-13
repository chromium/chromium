// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_connection.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/recovery_key_store_connection_impl.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/fake_trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/test/recovery_key_store_certificate_test_util.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using base::test::EqualsProto;
using testing::Eq;
using testing::HasSubstr;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::UnorderedElementsAre;
using ResponseMatchFlags = network::TestURLLoaderFactory::ResponseMatchFlags;

using UpdateRecoveryKeyStoreFuture =
    base::test::TestFuture<RecoveryKeyStoreStatus>;
using ListVaultsFuture = base::test::TestFuture<
    base::expected<std::vector<RecoveryKeyStoreEntry>, RecoveryKeyStoreStatus>>;
using FetchCertCallback = base::MockCallback<
    RecoveryKeyStoreConnection::FetchRecoveryKeyStoreCertificatesCallback>;
using FetchCertFuture = base::test::TestFuture<
    base::expected<RecoveryKeyStoreCertificate,
                   RecoveryKeyStoreCertificateFetchStatus>>;

constexpr char kPasskeysApplicationKeyName[] =
    "security_domain_member_key_encrypted_locally";

constexpr std::string_view kBackendPublicKey1 = "backend-public-key-1";
constexpr std::string_view kVaultHandle1 = "vault-handle-1";
constexpr std::string_view kBackendPublicKey2 = "backend-public-key-2";
constexpr std::string_view kVaultHandle2 = "vault-handle-2";

class RecoveryKeyStoreConnectionImplTest : public testing::Test {
 protected:
  static constexpr char kUpdateVaultUrl[] =
      "https://cryptauthvault.googleapis.com/v1/vaults/0?alt=proto";
  static constexpr char kListVaultsUrl[] =
      "https://cryptauthvault.googleapis.com/v1/vaults";
  static constexpr char kRecoveryKeyStoreCertFileUrl[] =
      "https://www.gstatic.com/cryptauthvault/v0/cert.xml";
  static constexpr char kRecoveryKeyStoreSigFileUrl[] =
      "https://www.gstatic.com/cryptauthvault/v0/cert.sig.xml";

  RecoveryKeyStoreConnectionImplTest() {
    clock_.SetNow(test_certs::kValidCertificateDate);
  }
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

  bool SimulateResponse(std::string_view url,
                        net::HttpStatusCode response_http_code,
                        std::string_view content = "") {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, content, response_http_code,
        static_cast<ResponseMatchFlags>(ResponseMatchFlags::kUrlMatchPrefix |
                                        ResponseMatchFlags::kWaitForRequest));
  }

  bool SimulateNetworkError(std::string_view url, net::Error err) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(url), network::URLLoaderCompletionStatus(err),
        network::mojom::URLResponseHead::New(),
        /*content=*/"",
        static_cast<ResponseMatchFlags>(ResponseMatchFlags::kUrlMatchPrefix |
                                        ResponseMatchFlags::kWaitForRequest));
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingHTTPRequest() {
    test_url_loader_factory_.WaitForRequest(
        GURL(""), ResponseMatchFlags::kUrlMatchPrefix);
    CHECK_EQ(test_url_loader_factory_.pending_requests()->size(), 1u);
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

  base::SimpleTestClock clock_;
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

  EXPECT_CALL(callback, Run(Eq(RecoveryKeyStoreStatus::kSuccess)));
  SimulateResponse(kUpdateVaultUrl, net::HttpStatusCode::HTTP_OK);
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

  UpdateRecoveryKeyStoreFuture future;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->UpdateRecoveryKeyStore(CoreAccountInfo(), MakeRequest(),
                                         future.GetCallback());
  ASSERT_THAT(request, NotNull());
  EXPECT_EQ(future.Get(),
            RecoveryKeyStoreStatus::kPersistentAccessTokenFetchError);

  // No network requests should be made.
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);
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

    EXPECT_CALL(callback, Run(Eq(RecoveryKeyStoreStatus::kOtherError)));
    SimulateResponse(kUpdateVaultUrl, http_status);
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

  EXPECT_CALL(callback, Run(Eq(RecoveryKeyStoreStatus::kNetworkError)));
  SimulateNetworkError(kUpdateVaultUrl, net::ERR_FAILED);
}

TEST_F(RecoveryKeyStoreConnectionImplTest, ShouldListVaults) {
  ListVaultsFuture future;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->ListRecoveryKeyStores(CoreAccountInfo(),
                                          future.GetCallback());
  ASSERT_THAT(request, NotNull());

  trusted_vault_pb::ListVaultsResponse response;
  trusted_vault_pb::Vault* vault = response.add_vaults();
  vault->mutable_vault_parameters()->set_backend_public_key(kBackendPublicKey1);
  vault->mutable_vault_parameters()->set_vault_handle(kVaultHandle1);
  network::ResourceRequest url_request = GetPendingHTTPRequest()->request;
  EXPECT_EQ(url_request.method, "GET");
  EXPECT_THAT(url_request.url.GetQuery(), Not(HasSubstr("page_token")));
  EXPECT_THAT(url_request.url.GetQuery(), HasSubstr("use_case=13"));
  EXPECT_THAT(url_request.url.GetQuery(),
              HasSubstr("challenge_not_required=1"));
  SimulateResponse(kListVaultsUrl, net::HTTP_OK, response.SerializeAsString());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().size(), 1u);
  EXPECT_EQ(result.value().at(0).backend_public_key,
            ProtoStringToBytes(kBackendPublicKey1));
  EXPECT_EQ(result.value().at(0).vault_handle,
            ProtoStringToBytes(kVaultHandle1));
}

TEST_F(RecoveryKeyStoreConnectionImplTest, ShouldListVaultsWithPagination) {
  ListVaultsFuture future;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->ListRecoveryKeyStores(CoreAccountInfo(),
                                          future.GetCallback());
  ASSERT_THAT(request, NotNull());
  {
    trusted_vault_pb::ListVaultsResponse response;
    trusted_vault_pb::Vault* vault = response.add_vaults();
    vault->mutable_vault_parameters()->set_backend_public_key(
        kBackendPublicKey1);
    vault->mutable_vault_parameters()->set_vault_handle(kVaultHandle1);
    response.set_next_page_token("next-page-token");
    EXPECT_THAT(GetPendingHTTPRequest()->request.url.GetQuery(),
                Not(HasSubstr("page_token")));
    SimulateResponse(kListVaultsUrl, net::HTTP_OK,
                     response.SerializeAsString());
    ASSERT_FALSE(future.IsReady());
  }
  {
    trusted_vault_pb::ListVaultsResponse response;
    trusted_vault_pb::Vault* vault = response.add_vaults();
    vault->mutable_vault_parameters()->set_backend_public_key(
        kBackendPublicKey2);
    vault->mutable_vault_parameters()->set_vault_handle(kVaultHandle2);
    EXPECT_THAT(GetPendingHTTPRequest()->request.url.GetQuery(),
                HasSubstr("page_token=next-page-token"));
    SimulateResponse(kListVaultsUrl, net::HTTP_OK,
                     response.SerializeAsString());
  }
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().size(), 2u);
  EXPECT_EQ(result.value().at(0).backend_public_key,
            ProtoStringToBytes(kBackendPublicKey1));
  EXPECT_EQ(result.value().at(0).vault_handle,
            ProtoStringToBytes(kVaultHandle1));
  EXPECT_EQ(result.value().at(1).backend_public_key,
            ProtoStringToBytes(kBackendPublicKey2));
  EXPECT_EQ(result.value().at(1).vault_handle,
            ProtoStringToBytes(kVaultHandle2));
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleHttpErrorDuringListVaults) {
  base::MockCallback<RecoveryKeyStoreConnection::ListRecoveryKeyStoresCallback>
      callback;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->ListRecoveryKeyStores(CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(base::unexpected(RecoveryKeyStoreStatus::kOtherError))));
  SimulateResponse(kListVaultsUrl, net::HTTP_BAD_REQUEST);
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleNetworkErrorDuringListVaults) {
  base::MockCallback<RecoveryKeyStoreConnection::ListRecoveryKeyStoresCallback>
      callback;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->ListRecoveryKeyStores(CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(base::unexpected(RecoveryKeyStoreStatus::kNetworkError))));
  SimulateNetworkError(kListVaultsUrl, net::ERR_FAILED);
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleBadProtoResponseDuringListVaults) {
  base::MockCallback<RecoveryKeyStoreConnection::ListRecoveryKeyStoresCallback>
      callback;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->ListRecoveryKeyStores(CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(base::unexpected(RecoveryKeyStoreStatus::kOtherError))));
  SimulateResponse(kListVaultsUrl, net::HTTP_OK, "This is not a proto");
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleNetworkErrorDuringFetch) {
  for (const std::string_view error_url :
       {kRecoveryKeyStoreSigFileUrl, kRecoveryKeyStoreCertFileUrl}) {
    base::HistogramTester histogram_tester;
    FetchCertCallback callback;
    std::unique_ptr<TrustedVaultConnection::Request> request =
        connection()->FetchRecoveryKeyStoreCertificates(&clock_,
                                                        callback.Get());
    ASSERT_THAT(request, NotNull());
    EXPECT_CALL(callback,
                Run(Eq(base::unexpected(
                    RecoveryKeyStoreCertificateFetchStatus::kNetworkError))));
    SimulateNetworkError(error_url, net::ERR_FAILED);
    histogram_tester.ExpectUniqueSample(
        "TrustedVault.RecoveryKeyStoreCertificatesFetchStatus",
        RecoveryKeyStoreCertificatesFetchStatusForUMA::kNetworkError, 1);
  }
}

TEST_F(RecoveryKeyStoreConnectionImplTest,
       ShouldHandleParseErrorDuringFetchCerts) {
  base::HistogramTester histogram_tester;
  FetchCertCallback callback;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->FetchRecoveryKeyStoreCertificates(&clock_, callback.Get());
  ASSERT_THAT(request, NotNull());
  EXPECT_CALL(callback,
              Run(Eq(base::unexpected(
                  RecoveryKeyStoreCertificateFetchStatus::kParseError))));
  SimulateResponse(kRecoveryKeyStoreCertFileUrl, net::HTTP_OK,
                   "This is not a certificate file");
  SimulateResponse(kRecoveryKeyStoreSigFileUrl, net::HTTP_OK,
                   test_certs::kSigXml);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryKeyStoreCertificatesFetchStatus",
      RecoveryKeyStoreCertificatesFetchStatusForUMA::kParseError, 1);
}

TEST_F(RecoveryKeyStoreConnectionImplTest, ShouldFetchAndParseCertificates) {
  base::HistogramTester histogram_tester;
  FetchCertFuture future;
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->FetchRecoveryKeyStoreCertificates(&clock_,
                                                      future.GetCallback());
  SimulateResponse(kRecoveryKeyStoreCertFileUrl, net::HTTP_OK,
                   test_certs::kCertXml);
  SimulateResponse(kRecoveryKeyStoreSigFileUrl, net::HTTP_OK,
                   test_certs::kSigXml);
  base::expected<RecoveryKeyStoreCertificate,
                 RecoveryKeyStoreCertificateFetchStatus>
      result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->endpoint_public_keys().size(), 3u);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.RecoveryKeyStoreCertificatesFetchStatus",
      RecoveryKeyStoreCertificatesFetchStatusForUMA::kSuccess, 1);
}

}  // namespace

}  // namespace trusted_vault
