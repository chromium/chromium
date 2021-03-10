// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <utility>

#include "base/base64url.h"
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
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
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

// TODO(crbug.com/1113598): revisit this tests suite and determine what actually
// should be tested on the Connection level and what should be done on lower
// levels (DownloadKeysResponseHandler and RegisterAuthenticationFactorRequest).
class TrustedVaultConnectionImplTest : public testing::Test {
 public:
  TrustedVaultConnectionImplTest()
      : connection_(
            kTestURL,
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
        kTestURL,
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
        GetFullJoinSecurityDomainsURLForTesting(kTestURL).spec(),
        /*content=*/std::string(), response_http_code);
  }

  bool RespondToGetSecurityDomainMemberRequest(
      net::HttpStatusCode response_http_code) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullGetSecurityDomainMemberURLForTesting(
            kTestURL, MakeTestKeyPair()->public_key().ExportToBytes())
            .spec(),
        /*content=*/std::string(), response_http_code);
  }

  const std::vector<uint8_t> kTrustedVaultKey = {1, 2, 3, 4};
  const GURL kTestURL = GURL("https://test.com/test");

 private:
  base::test::TaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  TrustedVaultConnectionImpl connection_;
};

TEST_F(TrustedVaultConnectionImplTest, ShouldSendJoinSecurityDomainsRequest) {
  const TrustedVaultKeyAndVersion kTrustedVaultKeyAndVersion(kTrustedVaultKey,
                                                             /*version=*/1234);
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeyAndVersion,
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GetFullJoinSecurityDomainsURLForTesting(kTestURL)));

  sync_pb::JoinSecurityDomainsRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain().name(),
              Eq(kSyncSecurityDomainName));

  std::string public_key_string;
  AssignBytesToProtoString(key_pair->public_key().ExportToBytes(),
                           &public_key_string);

  std::string encoded_public_key;
  base::Base64UrlEncode(public_key_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  const sync_pb::SecurityDomainMember& member =
      deserialized_body.security_domain_member();
  EXPECT_THAT(member.name(),
              Eq(kSecurityDomainMemberNamePrefix + encoded_public_key));
  EXPECT_THAT(member.public_key(), Eq(public_key_string));
  EXPECT_THAT(member.member_type(),
              Eq(sync_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE));

  const sync_pb::SharedMemberKey& shared_key =
      deserialized_body.shared_member_key();
  EXPECT_THAT(shared_key.epoch(), Eq(kTrustedVaultKeyAndVersion.version));

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(
                  key_pair->private_key(),
                  /*wrapped_key=*/ProtoStringToBytes(shared_key.wrapped_key())),
              Eq(kTrustedVaultKeyAndVersion.key));
  EXPECT_TRUE(VerifyTrustedVaultHMAC(
      /*key=*/kTrustedVaultKeyAndVersion.key,
      /*data=*/key_pair->public_key().ExportToBytes(),
      /*digest=*/ProtoStringToBytes(shared_key.member_proof())));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestWithConstantKey) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          /*last_trusted_vault_key_and_version=*/base::nullopt,
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GetFullJoinSecurityDomainsURLForTesting(kTestURL)));

  sync_pb::JoinSecurityDomainsRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain().name(),
              Eq(kSyncSecurityDomainName));

  std::string public_key_string;
  AssignBytesToProtoString(key_pair->public_key().ExportToBytes(),
                           &public_key_string);

  std::string encoded_public_key;
  base::Base64UrlEncode(public_key_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  const sync_pb::SecurityDomainMember& member =
      deserialized_body.security_domain_member();
  EXPECT_THAT(member.name(),
              Eq(kSecurityDomainMemberNamePrefix + encoded_public_key));
  EXPECT_THAT(member.public_key(), Eq(public_key_string));
  EXPECT_THAT(member.member_type(),
              Eq(sync_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE));

  const sync_pb::SharedMemberKey& shared_key =
      deserialized_body.shared_member_key();
  EXPECT_FALSE(shared_key.has_epoch());

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(
                  key_pair->private_key(),
                  /*wrapped_key=*/ProtoStringToBytes(shared_key.wrapped_key())),
              Eq(GetConstantTrustedVaultKey()));
  EXPECT_TRUE(VerifyTrustedVaultHMAC(
      /*key=*/GetConstantTrustedVaultKey(),
      /*data=*/key_pair->public_key().ExportToBytes(),
      /*digest=*/ProtoStringToBytes(shared_key.member_proof())));
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
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          callback.Get());
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
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kOtherError)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_INTERNAL_SERVER_ERROR));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithNotFoundStatus) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, /*version=*/1),
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          callback.Get());
  ASSERT_THAT(request, NotNull());

  // In particular, HTTP_NOT_FOUND indicates that security domain was removed.
  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kLocalDataObsolete)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(net::HTTP_NOT_FOUND));
}

TEST_F(
    TrustedVaultConnectionImplTest,
    ShouldHandleFailedJoinSecurityDomainsRequestWithPreconditionFailedStatus) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(kTrustedVaultKey, /*version=*/1),
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          callback.Get());
  ASSERT_THAT(request, NotNull());

  // In particular, HTTP_PRECONDITION_FAILED indicates that
  // |last_trusted_vault_key_and_version| is not actually the last on the server
  // side.
  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kLocalDataObsolete)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_PRECONDITION_FAILED));
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
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          callback.Get());
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
          key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
          callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToJoinSecurityDomainsRequest(net::HTTP_OK);
}

TEST_F(TrustedVaultConnectionImplTest, ShouldSendListSecurityDomainsRequest) {
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
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
  EXPECT_THAT(resource_request.url,
              Eq(GetFullGetSecurityDomainMemberURLForTesting(
                  kTestURL, MakeTestKeyPair()->public_key().ExportToBytes())));
}

// TODO(crbug.com/1113598): add coverage for at least one successful case
// (need to share some helper functions with
// download_keys_response_handler_unittest.cc).
TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedListSecurityDomainsRequest) {
  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(/*key=*/std::vector<uint8_t>(),
                                    /*version=*/0),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kOtherError), _, _));
  EXPECT_TRUE(
      RespondToGetSecurityDomainMemberRequest(net::HTTP_INTERNAL_SERVER_ERROR));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleAccessTokenFetchingFailureWhenDownloadingKeys) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessToken(
          /*access_token=*/base::nullopt);

  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  // |callback| is called immediately after DownloadNewKeys(), because there is
  // no access token.
  EXPECT_CALL(callback, Run(Eq(TrustedVaultRequestStatus::kOtherError), _, _));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->DownloadNewKeys(
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
  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
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
  RespondToGetSecurityDomainMemberRequest(net::HTTP_OK);
}

}  // namespace

}  // namespace syncer
