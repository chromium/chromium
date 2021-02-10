// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/register_authentication_factor_request.h"

#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Eq;
using testing::Pointee;

const char kAccessToken[] = "access_token";
const char kEncodedPrivateKey[] =
    "49e052293c29b5a50b0013eec9d030ac2ad70a42fe093be084264647cb04e16f";
const char kTestURL[] = "https://test.com/test";

std::unique_ptr<SecureBoxKeyPair> MakeTestKeyPair() {
  std::vector<uint8_t> private_key_bytes;
  bool success = base::HexStringToBytes(kEncodedPrivateKey, &private_key_bytes);
  DCHECK(success);
  return SecureBoxKeyPair::CreateByPrivateKeyImport(private_key_bytes);
}

MATCHER(IsValidListSecurityDomainsRequest, "") {
  const network::TestURLLoaderFactory::PendingRequest& pending_http_request =
      arg;
  const network::ResourceRequest& resource_request =
      pending_http_request.request;
  return resource_request.method == "GET" &&
         resource_request.url ==
             GetFullListSecurityDomainsURLForTesting(GURL(kTestURL));
}

MATCHER_P(IsValidJoinSecurityDomainsRequestWithSharedKey,
          trusted_vault_key_and_version,
          "") {
  const network::TestURLLoaderFactory::PendingRequest& pending_http_request =
      arg;
  const network::ResourceRequest& resource_request =
      pending_http_request.request;
  if (resource_request.method != "POST" ||
      resource_request.url !=
          GetFullJoinSecurityDomainsURLForTesting(GURL(kTestURL))) {
    return false;
  }

  sync_pb::JoinSecurityDomainsRequest deserialized_body;
  deserialized_body.ParseFromString(network::GetUploadData(resource_request));
  if (deserialized_body.security_domains_size() != 1) {
    return false;
  }

  sync_pb::SecurityDomain security_domain =
      deserialized_body.security_domains(0);
  if (security_domain.name() != kSyncSecurityDomainName ||
      security_domain.members_size() != 1) {
    return false;
  }

  const std::unique_ptr<SecureBoxKeyPair> expected_member_key_pair =
      MakeTestKeyPair();
  sync_pb::SecurityDomain::Member member = security_domain.members(0);
  if (ProtoStringToBytes(member.public_key()) !=
          expected_member_key_pair->public_key().ExportToBytes() ||
      member.keys_size() != 1) {
    return false;
  }

  sync_pb::SharedKey shared_key = member.keys(0);
  // |shared_key| should correspond to |trusted_vault_key_and_version| and
  // contain valid |member_proof|.
  return DecryptTrustedVaultWrappedKey(
             expected_member_key_pair->private_key(),
             /*wrapped_key=*/ProtoStringToBytes(shared_key.wrapped_key())) ==
             trusted_vault_key_and_version.key &&
         shared_key.epoch() == trusted_vault_key_and_version.version &&
         VerifyTrustedVaultHMAC(
             /*key=*/trusted_vault_key_and_version.key,
             /*data=*/expected_member_key_pair->public_key().ExportToBytes(),
             /*digest=*/ProtoStringToBytes(shared_key.member_proof()));
}

// TODO(crbug.com/1113598): FakeTrustedVaultAccessTokenFetcher used in three
// tests suites already, move it to dedicated file?
class FakeTrustedVaultAccessTokenFetcher
    : public TrustedVaultAccessTokenFetcher {
 public:
  FakeTrustedVaultAccessTokenFetcher() = default;
  ~FakeTrustedVaultAccessTokenFetcher() override = default;

  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override {
    std::move(callback).Run(signin::AccessTokenInfo(
        kAccessToken, /*expiration_time_param=*/base::Time::Now() +
                          base::TimeDelta::FromHours(1),
        /*id_token=*/std::string()));
  }
};

// TODO(crbug.com/1113598): factor out some helper functions from
// download_keys_response_handler.cc and add coverage for more cases when
// ListSecurityDomainsRequest is sent. Add coverage for various error cases.
class RegisterAuthenticationFactorRequestTest : public testing::Test {
 public:
  RegisterAuthenticationFactorRequestTest()
      : request_(
            /*join_security_domains_url=*/GURL(std::string(kTestURL) +
                                               kJoinSecurityDomainsURLPath),
            /*list_security_domains_url=*/
            GURL(std::string(kTestURL) + kListSecurityDomainsURLPathAndQuery),
            /*url_loader_factory=*/
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            CoreAccountId(),
            /*authentication_factor_public_key=*/
            MakeTestKeyPair()->public_key(),
            &access_token_fetcher_) {}
  ~RegisterAuthenticationFactorRequestTest() override = default;

  RegisterAuthenticationFactorRequest* request() { return &request_; }

  bool RespondToJoinSecurityDomainsRequest() {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullJoinSecurityDomainsURLForTesting(GURL(kTestURL)).spec(),
        /*content=*/std::string(), net::HttpStatusCode::HTTP_OK);
  }

  bool RespondToListSecurityDomainsRequest(
      const sync_pb::ListSecurityDomainsResponse& response) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullListSecurityDomainsURLForTesting(GURL(kTestURL)).spec(),
        /*content=*/response.SerializeAsString(), net::HttpStatusCode::HTTP_OK);
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingHTTPRequest() {
    return test_url_loader_factory_.GetPendingRequest(/*index=*/0);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeTrustedVaultAccessTokenFetcher access_token_fetcher_;

  RegisterAuthenticationFactorRequest request_;
};

TEST_F(RegisterAuthenticationFactorRequestTest,
       ShouldRegisterWithKnownTrustedVaultKeyAndVersion) {
  const TrustedVaultKeyAndVersion kVaultKeyAndVersion = {
      /*key=*/std::vector<uint8_t>{1, 2, 3, 4}, /*version=*/1234};
  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  request()->StartWithKnownTrustedVaultKeyAndVersion(kVaultKeyAndVersion,
                                                     callback.Get());
  EXPECT_THAT(GetPendingHTTPRequest(),
              Pointee(IsValidJoinSecurityDomainsRequestWithSharedKey(
                  kVaultKeyAndVersion)));

  EXPECT_CALL(callback, Run(TrustedVaultRequestStatus::kSuccess));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest());
}

TEST_F(RegisterAuthenticationFactorRequestTest,
       ShouldRegisterWithConstantKeyWhenSecurityDomainNotExists) {
  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;
  request()->StartWithConstantKey(callback.Get());
  EXPECT_THAT(GetPendingHTTPRequest(),
              Pointee(IsValidListSecurityDomainsRequest()));

  EXPECT_TRUE(RespondToListSecurityDomainsRequest(
      sync_pb::ListSecurityDomainsResponse()));
  EXPECT_THAT(GetPendingHTTPRequest(),
              Pointee(IsValidJoinSecurityDomainsRequestWithSharedKey(
                  TrustedVaultKeyAndVersion(
                      /*key=*/GetConstantTrustedVaultKey(), /*version=*/0))));

  EXPECT_CALL(callback, Run(TrustedVaultRequestStatus::kSuccess));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest());
}

}  // namespace
}  // namespace syncer
