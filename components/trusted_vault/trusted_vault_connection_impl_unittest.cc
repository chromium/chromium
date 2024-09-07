// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_connection_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/fake_trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

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

trusted_vault_pb::SecurityDomain MakeSecurityDomainWithDegradedRecoverability(
    SecurityDomainId security_domain_id,
    bool recoverability_degraded) {
  trusted_vault_pb::SecurityDomain security_domain;
  security_domain.set_name(GetSecurityDomainPath(security_domain_id));
  security_domain.mutable_security_domain_details()
      ->mutable_sync_details()
      ->set_degraded_recoverability(recoverability_degraded);
  return security_domain;
}

trusted_vault_pb::JoinSecurityDomainsResponse MakeJoinSecurityDomainsResponse(
    SecurityDomainId security_domain_id,
    int current_epoch) {
  trusted_vault_pb::JoinSecurityDomainsResponse response;
  trusted_vault_pb::SecurityDomain* security_domain =
      response.mutable_security_domain();
  security_domain->set_name(GetSecurityDomainPath(security_domain_id));
  security_domain->set_current_epoch(current_epoch);
  return response;
}

constexpr char kTestSerializedWrappedPIN[] = "wrapped PIN";
// A valid hex encoded securebox public key.
constexpr char kTestMemberPublicKey[] =
    "045C8A5DF0CE8205E62621449AD2C5F7320EE6D1BC41A4BBC423289ADAF9954996F9E893C6"
    "13C99EDFC5B28BD119C80AD034DE52819963F3056E0F230264D62828";
constexpr int kTestKeyVersion = 100;
constexpr int kTestGPMExpirySeconds = 1000000;
constexpr int kTestLSKFExpirySeconds = 1000001;
constexpr char kTestMemberProof[] = "member_proof";
constexpr char kTestWrappedKey[] = "wrapped_key";

enum class Member {
  kPhysical,
  kOtherSecurityDomain,
  kUsableVirtual,
  kUnusableVirtual,
  kGooglePasswordManagerPIN,
  kICloudKeychain,
  kInvalidICloudKeychain,
};

trusted_vault_pb::ListSecurityDomainMembersResponse MakeSecurityDomainMembers(
    SecurityDomainId security_domain_id,
    const std::vector<Member>& members,
    std::optional<std::string> next_page_token) {
  trusted_vault_pb::ListSecurityDomainMembersResponse response;

  for (auto member_type : members) {
    trusted_vault_pb::SecurityDomainMember* member =
        response.add_security_domain_members();
    member->set_name("name");
    std::string public_key_bytes;
    base::HexStringToString(kTestMemberPublicKey, &public_key_bytes);
    member->set_public_key(std::move(public_key_bytes));
    member->add_memberships()->set_security_domain("other security domain");
    auto* membership = member->add_memberships();
    auto* key = membership->add_keys();
    if (member_type != Member::kOtherSecurityDomain) {
      membership->set_security_domain(
          GetSecurityDomainPath(security_domain_id));
      key->set_epoch(kTestKeyVersion * 2);
    } else {
      key->set_epoch(kTestKeyVersion);
    }
    key->set_member_proof(kTestMemberProof);
    key->set_wrapped_key(kTestWrappedKey);

    switch (member_type) {
      case Member::kPhysical:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_PHYSICAL_DEVICE);
        break;
      case Member::kOtherSecurityDomain:
        member->set_member_type(
            trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_UNSPECIFIED);
        break;
      case Member::kUnusableVirtual:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_LOCKSCREEN_KNOWLEDGE_FACTOR);
        break;
      case Member::kUsableVirtual: {
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_LOCKSCREEN_KNOWLEDGE_FACTOR);
        member->mutable_member_metadata()->set_usable_for_retrieval(true);
        auto* metadata =
            member->mutable_member_metadata()->mutable_lskf_metadata();
        metadata->mutable_expiration_time()->set_seconds(
            kTestLSKFExpirySeconds);
        break;
      }
      case Member::kICloudKeychain:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_ICLOUD_KEYCHAIN);
        break;
      case Member::kInvalidICloudKeychain:
        member->set_public_key("invalid-public-key");
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_ICLOUD_KEYCHAIN);
        break;
      case Member::kGooglePasswordManagerPIN:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN);
        member->mutable_member_metadata()->set_usable_for_retrieval(true);
        auto* gpm_metadata =
            member->mutable_member_metadata()
                ->mutable_google_password_manager_pin_metadata();
        gpm_metadata->mutable_expiration_time()->set_seconds(
            kTestGPMExpirySeconds);
        gpm_metadata->set_encrypted_pin_hash(kTestSerializedWrappedPIN);
        break;
    }
  }
  if (next_page_token) {
    response.set_next_page_token(*next_page_token);
  }
  return response;
}

signin::AccessTokenInfo MakeAccessTokenInfo(const std::string& access_token) {
  return signin::AccessTokenInfo(
      access_token,
      /*expiration_time_param=*/base::Time::Now() + base::Hours(1),
      /*id_token=*/std::string());
}

// TODO(crbug.com/40143544): revisit this tests suite and determine what
// actually should be tested on the Connection level and what should be done on
// lower levels (DownloadKeysResponseHandler and
// RegisterAuthenticationFactorRequest).
class TrustedVaultConnectionImplTest
    : public testing::TestWithParam<SecurityDomainId> {
 public:
  TrustedVaultConnectionImplTest()
      : connection_(
            security_domain(),
            kTestURL,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)
                ->Clone(),
            std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
                MakeAccessTokenInfo(kAccessToken))) {}

  ~TrustedVaultConnectionImplTest() override = default;

  SecurityDomainId security_domain() { return GetParam(); }

  std::string security_domain_name_uma() {
    return GetSecurityDomainNameForUma(security_domain());
  }

  TrustedVaultConnectionImpl* connection() { return &connection_; }

  // Allows overloading of FakeTrustedVaultAccessTokenFetcher behavior, doesn't
  // overwrite connection().
  std::unique_ptr<TrustedVaultConnectionImpl>
  CreateConnectionWithAccessTokenError(
      TrustedVaultAccessTokenFetcher::FetchingError fetching_error) {
    return std::make_unique<TrustedVaultConnectionImpl>(
        security_domain(), kTestURL,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_)
            ->Clone(),
        std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
            base::unexpected(fetching_error)));
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingHTTPRequest() {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.GetPendingRequest(/*index=*/0);
  }

  bool RespondToJoinSecurityDomainsRequest(
      net::HttpStatusCode response_http_code,
      const std::string& response_content) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullJoinSecurityDomainsURLForTesting(kTestURL, security_domain())
            .spec(),
        response_content, response_http_code);
  }

  bool RespondToJoinSecurityDomainsRequestWithNetworkError() {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullJoinSecurityDomainsURLForTesting(kTestURL, security_domain()),
        network::URLLoaderCompletionStatus(net::ERR_FAILED),
        /*response_head=*/network::mojom::URLResponseHead::New(),
        /*content=*/std::string());
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

  bool RespondToGetSecurityDomainRequest(net::HttpStatusCode response_http_code,
                                         const std::string& response_body) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullGetSecurityDomainURLForTesting(kTestURL, security_domain())
            .spec(),
        response_body, response_http_code);
  }

  bool RespondToDownloadAuthenticationFactorsRegistrationStateRequest(
      const std::optional<std::string>& next_page_token,
      net::HttpStatusCode response_http_code,
      const std::string& response_body) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetGetSecurityDomainMembersURLForTesting(next_page_token, kTestURL)
            .spec(),
        response_body, response_http_code);
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  const std::vector<std::vector<uint8_t>> kTrustedVaultKeys = {{1, 2},
                                                               {1, 2, 3, 4}};
  const GURL kTestURL = GURL("https://test.com/test");

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;

  base::HistogramTester histogram_tester_;

  TrustedVaultConnectionImpl connection_;
};

INSTANTIATE_TEST_SUITE_P(ForSecurityDomain,
                         TrustedVaultConnectionImplTest,
                         testing::ValuesIn(kAllSecurityDomainIdValues.begin(),
                                           kAllSecurityDomainIdValues.end()));

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestWithoutKeys) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterLocalDeviceWithoutKeys(
          /*account_info=*/CoreAccountInfo(), key_pair->public_key(),
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url, Eq(GetFullJoinSecurityDomainsURLForTesting(
                                        kTestURL, security_domain())));

  trusted_vault_pb::JoinSecurityDomainsRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain().name(),
              Eq(GetSecurityDomainPath(security_domain())));

  std::string public_key_string;
  AssignBytesToProtoString(key_pair->public_key().ExportToBytes(),
                           &public_key_string);

  std::string encoded_public_key;
  base::Base64UrlEncode(public_key_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  const trusted_vault_pb::SecurityDomainMember& member =
      deserialized_body.security_domain_member();
  EXPECT_THAT(member.name(),
              Eq(kSecurityDomainMemberNamePrefix + encoded_public_key));
  EXPECT_THAT(member.public_key(), Eq(public_key_string));
  EXPECT_THAT(
      member.member_type(),
      Eq(trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE));

  // Constant key with |epoch| set to kUnknownConstantKeyVersion must be sent.
  ASSERT_THAT(deserialized_body.shared_member_key(), SizeIs(1));
  const trusted_vault_pb::SharedMemberKey& shared_key =
      deserialized_body.shared_member_key(0);
  EXPECT_THAT(shared_key.epoch(), Eq(0));

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(
                  key_pair->private_key(),
                  /*wrapped_key=*/ProtoStringToBytes(shared_key.wrapped_key())),
              Eq(GetConstantTrustedVaultKey()));
  EXPECT_TRUE(VerifyMemberProof(key_pair->public_key(),
                                GetConstantTrustedVaultKey(),
                                ProtoStringToBytes(shared_key.member_proof())));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestWithKeys) {
  const std::vector<std::vector<uint8_t>> kTrustedVaultKeys = {{1, 2},
                                                               {1, 2, 3, 4}};
  const int kLastKeyVersion = 1234;
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys, kLastKeyVersion),
          key_pair->public_key(), LocalPhysicalDevice(),
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url, Eq(GetFullJoinSecurityDomainsURLForTesting(
                                        kTestURL, security_domain())));

  trusted_vault_pb::JoinSecurityDomainsRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain().name(),
              Eq(GetSecurityDomainPath(security_domain())));

  std::string public_key_string;
  AssignBytesToProtoString(key_pair->public_key().ExportToBytes(),
                           &public_key_string);

  std::string encoded_public_key;
  base::Base64UrlEncode(public_key_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  const trusted_vault_pb::SecurityDomainMember& member =
      deserialized_body.security_domain_member();
  EXPECT_THAT(member.name(),
              Eq(kSecurityDomainMemberNamePrefix + encoded_public_key));
  EXPECT_THAT(member.public_key(), Eq(public_key_string));
  EXPECT_THAT(
      member.member_type(),
      Eq(trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE));

  ASSERT_THAT(deserialized_body.shared_member_key(),
              SizeIs(kTrustedVaultKeys.size()));
  const trusted_vault_pb::SharedMemberKey& shared_key_1 =
      deserialized_body.shared_member_key(0);
  EXPECT_THAT(shared_key_1.epoch(), Eq(kLastKeyVersion - 1));

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(key_pair->private_key(),
                                            /*wrapped_key=*/ProtoStringToBytes(
                                                shared_key_1.wrapped_key())),
              Eq(kTrustedVaultKeys[0]));
  EXPECT_TRUE(
      VerifyMemberProof(key_pair->public_key(), kTrustedVaultKeys[0],
                        ProtoStringToBytes(shared_key_1.member_proof())));

  const trusted_vault_pb::SharedMemberKey& shared_key_2 =
      deserialized_body.shared_member_key(1);
  EXPECT_THAT(shared_key_2.epoch(), Eq(kLastKeyVersion));

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(key_pair->private_key(),
                                            /*wrapped_key=*/ProtoStringToBytes(
                                                shared_key_2.wrapped_key())),
              Eq(kTrustedVaultKeys[1]));
  EXPECT_TRUE(
      VerifyMemberProof(key_pair->public_key(), kTrustedVaultKeys[1],
                        ProtoStringToBytes(shared_key_2.member_proof())));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestWithPrecomputedKeys) {
  constexpr int kVersion = 123;
  const std::vector<uint8_t> kWrappedKey{1, 2, 3};
  const std::vector<uint8_t> kProof{4, 5, 6};
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          MemberKeys(kVersion, kWrappedKey, kProof), key_pair->public_key(),
          LocalPhysicalDevice(),
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url, Eq(GetFullJoinSecurityDomainsURLForTesting(
                                        kTestURL, security_domain())));

  trusted_vault_pb::JoinSecurityDomainsRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  ASSERT_THAT(deserialized_body.shared_member_key(), SizeIs(1));
  const trusted_vault_pb::SharedMemberKey& shared_key_1 =
      deserialized_body.shared_member_key(0);
  EXPECT_THAT(shared_key_1.epoch(), Eq(kVersion));
  EXPECT_THAT(shared_key_1.wrapped_key(),
              Eq(std::string(kWrappedKey.begin(), kWrappedKey.end())));
  EXPECT_THAT(shared_key_1.member_proof(),
              Eq(std::string(kProof.begin(), kProof.end())));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestTypeHint) {
  const int kTypeHint = 19;
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1234),
          key_pair->public_key(),
          UnspecifiedAuthenticationFactorType(kTypeHint),
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url, Eq(GetFullJoinSecurityDomainsURLForTesting(
                                        kTestURL, security_domain())));

  trusted_vault_pb::JoinSecurityDomainsRequest deserialized_body;
  ASSERT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.member_type_hint(), Eq(kTypeHint));
  EXPECT_THAT(
      deserialized_body.security_domain_member().member_type(),
      Eq(trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_UNSPECIFIED));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestGpmPinMetadata) {
  const std::string old_public_key = "old_public_key";
  const std::string metadata = "metadata";
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1234),
          key_pair->public_key(),
          GpmPinMetadata(old_public_key, metadata, /*expiry=*/base::Time()),
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url, Eq(GetFullJoinSecurityDomainsURLForTesting(
                                        kTestURL, security_domain())));

  trusted_vault_pb::JoinSecurityDomainsRequest deserialized_body;
  ASSERT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.current_public_key_to_replace(),
              Eq(old_public_key));
  EXPECT_THAT(deserialized_body.security_domain_member().member_type(),
              Eq(trusted_vault_pb::SecurityDomainMember::
                     MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN));
  EXPECT_THAT(deserialized_body.security_domain_member()
                  .member_metadata()
                  .google_password_manager_pin_metadata()
                  .encrypted_pin_hash(),
              Eq(metadata));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestForLskf) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1234),
          key_pair->public_key(), LockScreenKnowledgeFactor(),
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url, Eq(GetFullJoinSecurityDomainsURLForTesting(
                                        kTestURL, security_domain())));

  trusted_vault_pb::JoinSecurityDomainsRequest deserialized_body;
  ASSERT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain_member().member_type(),
              Eq(trusted_vault_pb::SecurityDomainMember::
                     MEMBER_TYPE_LOCKSCREEN_KNOWLEDGE_FACTOR));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleSuccessfulJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRegistrationStatus::kSuccess),
                            /*key_version=*/Eq(1)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_OK, MakeJoinSecurityDomainsResponse(security_domain(),
                                                    /*current_epoch=*/1)
                        .SerializeAsString()));

  histogram_tester().ExpectUniqueSample(
      "TrustedVault.SecurityDomainServiceURLFetchResponse",
      /*sample=*/200,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      "TrustedVault.SecurityDomainServiceURLFetchResponse.RegisterDevice",
      /*sample=*/200,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      base::StrCat(
          {"TrustedVault.SecurityDomainServiceURLFetchResponse.RegisterDevice.",
           security_domain_name_uma()}),
      /*sample=*/200,
      /*expected_bucket_count=*/1);
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldPopulateConstantKeyAndVersionWhenJoinSecurityDomain) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterLocalDeviceWithoutKeys(
          /*account_info=*/CoreAccountInfo(), key_pair->public_key(),
          callback.Get());
  ASSERT_THAT(request, NotNull());

  const int kServerConstantKeyVersion = 100;
  EXPECT_CALL(callback, Run(Eq(TrustedVaultRegistrationStatus::kSuccess),
                            Eq(kServerConstantKeyVersion)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_OK, MakeJoinSecurityDomainsResponse(
                        security_domain(),
                        /*current_epoch=*/kServerConstantKeyVersion)
                        .SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleJoinSecurityDomainsResponseWithConflictError) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterLocalDeviceWithoutKeys(
          /*account_info=*/CoreAccountInfo(), key_pair->public_key(),
          callback.Get());
  ASSERT_THAT(request, NotNull());

  const int kServerConstantKeyVersion = 100;
  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kAlreadyRegistered),
                  Eq(kServerConstantKeyVersion)));

  trusted_vault_pb::JoinSecurityDomainsErrorDetail error_detail;
  *error_detail.mutable_already_exists_response() =
      MakeJoinSecurityDomainsResponse(
          security_domain(),
          /*current_epoch=*/kServerConstantKeyVersion);

  trusted_vault_pb::RPCStatus response;
  trusted_vault_pb::Proto3Any* status_detail = response.add_details();
  status_detail->set_type_url(kJoinSecurityDomainsErrorDetailTypeURL);
  status_detail->set_value(error_detail.SerializeAsString());

  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_CONFLICT, response.SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleJoinSecurityDomainsRequestWithEmptyResponse) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/0),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kOtherError), Eq(0)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_OK,
                                          /*response_content=*/std::string()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleJoinSecurityDomainsRequestWithCorruptedResponse) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/0),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kOtherError), Eq(0)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_OK,
      /*response_content=*/"corrupted_proto"));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithHttpError) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kOtherError), Eq(0)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                                          /*response_content=*/std::string()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithNetworkError) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // Advance time to bypass retry logic.
  task_environment().FastForwardBy(
      TrustedVaultConnectionImpl::kMaxJoinSecurityDomainRetryDuration);
  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kNetworkError), Eq(0)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequestWithNetworkError());
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithNotFoundStatus) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // In particular, HTTP_NOT_FOUND indicates that security domain was removed.
  EXPECT_CALL(
      callback,
      Run(Eq(TrustedVaultRegistrationStatus::kLocalDataObsolete), Eq(0)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_NOT_FOUND,
                                          /*response_content=*/std::string()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithBadRequestStatus) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // In particular, HTTP_BAD_REQUEST indicates that
  // |last_trusted_vault_key_and_version| is not actually the last on the server
  // side.
  EXPECT_CALL(
      callback,
      Run(Eq(TrustedVaultRegistrationStatus::kLocalDataObsolete), Eq(0)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_BAD_REQUEST,
                                          /*response_content=*/std::string()));
}

TEST_P(
    TrustedVaultConnectionImplTest,
    ShouldHandleAccessTokenFetchingFailureWhenRegisteringAuthenticationFactor) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessTokenError(
          TrustedVaultAccessTokenFetcher::FetchingError::kPersistentAuthError);

  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  // |callback| is called immediately after RegisterAuthenticationFactor(),
  // because there is no access token.
  EXPECT_CALL(
      callback,
      Run(Eq(TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError),
          Eq(0)));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_P(TrustedVaultConnectionImplTest, ShouldCancelJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1),
          key_pair->public_key(), LocalPhysicalDevice(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToJoinSecurityDomainsRequest(net::HTTP_OK,
                                      /*response_content=*/std::string());
}

TEST_P(TrustedVaultConnectionImplTest, ShouldSendGetSecurityDomainsRequest) {
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(/*key=*/std::vector<uint8_t>(),
                                    /*version=*/1),
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

// TODO(crbug.com/40143544): add coverage for at least one successful case
// (need to share some helper functions with
// download_keys_response_handler_unittest.cc).
TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleFailedGetSecurityDomainMemberRequest) {
  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(/*key=*/std::vector<uint8_t>(),
                                    /*version=*/1),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultDownloadKeysStatus::kOtherError), _, _));
  EXPECT_TRUE(
      RespondToGetSecurityDomainMemberRequest(net::HTTP_INTERNAL_SERVER_ERROR));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleAccessTokenFetchingFailureWhenDownloadingKeys) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessTokenError(
          TrustedVaultAccessTokenFetcher::FetchingError::kPersistentAuthError);

  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  // |callback| is called immediately after DownloadNewKeys(), because there is
  // no access token.
  EXPECT_CALL(
      callback,
      Run(Eq(TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure), _,
          _));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(
              /*key=*/std::vector<uint8_t>(),
              /*version=*/1),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldCancelGetSecurityDomainMemberRequest) {
  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(
              /*key=*/std::vector<uint8_t>(),
              /*version=*/1),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToGetSecurityDomainMemberRequest(net::HTTP_OK);
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendGetSecurityDomainRequestWhenRetrievingRecoverability) {
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultConnection::IsRecoverabilityDegradedCallback());
  ASSERT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_http_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_http_request, NotNull());

  const network::ResourceRequest& resource_request =
      pending_http_request->request;
  EXPECT_THAT(resource_request.method, Eq("GET"));
  EXPECT_THAT(resource_request.url, Eq(GetFullGetSecurityDomainURLForTesting(
                                        kTestURL, security_domain())));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleValidResponseWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kNotDegraded));
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          security_domain(),
          /*recoverability_degraded=*/false)
          .SerializeAsString()));
  testing::Mock::VerifyAndClearExpectations(&callback);

  request = connection()->DownloadIsRecoverabilityDegraded(
      /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kDegraded));
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          security_domain(),
          /*recoverability_degraded=*/true)
          .SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleFailedRequestWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kError));
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_INTERNAL_SERVER_ERROR,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          security_domain(),
          /*recoverability_degraded=*/false)
          .SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldHandleCorruptedResponseWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kError));
  // Respond with invalid proto.
  EXPECT_TRUE(
      RespondToGetSecurityDomainRequest(net::HTTP_OK,
                                        /*response_body=*/"invalid proto"));

  request = connection()->DownloadIsRecoverabilityDegraded(
      /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kError));
  // Respond with empty proto.
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/trusted_vault_pb::SecurityDomain()
          .SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       ShouldCancelRequestWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          security_domain(),
          /*recoverability_degraded=*/false)
          .SerializeAsString());
}

MATCHER_P(HasRecoveryState,
          state,
          "DownloadAuthenticationFactorsRegistrationStateResult::State") {
  return arg.state == state;
}

MATCHER_P2(
    HasGpmPinMetadata,
    public_key,
    wrapped_pin,
    "DownloadAuthenticationFactorsRegistrationStateResult::GpmPinMetadata") {
  if (!arg.gpm_pin_metadata) {
    return false;
  }
  return testing::ExplainMatchResult(*arg.gpm_pin_metadata,
                                     GpmPinMetadata(public_key, wrapped_pin));
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadAuthenticationFactorsRegistrationState_Basic) {
  base::MockCallback<TrustedVaultConnection::
                         DownloadAuthenticationFactorsRegistrationStateCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadAuthenticationFactorsRegistrationState(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(HasRecoveryState(
                  DownloadAuthenticationFactorsRegistrationStateResult::State::
                      kRecoverable)));

  ASSERT_TRUE(RespondToDownloadAuthenticationFactorsRegistrationStateRequest(
      /*next_page_token=*/std::nullopt, net::HTTP_OK,
      /*response_body=*/
      MakeSecurityDomainMembers(
          security_domain(),
          {Member::kPhysical, Member::kOtherSecurityDomain,
           Member::kUsableVirtual},
          /*next_page_token=*/std::nullopt)
          .SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadAuthenticationFactorsRegistrationState_Cases) {
  using State = DownloadAuthenticationFactorsRegistrationStateResult::State;
  std::string member_public_key_bytes;
  base::HexStringToString(kTestMemberPublicKey, &member_public_key_bytes);
  const GpmPinMetadata gpm_pin_metadata(
      std::move(member_public_key_bytes), kTestSerializedWrappedPIN,
      /*expiry=*/base::Time::FromTimeT(kTestGPMExpirySeconds));
  const base::Time lskf_expiry = base::Time::FromTimeT(kTestLSKFExpirySeconds);
  const struct TestCase {
    // responses contains the set of security domain members included in each
    // page of results from the "server".
    std::vector<std::vector<Member>> responses;
    State expected_result;
    std::optional<int> expected_key_version;
    std::optional<GpmPinMetadata> expected_gpm_pin_metadata;
    std::vector<base::Time> expected_lskf_expiries;
    std::optional<std::string> expected_icloud_key;
  } kTestCases[] = {
      {
          {{}},
          State::kEmpty,
          /*expected_key_version=*/std::nullopt,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{}, {}},
          State::kEmpty,
          /*expected_key_version=*/std::nullopt,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kOtherSecurityDomain}, {Member::kOtherSecurityDomain}},
          State::kEmpty,
          /*expected_key_version=*/std::nullopt,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kPhysical}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kPhysical, Member::kUsableVirtual}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry},
      },
      {
          {{Member::kPhysical, Member::kUnusableVirtual}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kPhysical}, {}, {Member::kUsableVirtual}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry},
      },
      {
          {{Member::kUsableVirtual}, {}, {Member::kPhysical}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry},
      },
      {
          {{Member::kPhysical}, {}, {Member::kUnusableVirtual}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kPhysical}, {}, {Member::kOtherSecurityDomain}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kGooglePasswordManagerPIN}, {Member::kOtherSecurityDomain}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/gpm_pin_metadata,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kGooglePasswordManagerPIN},
           {Member::kGooglePasswordManagerPIN}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/gpm_pin_metadata,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{Member::kICloudKeychain}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
          /*expected_icloud_keys=*/{kTestMemberPublicKey},
      },
      {
          {{Member::kInvalidICloudKeychain}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
          /*expected_icloud_keys=*/{},
      },
      {
          {{Member::kUsableVirtual, Member::kUsableVirtual}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry, lskf_expiry},
          /*expected_icloud_keys=*/{},
      },
  };

  int test_case = 0;
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test_case);
    test_case++;

    std::optional<DownloadAuthenticationFactorsRegistrationStateResult> result;
    auto callback = base::BindLambdaForTesting(
        [&result](
            DownloadAuthenticationFactorsRegistrationStateResult in_result) {
          result.emplace(std::move(in_result));
        });

    std::unique_ptr<TrustedVaultConnection::Request> request =
        connection()->DownloadAuthenticationFactorsRegistrationState(
            /*account_info=*/CoreAccountInfo(), std::move(callback));
    ASSERT_THAT(request, NotNull());

    std::optional<std::string> prev_next_page_token;
    size_t num_pages_downloaded = 0;
    for (size_t i = 0; i < test.responses.size(); i++) {
      if (result.has_value()) {
        // The process stopped early. (This is valid if enough members have been
        // seen to determine the result.)
        break;
      }

      std::optional<std::string> next_page_token;
      if (i < test.responses.size() - 1) {
        next_page_token = base::NumberToString(i);
      }
      ASSERT_TRUE(
          RespondToDownloadAuthenticationFactorsRegistrationStateRequest(
              prev_next_page_token, net::HTTP_OK,
              /*response_body=*/
              MakeSecurityDomainMembers(security_domain(), test.responses[i],
                                        next_page_token)
                  .SerializeAsString()));
      num_pages_downloaded++;
      prev_next_page_token = std::move(next_page_token);
    }

    EXPECT_EQ(num_pages_downloaded, test.responses.size());
    EXPECT_EQ(result->state, test.expected_result);
    EXPECT_EQ(result->gpm_pin_metadata, test.expected_gpm_pin_metadata);
    EXPECT_EQ(result->lskf_expiries, test.expected_lskf_expiries);
    EXPECT_EQ(result->icloud_keys.size(), test.expected_icloud_key ? 1u : 0u);
    if (test.expected_icloud_key) {
      EXPECT_EQ(base::HexEncode(
                    result->icloud_keys.at(0).public_key->ExportToBytes()),
                test.expected_icloud_key);
      EXPECT_EQ(result->icloud_keys.at(0).member_keys.size(), 1u);
      EXPECT_EQ(result->icloud_keys.at(0).member_keys.at(0).proof,
                ProtoStringToBytes(kTestMemberProof));
      EXPECT_EQ(result->icloud_keys.at(0).member_keys.at(0).wrapped_key,
                ProtoStringToBytes(kTestWrappedKey));
      EXPECT_EQ(result->icloud_keys.at(0).member_keys.at(0).version,
                kTestKeyVersion * 2);
    }
  }
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadAuthenticationFactorsRegistrationState_Error) {
  base::MockCallback<TrustedVaultConnection::
                         DownloadAuthenticationFactorsRegistrationStateCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadAuthenticationFactorsRegistrationState(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(HasRecoveryState(
                  DownloadAuthenticationFactorsRegistrationStateResult::State::
                      kError)));

  ASSERT_TRUE(RespondToDownloadAuthenticationFactorsRegistrationStateRequest(
      /*next_page_token=*/std::nullopt, net::HTTP_INTERNAL_SERVER_ERROR,
      /*response_body=*/""));
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadAuthenticationFactorsRegistrationState_InvalidResponse) {
  base::MockCallback<TrustedVaultConnection::
                         DownloadAuthenticationFactorsRegistrationStateCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadAuthenticationFactorsRegistrationState(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(HasRecoveryState(
                  DownloadAuthenticationFactorsRegistrationStateResult::State::
                      kError)));

  ASSERT_TRUE(RespondToDownloadAuthenticationFactorsRegistrationStateRequest(
      /*next_page_token=*/std::nullopt, net::HTTP_OK,
      /*response_body=*/"not a valid protobuf"));
}

}  // namespace

}  // namespace trusted_vault
