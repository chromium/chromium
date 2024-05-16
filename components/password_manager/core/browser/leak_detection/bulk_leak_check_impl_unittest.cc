// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_request_factory.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "crypto/sha2.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Return;

constexpr char kAccessToken[] = "access_token";
constexpr char kTestEmail[] = "user@gmail.com";
constexpr char16_t kTestEmail16[] = u"user@gmail.com";
constexpr char kTestPassword[] = "password123";
constexpr char16_t kTestPassword16[] = u"password123";
constexpr char kUniqueString[] = "unique";

MATCHER_P(CredentialIs, credential, "") {
  return arg.username() == credential.get().username() &&
         arg.password() == credential.get().password();
}

struct CustomData : LeakCheckCredential::Data {
  explicit CustomData(std::string s) : data(std::move(s)) {}

  std::string data;
};

MATCHER_P(CustomDataIs, string, "") {
  return static_cast<CustomData*>(arg.GetUserData(kUniqueString))->data ==
         string;
}

struct TestLeakDetectionRequest : LeakDetectionRequestInterface {
  // LeakDetectionRequestInterface:
  void LookupSingleLeak(network::mojom::URLLoaderFactory* url_loader_factory,
                        const std::optional<std::string>& access_token,
                        const std::optional<std::string>& api_key,
                        LookupSingleLeakPayload payload,
                        LookupSingleLeakCallback callback) override {
    EXPECT_EQ(payload.initiator,
              LeakDetectionInitiator::kBulkSyncedPasswordsCheck);
    encrypted_payload = std::move(payload.encrypted_payload);
    lookup_callback = std::move(callback);
  }

  std::string encrypted_payload;
  LookupSingleLeakCallback lookup_callback;
};

// Helper struct for making a fake network request.
struct PayloadAndCallback {
  std::string payload;
  LeakDetectionRequestInterface::LookupSingleLeakCallback callback;
};

LeakCheckCredential TestCredential(std::u16string_view username) {
  return LeakCheckCredential(std::u16string(username), kTestPassword16);
}

class BulkLeakCheckTest : public testing::Test {
 public:
  BulkLeakCheckTest()
      : bulk_check_(
            &delegate_,
            identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    auto mock_request_factory = std::make_unique<
        ::testing::StrictMock<MockLeakDetectionRequestFactory>>();
    request_factory_ = mock_request_factory.get();
    bulk_check_.set_network_factory(std::move(mock_request_factory));
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  PayloadAndCallback ImitateNetworkRequest(LeakCheckCredential credential);

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }
  MockBulkLeakCheckDelegateInterface& delegate() { return delegate_; }
  MockLeakDetectionRequestFactory* request_factory() {
    return request_factory_;
  }
  BulkLeakCheckImpl& bulk_check() { return bulk_check_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  ::testing::StrictMock<MockBulkLeakCheckDelegateInterface> delegate_;
  BulkLeakCheckImpl bulk_check_;
  raw_ptr<MockLeakDetectionRequestFactory> request_factory_ = nullptr;
};

PayloadAndCallback BulkLeakCheckTest::ImitateNetworkRequest(
    LeakCheckCredential credential) {
  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(std::move(credential));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));

  auto network_request = std::make_unique<TestLeakDetectionRequest>();
  TestLeakDetectionRequest* raw_request = network_request.get();
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));

  // Return the access token.
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());

  return {std::move(raw_request->encrypted_payload),
          std::move(raw_request->lookup_callback)};
}

TEST_F(BulkLeakCheckTest, Create) {
  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);
  // Destroying |leak_check_| doesn't trigger anything.
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAndDestroyImmediately) {
  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential(u"user1"));
  credentials.push_back(TestCredential(u"user2"));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAndDestroyAfterPayload) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential(u"user1"));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));
  RunUntilIdle();
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAccessTokenAuthError) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kTokenRequestFailure));

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential(u"user1"));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceError("error"));
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAccessTokenNetError) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kNetworkError));

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential(u"user1"));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAccessTokenSignedOut) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kNotSignIn));

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential(u"user1"));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
}

TEST_F(BulkLeakCheckTest, CheckCredentialsAccessDoesNetworkRequest) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential(u"USERNAME@gmail.com"));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));

  auto network_request = std::make_unique<MockLeakDetectionRequest>();
  EXPECT_CALL(*network_request,
              LookupSingleLeak(
                  _, Optional(Eq(kAccessToken)), /*api_key=*/Eq(std::nullopt),
                  AllOf(Field(&LookupSingleLeakPayload::username_hash_prefix,
                              ElementsAre(0xBD, 0x74, 0xA9, 0x00)),
                        Field(&LookupSingleLeakPayload::encrypted_payload,
                              testing::Ne(""))),
                  _));
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());
}

TEST_F(BulkLeakCheckTest, CheckCredentialsMultipleNetworkRequests) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  EXPECT_EQ(0u, bulk_check().GetPendingChecksCount());
  std::vector<LeakCheckCredential> credentials;
  credentials.push_back(TestCredential(u"user1"));
  credentials.push_back(TestCredential(u"user2"));
  bulk_check().CheckCredentials(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      std::move(credentials));
  EXPECT_EQ(2u, bulk_check().GetPendingChecksCount());
  RunUntilIdle();
  EXPECT_EQ(2u, bulk_check().GetPendingChecksCount());

  auto network_request1 = std::make_unique<MockLeakDetectionRequest>();
  auto network_request2 = std::make_unique<MockLeakDetectionRequest>();
  EXPECT_CALL(*network_request1,
              LookupSingleLeak(_, Optional(Eq(kAccessToken)),
                               /*api_key=*/Eq(std::nullopt), _, _));
  EXPECT_CALL(*network_request2,
              LookupSingleLeak(_, Optional(Eq(kAccessToken)),
                               /*api_key=*/Eq(std::nullopt), _, _));
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request1))))
      .WillOnce(Return(ByMove(std::move(network_request2))));
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccessToken, base::Time::Max());
  EXPECT_EQ(2u, bulk_check().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckTest, CheckCredentialsDecryptionError) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  PayloadAndCallback payload_and_callback =
      ImitateNetworkRequest(TestCredential(kTestEmail16));
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  // Append trash bytes to force a decryption error.
  response->reencrypted_lookup_hash =
      *CipherReEncrypt(payload_and_callback.payload, &key_server) +
      "trash_bytes";
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(*CipherEncryptWithKey(
          *ScryptHashUsernameAndPassword("another_username", kTestPassword),
          key_server)));

  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kHashingFailure));
  std::move(payload_and_callback.callback)
      .Run(std::move(response), std::nullopt);
  RunUntilIdle();
  EXPECT_EQ(0u, bulk_check().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckTest, CheckCredentialsNotLeaked) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  LeakCheckCredential leaked_credential = TestCredential(kTestEmail16);
  leaked_credential.SetUserData(kUniqueString,
                                std::make_unique<CustomData>("custom"));
  PayloadAndCallback payload_and_callback =
      ImitateNetworkRequest(std::move(leaked_credential));
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  response->reencrypted_lookup_hash =
      *CipherReEncrypt(payload_and_callback.payload, &key_server);
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(*CipherEncryptWithKey(
          *ScryptHashUsernameAndPassword("another_username", kTestPassword),
          key_server)));

  EXPECT_EQ(1u, bulk_check().GetPendingChecksCount());
  leaked_credential = TestCredential(kTestEmail16);
  EXPECT_CALL(delegate(), OnFinishedCredential(
                              AllOf(CredentialIs(std::cref(leaked_credential)),
                                    CustomDataIs("custom")),
                              IsLeaked(false)));
  std::move(payload_and_callback.callback)
      .Run(std::move(response), std::nullopt);
  RunUntilIdle();
  EXPECT_EQ(0u, bulk_check().GetPendingChecksCount());
}

TEST_F(BulkLeakCheckTest, CheckCredentialsLeaked) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  LeakCheckCredential leaked_credential = TestCredential(u"abc");
  leaked_credential.SetUserData(kUniqueString,
                                std::make_unique<CustomData>("custom"));
  PayloadAndCallback payload_and_callback =
      ImitateNetworkRequest(std::move(leaked_credential));
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  response->reencrypted_lookup_hash =
      *CipherReEncrypt(payload_and_callback.payload, &key_server);
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(*CipherEncryptWithKey(
          *ScryptHashUsernameAndPassword("abc", kTestPassword), key_server)));

  EXPECT_EQ(1u, bulk_check().GetPendingChecksCount());
  leaked_credential = TestCredential(u"abc");
  EXPECT_CALL(delegate(), OnFinishedCredential(
                              AllOf(CredentialIs(std::cref(leaked_credential)),
                                    CustomDataIs("custom")),
                              IsLeaked(true)));
  std::move(payload_and_callback.callback)
      .Run(std::move(response), std::nullopt);
  RunUntilIdle();
  EXPECT_EQ(0u, bulk_check().GetPendingChecksCount());
}

}  // namespace
}  // namespace password_manager
