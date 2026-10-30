// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_connection_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
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
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace trusted_vault {

namespace {

using testing::_;
using testing::ElementsAreArray;
using testing::Eq;
using testing::IsEmpty;
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

std::vector<uint8_t> GenerateRandomPublicKey() {
  return SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
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
constexpr int kTestKeyVersion = 100;
constexpr int kTestGPMExpirySeconds = 1000000;
constexpr int kTestLSKFExpirySeconds = 1000001;

struct Member {
  enum class MemberType {
    kPhysical,
    kUsableVirtual,
    kUnusableVirtual,
    kGooglePasswordManagerPIN,
    kUnusableGooglePasswordManagerPIN,
    kICloudKeychain,
    kInvalidICloudKeychain,
  };

  MemberType member_type;
  std::vector<std::variant<SecurityDomainId, std::string>> security_domains;
  std::vector<uint8_t> public_key;
  TrustedVaultKeyAndVersion key_and_version;
};

trusted_vault_pb::ListSecurityDomainMembersResponse MakeSecurityDomainMembers(
    const std::vector<Member>& members,
    std::optional<std::string_view> next_page_token) {
  trusted_vault_pb::ListSecurityDomainMembersResponse response;

  for (const Member& member_info : members) {
    std::unique_ptr<SecureBoxPublicKey> public_key =
        SecureBoxPublicKey::CreateByImport(member_info.public_key);
    CHECK(public_key);

    std::string encoded_public_key;
    base::Base64UrlEncode(member_info.public_key,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_public_key);

    trusted_vault_pb::SecurityDomainMember* member =
        response.add_security_domain_members();
    member->set_name(kSecurityDomainMemberNamePrefix + encoded_public_key);
    AssignBytesToProtoString(member_info.public_key,
                             member->mutable_public_key());

    for (const auto& security_domain : member_info.security_domains) {
      auto* membership = member->add_memberships();
      membership->set_security_domain(
          std::visit(absl::Overload{
                         [](SecurityDomainId domain) {
                           return GetSecurityDomainPath(domain);
                         },
                         [](const std::string& domain) { return domain; },
                     },
                     security_domain));

      auto* key = membership->add_keys();
      key->set_epoch(member_info.key_and_version.version);
      AssignBytesToProtoString(
          ComputeMemberProof(*public_key, member_info.key_and_version.key),
          key->mutable_member_proof());
      AssignBytesToProtoString(
          ComputeTrustedVaultWrappedKey(*public_key,
                                        member_info.key_and_version.key),
          key->mutable_wrapped_key());
    }

    switch (member_info.member_type) {
      case Member::MemberType::kPhysical:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_PHYSICAL_DEVICE);
        break;
      case Member::MemberType::kUnusableVirtual:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_LOCKSCREEN_KNOWLEDGE_FACTOR);
        break;
      case Member::MemberType::kUsableVirtual: {
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_LOCKSCREEN_KNOWLEDGE_FACTOR);
        member->mutable_member_metadata()->set_usable_for_retrieval(true);
        auto* metadata =
            member->mutable_member_metadata()->mutable_lskf_metadata();
        metadata->mutable_expiration_time()->set_seconds(
            kTestLSKFExpirySeconds);
        break;
      }
      case Member::MemberType::kICloudKeychain:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_ICLOUD_KEYCHAIN);
        break;
      case Member::MemberType::kInvalidICloudKeychain:
        member->set_public_key("invalid-public-key");
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_ICLOUD_KEYCHAIN);
        break;
      case Member::MemberType::kGooglePasswordManagerPIN: {
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
      case Member::MemberType::kUnusableGooglePasswordManagerPIN:
        member->set_member_type(trusted_vault_pb::SecurityDomainMember::
                                    MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN);
        member->mutable_member_metadata()->set_usable_for_retrieval(false);
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
  TrustedVaultConnectionImplTest() = default;
  ~TrustedVaultConnectionImplTest() override = default;

  SecurityDomainId security_domain() { return GetParam(); }

  std::string security_domain_name_uma() {
    return GetSecurityDomainNameForUma(security_domain());
  }

  TrustedVaultConnectionImpl* connection() {
    if (!connection_) {
      connection_ = std::make_unique<TrustedVaultConnectionImpl>(
          kTestURL,
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)
              ->Clone(),
          std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
              MakeAccessTokenInfo(kAccessToken)));
    }
    return connection_.get();
  }

  // Allows overloading of FakeTrustedVaultAccessTokenFetcher behavior, doesn't
  // overwrite connection().
  std::unique_ptr<TrustedVaultConnectionImpl>
  CreateConnectionWithAccessTokenError(
      TrustedVaultAccessTokenFetcher::FetchingError fetching_error) {
    return std::make_unique<TrustedVaultConnectionImpl>(
        kTestURL,
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

  bool RespondToListSecurityDomainMembersRequest(
      const std::set<SecurityDomainId>& security_domain_filter,
      const std::set<trusted_vault_pb::SecurityDomainMember_MemberType>&
          member_filter,
      bool force_strong_read,
      std::optional<std::string_view> next_page_token,
      net::HttpStatusCode response_http_code,
      const std::string& response_body) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();

    GURL request_url = GetFullListSecurityDomainMembersURLForTesting(
        kTestURL, security_domain_filter, member_filter, next_page_token,
        force_strong_read);

    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        request_url.spec(), response_body, response_http_code);
  }

  bool RespondToDownloadGaiaPasswordPublicKeyRequest(
      net::HttpStatusCode response_http_code,
      const std::string& response_content) {
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullDownloadGaiaPasswordPublicKeyURLForTesting(kTestURL).spec(),
        response_content, response_http_code);
  }

  bool RespondToDownloadGaiaPasswordPublicKeyRequestWithNetworkError() {
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullDownloadGaiaPasswordPublicKeyURLForTesting(kTestURL),
        network::URLLoaderCompletionStatus(net::ERR_FAILED),
        /*response_head=*/network::mojom::URLResponseHead::New(),
        /*content=*/std::string());
  }

  bool RespondToRotateSharedKeyRequest(net::HttpStatusCode response_http_code,
                                       const std::string& response_content) {
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullRotateSharedKeyURLForTesting(kTestURL, security_domain()).spec(),
        response_content, response_http_code);
  }

  bool RespondToRotateSharedKeyRequestWithNetworkError() {
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullRotateSharedKeyURLForTesting(kTestURL, security_domain()),
        network::URLLoaderCompletionStatus(net::ERR_FAILED),
        /*response_head=*/network::mojom::URLResponseHead::New(),
        /*content=*/std::string());
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  const std::vector<std::vector<uint8_t>> kTrustedVaultKeys = {{1, 2},
                                                               {1, 2, 3, 4}};
  const GURL kTestURL = GURL("https://test.com/test/");

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<TrustedVaultConnectionImpl> connection_;
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          key_pair->public_key(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          GetTrustedVaultKeysWithVersions(kTrustedVaultKeys,
                                          /*last_key_version=*/1234),
          key_pair->public_key(),
          GpmPinMetadata(old_public_key,
                         UsableRecoveryPinMetadata(metadata,
                                                   /*expiry=*/base::Time())),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          key_pair->public_key(), callback.Get());
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          key_pair->public_key(), callback.Get());
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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

TEST_P(TrustedVaultConnectionImplTest,
       ShouldSendGetSecurityDomainMemberRequest) {
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          callback.Get());
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
      /*account_info=*/CoreAccountInfo(), security_domain(), callback.Get());
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          callback.Get());
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kError));
  // Respond with invalid proto.
  EXPECT_TRUE(
      RespondToGetSecurityDomainRequest(net::HTTP_OK,
                                        /*response_body=*/"invalid proto"));

  request = connection()->DownloadIsRecoverabilityDegraded(
      /*account_info=*/CoreAccountInfo(), security_domain(), callback.Get());
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
          /*account_info=*/CoreAccountInfo(), security_domain(),
          callback.Get());
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
  return testing::ExplainMatchResult(
      *arg.gpm_pin_metadata,
      UsableRecoveryPinMetadata(public_key, wrapped_pin));
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadAuthenticationFactorsRegistrationState_Basic_ServerFiltering) {
  const TrustedVaultKeyAndVersion key_and_version(/*key=*/{1, 2, 3, 4},
                                                  /*version=*/kTestKeyVersion);

  base::MockCallback<TrustedVaultConnection::
                         DownloadAuthenticationFactorsRegistrationStateCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadAuthenticationFactorsRegistrationState(
          /*account_info=*/CoreAccountInfo(), security_domain(), callback.Get(),
          base::NullCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(HasRecoveryState(
                  DownloadAuthenticationFactorsRegistrationStateResult::State::
                      kRecoverable)));

  ASSERT_TRUE(RespondToListSecurityDomainMembersRequest(
      /*security_domain_filter=*/{security_domain()},
      /*member_filter=*/{}, /*force_strong_read=*/false,
      /*next_page_token=*/std::nullopt, net::HTTP_OK,
      /*response_body=*/
      MakeSecurityDomainMembers(
          {{
               .member_type = Member::MemberType::kPhysical,
               .security_domains = {security_domain()},
               .public_key = GenerateRandomPublicKey(),
               .key_and_version = key_and_version,
           },
           {
               .member_type = Member::MemberType::kPhysical,
               .security_domains = {"other_security_domain"},
               .public_key = GenerateRandomPublicKey(),
               .key_and_version = key_and_version,
           },
           {
               .member_type = Member::MemberType::kUsableVirtual,
               .security_domains = {"other_security_domain", security_domain()},
               .public_key = GenerateRandomPublicKey(),
               .key_and_version = key_and_version,
           }},
          /*next_page_token=*/std::nullopt)
          .SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadAuthenticationFactorsRegistrationState_RecoveryFactorFilter) {
  const TrustedVaultKeyAndVersion key_and_version(/*key=*/{1, 2, 3, 4},
                                                  /*version=*/kTestKeyVersion);

  base::MockCallback<TrustedVaultConnection::
                         DownloadAuthenticationFactorsRegistrationStateCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadAuthenticationFactorsRegistrationState(
          /*account_info=*/CoreAccountInfo(), security_domain(),
          {trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE},
          callback.Get(), base::NullCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(HasRecoveryState(
                  DownloadAuthenticationFactorsRegistrationStateResult::State::
                      kRecoverable)));

  ASSERT_TRUE(RespondToListSecurityDomainMembersRequest(
      /*security_domain_filter=*/{security_domain()},
      /*member_filter=*/
      {trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE},
      /*force_strong_read=*/false,
      /*next_page_token=*/std::nullopt, net::HTTP_OK,
      /*response_body=*/
      MakeSecurityDomainMembers(
          {{
               .member_type = Member::MemberType::kPhysical,
               .security_domains = {security_domain()},
               .public_key = GenerateRandomPublicKey(),
               .key_and_version = key_and_version,
           },
           {
               .member_type = Member::MemberType::kPhysical,
               .security_domains = {"other_security_domain"},
               .public_key = GenerateRandomPublicKey(),
               .key_and_version = key_and_version,
           },
           {
               .member_type = Member::MemberType::kUsableVirtual,
               .security_domains = {security_domain()},
               .public_key = GenerateRandomPublicKey(),
               .key_and_version = key_and_version,
           }},
          /*next_page_token=*/std::nullopt)
          .SerializeAsString()));
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadAuthenticationFactorsRegistrationState_Cases) {
  using State = DownloadAuthenticationFactorsRegistrationStateResult::State;
  const TrustedVaultKeyAndVersion key_and_version(/*key=*/{1, 2, 3, 4},
                                                  /*version=*/kTestKeyVersion);

  const auto make_gpm_pin_metadata = [](const std::vector<uint8_t>& public_key,
                                        bool usable) {
    std::string public_key_bytes;
    AssignBytesToProtoString(public_key, &public_key_bytes);
    return GpmPinMetadata(
        std::move(public_key_bytes),
        usable ? std::make_optional(UsableRecoveryPinMetadata(
                     kTestSerializedWrappedPIN,
                     /*expiry=*/base::Time::FromTimeT(kTestGPMExpirySeconds)))
               : std::nullopt);
  };

  const std::vector<uint8_t> gpm_pin_public_key_1 = GenerateRandomPublicKey();
  const GpmPinMetadata gpm_pin_metadata_1 =
      make_gpm_pin_metadata(gpm_pin_public_key_1, /*usable=*/true);
  const std::vector<uint8_t> gpm_pin_public_key_2 = GenerateRandomPublicKey();
  const GpmPinMetadata gpm_pin_metadata_2 =
      make_gpm_pin_metadata(gpm_pin_public_key_2, /*usable=*/true);
  const std::vector<uint8_t> unusable_gpm_pin_public_key =
      GenerateRandomPublicKey();
  const GpmPinMetadata unusable_gpm_pin_metadata =
      make_gpm_pin_metadata(unusable_gpm_pin_public_key, /*usable=*/false);
  const std::unique_ptr<SecureBoxKeyPair> icloud_key_pair =
      SecureBoxKeyPair::GenerateRandom();
  const base::Time lskf_expiry = base::Time::FromTimeT(kTestLSKFExpirySeconds);

  const auto make_member = [&](Member::MemberType member_type) {
    return Member{
        .member_type = member_type,
        .security_domains = {security_domain()},
        .public_key = GenerateRandomPublicKey(),
        .key_and_version = key_and_version,
    };
  };
  const auto make_other_domain_member = [&]() {
    return Member{
        .member_type = Member::MemberType::kPhysical,
        .security_domains = {"other_security_domain"},
        .public_key = GenerateRandomPublicKey(),
        .key_and_version =
            TrustedVaultKeyAndVersion(/*key=*/{5, 6, 7, 8},
                                      /*version=*/kTestKeyVersion * 2),
    };
  };
  const Member gpm_pin_member_1{
      .member_type = Member::MemberType::kGooglePasswordManagerPIN,
      .security_domains = {security_domain()},
      .public_key = gpm_pin_public_key_1,
      .key_and_version = key_and_version,
  };
  const Member gpm_pin_member_2{
      .member_type = Member::MemberType::kGooglePasswordManagerPIN,
      .security_domains = {security_domain()},
      .public_key = gpm_pin_public_key_2,
      .key_and_version = key_and_version,
  };
  const Member unusable_gpm_pin_member{
      .member_type = Member::MemberType::kUnusableGooglePasswordManagerPIN,
      .security_domains = {security_domain()},
      .public_key = unusable_gpm_pin_public_key,
      .key_and_version = key_and_version,
  };
  const Member icloud_keychain_member{
      .member_type = Member::MemberType::kICloudKeychain,
      .security_domains = {security_domain()},
      .public_key = icloud_key_pair->public_key().ExportToBytes(),
      .key_and_version = key_and_version,
  };

  const struct TestCase {
    // responses contains the set of security domain members included in each
    // page of results from the "server".
    std::vector<std::vector<Member>> responses;
    State expected_result;
    std::optional<int> expected_key_version;
    std::optional<GpmPinMetadata> expected_gpm_pin_metadata;
    std::vector<base::Time> expected_lskf_expiries;
    std::optional<std::vector<uint8_t>> expected_icloud_key;
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
          {{make_other_domain_member()}, {make_other_domain_member()}},
          State::kEmpty,
          /*expected_key_version=*/std::nullopt,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{make_member(Member::MemberType::kPhysical)}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{make_member(Member::MemberType::kPhysical),
            make_member(Member::MemberType::kUsableVirtual)}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry},
      },
      {
          {{make_member(Member::MemberType::kPhysical),
            make_member(Member::MemberType::kUnusableVirtual)}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{make_member(Member::MemberType::kPhysical)},
           {},
           {make_member(Member::MemberType::kUsableVirtual)}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry},
      },
      {
          {{make_member(Member::MemberType::kUsableVirtual)},
           {},
           {make_member(Member::MemberType::kPhysical)}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry},
      },
      {
          {{make_member(Member::MemberType::kPhysical)},
           {},
           {make_member(Member::MemberType::kUnusableVirtual)}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{make_member(Member::MemberType::kPhysical)},
           {},
           {make_other_domain_member()}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{gpm_pin_member_1}, {make_other_domain_member()}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/gpm_pin_metadata_1,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{gpm_pin_member_1}, {gpm_pin_member_2}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/gpm_pin_metadata_2,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{unusable_gpm_pin_member}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/unusable_gpm_pin_metadata,
          /*expected_lskf_expiries=*/{},
      },
      {
          {{icloud_keychain_member}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
          /*expected_icloud_key=*/
          icloud_key_pair->public_key().ExportToBytes(),
      },
      {
          {{make_member(Member::MemberType::kInvalidICloudKeychain)}},
          State::kIrrecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{},
          /*expected_icloud_key=*/std::nullopt,
      },
      {
          {{make_member(Member::MemberType::kUsableVirtual),
            make_member(Member::MemberType::kUsableVirtual)}},
          State::kRecoverable,
          /*expected_key_version=*/kTestKeyVersion,
          /*expected_gpm_pin_metadata=*/std::nullopt,
          /*expected_lskf_expiries=*/{lskf_expiry, lskf_expiry},
          /*expected_icloud_key=*/std::nullopt,
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
    testing::StrictMock<base::MockRepeatingClosure> keep_alive_callback;
    std::unique_ptr<TrustedVaultConnection::Request> request =
        connection()->DownloadAuthenticationFactorsRegistrationState(
            /*account_info=*/CoreAccountInfo(), security_domain(),
            std::move(callback), keep_alive_callback.Get());
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
        EXPECT_CALL(keep_alive_callback, Run());
        next_page_token = base::NumberToString(i);
      }
      ASSERT_TRUE(RespondToListSecurityDomainMembersRequest(
          /*security_domain_filter=*/{security_domain()},
          /*member_filter=*/{}, /*force_strong_read=*/false,
          prev_next_page_token, net::HTTP_OK,
          /*response_body=*/
          MakeSecurityDomainMembers(test.responses[i], next_page_token)
              .SerializeAsString()));
      EXPECT_TRUE(
          testing::Mock::VerifyAndClearExpectations(&keep_alive_callback));
      num_pages_downloaded++;
      prev_next_page_token = std::move(next_page_token);
    }

    EXPECT_EQ(num_pages_downloaded, test.responses.size());
    EXPECT_EQ(result->state, test.expected_result);
    EXPECT_EQ(result->key_version, test.expected_key_version);
    EXPECT_EQ(result->gpm_pin_metadata, test.expected_gpm_pin_metadata);
    EXPECT_EQ(result->lskf_expiries, test.expected_lskf_expiries);
    EXPECT_EQ(result->icloud_keys.size(), test.expected_icloud_key ? 1u : 0u);
    if (test.expected_icloud_key) {
      EXPECT_EQ(result->icloud_keys.at(0).public_key->ExportToBytes(),
                *test.expected_icloud_key);
      EXPECT_EQ(result->icloud_keys.at(0).member_keys.size(), 1u);
      EXPECT_THAT(
          result->icloud_keys.at(0).member_keys.at(0).proof,
          ElementsAreArray(ComputeMemberProof(
              *result->icloud_keys.at(0).public_key, key_and_version.key)));
      EXPECT_THAT(DecryptTrustedVaultWrappedKey(
                      icloud_key_pair->private_key(),
                      result->icloud_keys.at(0).member_keys.at(0).wrapped_key),
                  Eq(key_and_version.key));
      EXPECT_EQ(result->icloud_keys.at(0).member_keys.at(0).version,
                key_and_version.version);
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
          /*account_info=*/CoreAccountInfo(), security_domain(), callback.Get(),
          base::NullCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(HasRecoveryState(
                  DownloadAuthenticationFactorsRegistrationStateResult::State::
                      kError)));

  ASSERT_TRUE(RespondToListSecurityDomainMembersRequest(
      /*security_domain_filter=*/{security_domain()},
      /*member_filter=*/{}, /*force_strong_read=*/false,
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
          /*account_info=*/CoreAccountInfo(), security_domain(), callback.Get(),
          base::NullCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(HasRecoveryState(
                  DownloadAuthenticationFactorsRegistrationStateResult::State::
                      kError)));

  ASSERT_TRUE(RespondToListSecurityDomainMembersRequest(
      /*security_domain_filter=*/{security_domain()},
      /*member_filter=*/{}, /*force_strong_read=*/false,
      /*next_page_token=*/std::nullopt, net::HTTP_OK,
      /*response_body=*/"not a valid protobuf"));
}

TEST_P(TrustedVaultConnectionImplTest, DownloadGaiaPasswordPublicKeySuccess) {
  std::string expected_public_key = "test_public_key_bytes";
  trusted_vault_pb::GetCurrentGaiaPasswordEncryptionKeyDataResponse response;
  response.mutable_current_gaia_password_encryption_key_data()->set_public_key(
      expected_public_key);

  base::test::TestFuture<TrustedVaultDownloadPasswordPublicKeyStatus,
                         const std::vector<uint8_t>&>
      future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadGaiaPasswordPublicKey(CoreAccountInfo(),
                                                  future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(RespondToDownloadGaiaPasswordPublicKeyRequest(
      net::HTTP_OK, response.SerializeAsString()));

  EXPECT_EQ(future.Get<0>(),
            TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess);
  EXPECT_THAT(future.Get<1>(), ElementsAreArray(expected_public_key.begin(),
                                                expected_public_key.end()));
  histogram_tester().ExpectUniqueSample(
      /*name=*/"TrustedVault.AccessTokenFetchSuccess",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"TrustedVault.SecurityDomainServiceURLFetchResponse",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      /*name=*/
      "TrustedVault.SecurityDomainServiceURLFetchResponse.NoSecurityDomain",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      /*name=*/
      "TrustedVault.SecurityDomainServiceURLFetchResponse."
      "DownloadGaiaPasswordPublicKey",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectUniqueSample(
      /*name=*/
      "TrustedVault.SecurityDomainServiceURLFetchResponse."
      "DownloadGaiaPasswordPublicKey.NoSecurityDomain",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadGaiaPasswordPublicKeyNetworkError) {
  base::test::TestFuture<TrustedVaultDownloadPasswordPublicKeyStatus,
                         const std::vector<uint8_t>&>
      future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadGaiaPasswordPublicKey(CoreAccountInfo(),
                                                  future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(RespondToDownloadGaiaPasswordPublicKeyRequestWithNetworkError());

  EXPECT_EQ(future.Get<0>(),
            TrustedVaultDownloadPasswordPublicKeyStatus::kNetworkError);
  EXPECT_THAT(future.Get<1>(), IsEmpty());
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadGaiaPasswordPublicKeyParsingError) {
  base::test::TestFuture<TrustedVaultDownloadPasswordPublicKeyStatus,
                         const std::vector<uint8_t>&>
      future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadGaiaPasswordPublicKey(CoreAccountInfo(),
                                                  future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(RespondToDownloadGaiaPasswordPublicKeyRequest(
      net::HTTP_OK, "corrupted_proto_bytes"));

  EXPECT_EQ(future.Get<0>(),
            TrustedVaultDownloadPasswordPublicKeyStatus::kOtherError);
  EXPECT_THAT(future.Get<1>(), IsEmpty());
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadGaiaPasswordPublicKeyEmptyKeyError) {
  trusted_vault_pb::GetCurrentGaiaPasswordEncryptionKeyDataResponse response;
  // The public key inside the response is empty.

  base::test::TestFuture<TrustedVaultDownloadPasswordPublicKeyStatus,
                         const std::vector<uint8_t>&>
      future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadGaiaPasswordPublicKey(CoreAccountInfo(),
                                                  future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(RespondToDownloadGaiaPasswordPublicKeyRequest(
      net::HTTP_OK, response.SerializeAsString()));

  EXPECT_EQ(future.Get<0>(),
            TrustedVaultDownloadPasswordPublicKeyStatus::kOtherError);
  EXPECT_THAT(future.Get<1>(), IsEmpty());
}

TEST_P(TrustedVaultConnectionImplTest,
       DownloadGaiaPasswordPublicKeyAccessTokenFetchingFailure) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessTokenError(
          TrustedVaultAccessTokenFetcher::FetchingError::kPersistentAuthError);

  base::test::TestFuture<TrustedVaultDownloadPasswordPublicKeyStatus,
                         const std::vector<uint8_t>&>
      future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->DownloadGaiaPasswordPublicKey(CoreAccountInfo(),
                                                future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_EQ(future.Get<0>(), TrustedVaultDownloadPasswordPublicKeyStatus::
                                 kPersistentAccessTokenFetchError);
  EXPECT_THAT(future.Get<1>(), IsEmpty());
  histogram_tester().ExpectUniqueSample(
      /*name=*/"TrustedVault.AccessTokenFetchSuccess",
      /*sample=*/false,
      /*expected_bucket_count=*/1);

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_P(TrustedVaultConnectionImplTest, RotateSharedKeySuccess) {
  trusted_vault_pb::RotateSharedKeyRequest request_proto;
  request_proto.set_security_domain("test_security_domain");

  int new_epoch = 123;
  trusted_vault_pb::RotateSharedKeyResponse response_proto;
  response_proto.set_new_epoch(new_epoch);

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                    request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  // Verify request sent.
  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url, Eq(GetFullRotateSharedKeyURLForTesting(
                                        kTestURL, security_domain())));

  trusted_vault_pb::RotateSharedKeyRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain(), Eq("test_security_domain"));

  // Respond
  EXPECT_TRUE(RespondToRotateSharedKeyRequest(
      net::HTTP_OK, response_proto.SerializeAsString()));

  EXPECT_THAT(future.Get<0>(), Eq(TrustedVaultKeyRotationStatus::kSuccess));
  EXPECT_THAT(future.Get<1>(), Eq(new_epoch));
}

TEST_P(TrustedVaultConnectionImplTest, RotateSharedKeyNetworkError) {
  trusted_vault_pb::RotateSharedKeyRequest request_proto;

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                    request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  // Advance time to bypass retry logic.
  task_environment().FastForwardBy(
      TrustedVaultConnectionImpl::kMaxKeyRotationRetryDuration);

  EXPECT_TRUE(RespondToRotateSharedKeyRequestWithNetworkError());

  EXPECT_THAT(future.Get<0>(),
              Eq(TrustedVaultKeyRotationStatus::kNetworkError));
  EXPECT_THAT(future.Get<1>(), Eq(0));
}

TEST_P(TrustedVaultConnectionImplTest, RotateSharedKeyBadRequest) {
  trusted_vault_pb::RotateSharedKeyRequest request_proto;

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                    request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(RespondToRotateSharedKeyRequest(net::HTTP_BAD_REQUEST, ""));

  EXPECT_THAT(future.Get<0>(),
              Eq(TrustedVaultKeyRotationStatus::kMembershipMismatch));
  EXPECT_THAT(future.Get<1>(), Eq(0));
}

TEST_P(TrustedVaultConnectionImplTest, RotateSharedKeyNotFound) {
  trusted_vault_pb::RotateSharedKeyRequest request_proto;

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                    request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(RespondToRotateSharedKeyRequest(net::HTTP_NOT_FOUND, ""));

  EXPECT_THAT(future.Get<0>(),
              Eq(TrustedVaultKeyRotationStatus::kEmptySecurityDomain));
  EXPECT_THAT(future.Get<1>(), Eq(0));
}

TEST_P(TrustedVaultConnectionImplTest, RotateSharedKeyConflict) {
  trusted_vault_pb::RotateSharedKeyRequest request_proto;

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                    request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(RespondToRotateSharedKeyRequest(net::HTTP_CONFLICT, ""));

  EXPECT_THAT(future.Get<0>(),
              Eq(TrustedVaultKeyRotationStatus::kLocalDataObsolete));
  EXPECT_THAT(future.Get<1>(), Eq(0));
}

TEST_P(TrustedVaultConnectionImplTest, RotateSharedKeyInternalServerError) {
  trusted_vault_pb::RotateSharedKeyRequest request_proto;

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                    request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(
      RespondToRotateSharedKeyRequest(net::HTTP_INTERNAL_SERVER_ERROR, ""));

  EXPECT_THAT(future.Get<0>(), Eq(TrustedVaultKeyRotationStatus::kOtherError));
  EXPECT_THAT(future.Get<1>(), Eq(0));
}

TEST_P(TrustedVaultConnectionImplTest, RotateSharedKeyParsingError) {
  trusted_vault_pb::RotateSharedKeyRequest request_proto;

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                    request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  EXPECT_TRUE(
      RespondToRotateSharedKeyRequest(net::HTTP_OK, "corrupted_proto_bytes"));

  EXPECT_THAT(future.Get<0>(), Eq(TrustedVaultKeyRotationStatus::kOtherError));
  EXPECT_THAT(future.Get<1>(), Eq(0));
}

TEST_P(TrustedVaultConnectionImplTest,
       RotateSharedKeyAccessTokenFetchingFailure) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessTokenError(
          TrustedVaultAccessTokenFetcher::FetchingError::kPersistentAuthError);

  trusted_vault_pb::RotateSharedKeyRequest request_proto;

  base::test::TestFuture<TrustedVaultKeyRotationStatus, int> future;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->RotateSharedKey(CoreAccountInfo(), security_domain(),
                                  request_proto, future.GetCallback());
  ASSERT_THAT(request, NotNull());

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());

  EXPECT_THAT(
      future.Get<0>(),
      Eq(TrustedVaultKeyRotationStatus::kPersistentAccessTokenFetchError));
  EXPECT_THAT(future.Get<1>(), Eq(0));
}

}  // namespace

}  // namespace trusted_vault
