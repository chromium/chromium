// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::Ne;
using testing::NotNull;
using testing::SizeIs;

const char kAccessToken[] = "access_token";
const char kEncodedPrivateKey[] =
    "49e052293c29b5a50b0013eec9d030ac2ad70a42fe093be084264647cb04e16f";
const char kTestURL[] = "https://test.com/test";
const char kTestJoinSecurityDomainsURL[] =
    "https://test.com/test/domain:join?alt=proto";
const char kTestListSecurityDomainsURL[] =
    "https://test.com/test/domain:list?view=1&alt=proto";

std::unique_ptr<SecureBoxKeyPair> MakeTestKeyPair() {
  std::vector<uint8_t> private_key_bytes;
  bool success = base::HexStringToBytes(kEncodedPrivateKey, &private_key_bytes);
  DCHECK(success);
  return SecureBoxKeyPair::CreateByPrivateKeyImport(private_key_bytes);
}

class FakeTrustedVaultAccessTokenFetcher
    : public TrustedVaultAccessTokenFetcher {
 public:
  explicit FakeTrustedVaultAccessTokenFetcher(
      const base::Optional<std::string>& access_token)
      : access_token_(access_token) {}
  ~FakeTrustedVaultAccessTokenFetcher() override = default;

  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override {
    base::Optional<signin::AccessTokenInfo> access_token_info;
    if (access_token_) {
      access_token_info = signin::AccessTokenInfo(
          *access_token_, /*expiration_time_param=*/base::Time::Now() +
                              base::TimeDelta::FromHours(1),
          /*id_token=*/std::string());
    }
    std::move(callback).Run(access_token_info);
  }

 private:
  const base::Optional<std::string> access_token_;
};

class TrustedVaultConnectionImplTest : public testing::Test {
 public:
  TrustedVaultConnectionImplTest()
      : connection_(
            GURL(kTestURL),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)
                ->Clone(),
            std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
                kAccessToken)) {}

  ~TrustedVaultConnectionImplTest() override = default;

  TrustedVaultConnectionImpl* connection() { return &connection_; }

  // Allows overloading of FakeTrustedVaultAccessTokenFetcher behavior, doesn't
  // overwrite connection().
  std::unique_ptr<TrustedVaultConnectionImpl> CreateConnectionWithAccessToken(
      base::Optional<std::string> access_token) {
    return std::make_unique<TrustedVaultConnectionImpl>(
        GURL(kTestURL),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_)
            ->Clone(),
        std::make_unique<FakeTrustedVaultAccessTokenFetcher>(access_token));
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingHTTPRequest() {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.GetPendingRequest(/*index=*/0);
  }

  bool RespondToJoinSecurityDomainsRequest(
      net::HttpStatusCode response_http_code) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        kTestJoinSecurityDomainsURL,
        /*content=*/std::string(), response_http_code);
  }

  bool RespondToListSecurityDomainsRequest(
      net::HttpStatusCode response_http_code) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        kTestListSecurityDomainsURL, /*content=*/std::string(),
        response_http_code);
  }

  const std::vector<uint8_t> kTrustedVaultKey = {1, 2, 3, 4};

 private:
  base::test::TaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  TrustedVaultConnectionImpl connection_;
};

TEST_F(TrustedVaultConnectionImplTest, ShouldSendJoinSecurityDomainsRequest) {
  const int kLastKeyVersion = 1234;

  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, kLastKeyVersion),
          key_pair->public_key(),
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  network::TestURLLoaderFactory::PendingRequest* pending_http_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_http_request, NotNull());

  const network::ResourceRequest& resource_request =
      pending_http_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GURL(std::string(kTestURL) + "/domain:join?alt=proto")));

  sync_pb::JoinSecurityDomainsRequest deserialized_body;
  deserialized_body.ParseFromString(network::GetUploadData(resource_request));

  ASSERT_THAT(deserialized_body.security_domains(), SizeIs(1));
  const sync_pb::SecurityDomain& security_domain =
      deserialized_body.security_domains(0);
  EXPECT_THAT(security_domain.name(), Eq("chromesync"));

  ASSERT_THAT(security_domain.members(), SizeIs(1));
  const sync_pb::SecurityDomain::Member& member = security_domain.members(0);
  EXPECT_THAT(ProtoStringToBytes(member.public_key()),
              Eq(key_pair->public_key().ExportToBytes()));

  ASSERT_THAT(member.keys(), SizeIs(1));
  const sync_pb::SharedKey& shared_key = member.keys(0);
  EXPECT_THAT(shared_key.epoch(), Eq(kLastKeyVersion));

  base::Optional<std::vector<uint8_t>> decrypted_trusted_vault_key =
      DecryptTrustedVaultWrappedKey(
          key_pair->private_key(),
          /*wrapped_key=*/ProtoStringToBytes(shared_key.wrapped_key()));
  ASSERT_THAT(decrypted_trusted_vault_key, Ne(base::nullopt));
  EXPECT_THAT(*decrypted_trusted_vault_key, Eq(kTrustedVaultKey));

  // HMAC_SHA256 result using |key_pair| public key as message and
  // kTrustedVaultKey as a secret key.
  const std::string kHexEncodedExpectedMemberProof =
      "247b19e1a835dff90405b349413d51a8a9c3c10035772cf5d60e9403053e67fd";
  std::string expected_member_proof;
  ASSERT_TRUE(base::HexStringToString(kHexEncodedExpectedMemberProof,
                                      &expected_member_proof));
  EXPECT_THAT(shared_key.member_proof(), Eq(expected_member_proof));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleSuccessfulJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, /*version=*/1),
          key_pair->public_key(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kSuccess)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(net::HTTP_OK));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, /*version=*/1),
          key_pair->public_key(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kOtherError)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_INTERNAL_SERVER_ERROR));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithBadRequestStatus) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, /*version=*/1),
          key_pair->public_key(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // In particular, HTTP_BAD_REQUEST indicates that
  // |last_trusted_vault_key_and_version| is not actually the last on the server
  // side.
  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kLocalDataObsolete)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(net::HTTP_BAD_REQUEST));
}

TEST_F(
    TrustedVaultConnectionImplTest,
    ShouldHandleAccessTokenFetchingFailureWhenRegisteringAuthenticationFactor) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessToken(
          /*access_token=*/base::nullopt);

  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  // |callback| is called immediately after RegisterAuthenticationFactor(),
  // because there is no access token.
  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kOtherError)));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, /*version=*/1),
          key_pair->public_key(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_F(TrustedVaultConnectionImplTest, ShouldCancelJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, /*version=*/1),
          key_pair->public_key(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToJoinSecurityDomainsRequest(net::HTTP_OK);
}

TEST_F(TrustedVaultConnectionImplTest, ShouldSendListSecurityDomainsRequest) {
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(/*key=*/std::vector<uint8_t>(),
                                    /*version=*/0),
          /*device_key_pair=*/MakeTestKeyPair(), base::DoNothing());
  EXPECT_THAT(request, NotNull());

  network::TestURLLoaderFactory::PendingRequest* pending_http_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_http_request, NotNull());

  const network::ResourceRequest& resource_request =
      pending_http_request->request;
  EXPECT_THAT(resource_request.method, Eq("GET"));
  EXPECT_THAT(resource_request.url, Eq(GURL(kTestListSecurityDomainsURL)));
}

// TODO(crbug.com/1113598): add coverage for at least one successful case
// (need to share some helper functions with
// download_keys_response_handler_unittest.cc).
TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedListSecurityDomainsRequest) {
  base::MockCallback<TrustedVaultConnection::DownloadKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(/*key=*/std::vector<uint8_t>(),
                                    /*version=*/0),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kOtherError), _, _));
  EXPECT_TRUE(
      RespondToListSecurityDomainsRequest(net::HTTP_INTERNAL_SERVER_ERROR));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleAccessTokenFetchingFailureWhenDownloadingKeys) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessToken(
          /*access_token=*/base::nullopt);

  base::MockCallback<TrustedVaultConnection::DownloadKeysCallback> callback;

  // |callback| is called immediately after DownloadKeys(), because there is no
  // access token.
  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kOtherError), _, _));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->DownloadKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(
              /*key=*/std::vector<uint8_t>(),
              /*version=*/0),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_F(TrustedVaultConnectionImplTest, ShouldCancelListSecurityDomainsRequest) {
  base::MockCallback<TrustedVaultConnection::DownloadKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(
              /*key=*/std::vector<uint8_t>(),
              /*version=*/0),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToListSecurityDomainsRequest(net::HTTP_OK);
}

}  // namespace

}  // namespace syncer
