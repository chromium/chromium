// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_request_factory.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;

constexpr char kTestEmail[] = "user@gmail.com";
constexpr char kUsername[] = "USERNAME@gmail.com";
constexpr char16_t kUsername16[] = u"USERNAME@gmail.com";
constexpr char kPassword[] = "password123";
constexpr char16_t kPassword16[] = u"password123";
constexpr char kExampleCom[] = "https://example.com";
constexpr char kApiKey[] = "api_key";

const int64_t kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();

struct TestLeakDetectionRequest : public LeakDetectionRequestInterface {
  ~TestLeakDetectionRequest() override = default;
  // LeakDetectionRequestInterface:
  void LookupSingleLeak(network::mojom::URLLoaderFactory* url_loader_factory,
                        const std::optional<std::string>& access_token,
                        const std::optional<std::string>& api_key,
                        LookupSingleLeakPayload payload,
                        LookupSingleLeakCallback callback) override {
    encrypted_payload_ = std::move(payload.encrypted_payload);
    callback_ = std::move(callback);
  }

  std::string encrypted_payload_;
  LookupSingleLeakCallback callback_;
};

// Helper struct for making a fake network request.
struct PayloadAndCallback {
  std::string payload;
  LeakDetectionRequestInterface::LookupSingleLeakCallback callback;
};

// The test parameter controls whether the leak detection is done for a
// signed-in user.
class LeakDetectionCheckImplTest : public testing::TestWithParam<bool> {
 public:
  ~LeakDetectionCheckImplTest() override = default;

  base::test::TaskEnvironment& task_env() { return task_env_; }
  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  MockLeakDetectionDelegateInterface& delegate() { return delegate_; }
  MockLeakDetectionRequestFactory* request_factory() {
    return request_factory_;
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  LeakDetectionCheckImpl* leak_check() { return leak_check_.get(); }

  // Initializes |leak_check_| with the appropriate identity environment based
  // on the provided |user_signed_in| parameter.
  void InitializeLeakCheck(bool user_signed_in) {
    std::optional<std::string> api_key = kApiKey;
    if (user_signed_in) {
      api_key = std::nullopt;
      identity_env().MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSignin);
    }
    leak_check_ = std::make_unique<LeakDetectionCheckImpl>(
        &delegate_, identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(), api_key);
    auto mock_request_factory =
        std::make_unique<StrictMock<MockLeakDetectionRequestFactory>>();
    request_factory_ = mock_request_factory.get();
    leak_check_->set_network_factory(std::move(mock_request_factory));
  }

  // Brings |leak_check_| to the state right after the network request.
  // The check is done for |kUsername|/|kPassword| credential on |kExampleCom|.
  // Returns |encrypted_payload| and |callback| arguments of LookupSingleLeak().
  PayloadAndCallback ImitateNetworkRequest(bool user_signed_in);

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  StrictMock<MockLeakDetectionDelegateInterface> delegate_;
  base::HistogramTester histogram_tester_;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  std::unique_ptr<LeakDetectionCheckImpl> leak_check_;
  raw_ptr<MockLeakDetectionRequestFactory> request_factory_ = nullptr;
};

PayloadAndCallback LeakDetectionCheckImplTest::ImitateNetworkRequest(
    bool user_signed_in) {
  InitializeLeakCheck(user_signed_in);
  leak_check()->Start(LeakDetectionInitiator::kSignInCheck, GURL(kExampleCom),
                      kUsername16, kPassword16);

  auto network_request = std::make_unique<TestLeakDetectionRequest>();
  TestLeakDetectionRequest* raw_request = network_request.get();
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));

  // Return the access token. This is skipped for signed-out users.
  if (user_signed_in) {
    static constexpr char access_token[] = "access_token";
    identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        access_token, base::Time::Max());
    histogram_tester().ExpectUniqueSample(
        "PasswordManager.LeakDetection.ObtainAccessTokenTime", kMockElapsedTime,
        1);
  }

  // Crypto stuff is done here.
  task_env().RunUntilIdle();

  return {std::move(raw_request->encrypted_payload_),
          std::move(raw_request->callback_)};
}

}  // namespace

TEST_P(LeakDetectionCheckImplTest, Create) {
  InitializeLeakCheck(/*user_signed_in=*/GetParam());
  EXPECT_CALL(delegate(), OnLeakDetectionDone).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);
  // Destroying |leak_check_| doesn't trigger anything.
}

TEST_P(LeakDetectionCheckImplTest, HasAccountForRequest_SignedIn) {
  InitializeLeakCheck(/*user_signed_in=*/true);
  EXPECT_TRUE(LeakDetectionCheckImpl::HasAccountForRequest(
      identity_env().identity_manager()));
}

TEST_P(LeakDetectionCheckImplTest, HasAccountForRequest_Syncing) {
  identity_env().SetPrimaryAccount(kTestEmail, signin::ConsentLevel::kSync);
  EXPECT_TRUE(LeakDetectionCheckImpl::HasAccountForRequest(
      identity_env().identity_manager()));
}

TEST_P(LeakDetectionCheckImplTest, GetAccessTokenBeforeEncryption) {
  // For signed-out users the access token is not requested.
  if (GetParam() == false) {
    return;
  }

  InitializeLeakCheck(/*user_signed_in=*/GetParam());
  const std::string access_token = "access_token";

  leak_check()->Start(LeakDetectionInitiator::kSignInCheck, GURL(kExampleCom),
                      kUsername16, kPassword16);
  // Return the access token before the crypto stuff is done.
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token, base::Time::Max());

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.ObtainAccessTokenTime", kMockElapsedTime,
      1);

  auto network_request = std::make_unique<MockLeakDetectionRequest>();
  EXPECT_CALL(
      *network_request,
      LookupSingleLeak(
          _, Optional(access_token), /*api_key=*/Eq(std::nullopt),
          AllOf(Field(&LookupSingleLeakPayload::username_hash_prefix,
                      ElementsAre(0xBD, 0x74, 0xA9, 0x00)),
                Field(&LookupSingleLeakPayload::encrypted_payload, Ne(""))),
          _));
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));
  // Crypto stuff is done here.
  task_env().RunUntilIdle();
}

TEST_P(LeakDetectionCheckImplTest, GetAccessTokenAfterEncryption) {
  // For signed-out users the access token is not requested.
  if (GetParam() == false) {
    return;
  }

  InitializeLeakCheck(/*user_signed_in=*/GetParam());

  leak_check()->Start(LeakDetectionInitiator::kSignInCheck, GURL(kExampleCom),
                      kUsername16, kPassword16);
  // crypto stuff is done here.
  task_env().RunUntilIdle();

  const std::string access_token = "access_token";
  auto network_request = std::make_unique<MockLeakDetectionRequest>();
  EXPECT_CALL(
      *network_request,
      LookupSingleLeak(
          _, Optional(access_token), /*api_key=*/Eq(std::nullopt),
          AllOf(Field(&LookupSingleLeakPayload::username_hash_prefix,
                      ElementsAre(0xBD, 0x74, 0xA9, 0x00)),
                Field(&LookupSingleLeakPayload::encrypted_payload, Ne(""))),
          _));
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));

  // Return the access token.
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token, base::Time::Max());

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.ObtainAccessTokenTime", kMockElapsedTime,
      1);
}

TEST_P(LeakDetectionCheckImplTest, GetAccessTokenFailure) {
  // For signed-out users the access token is not requested.
  if (GetParam() == false) {
    return;
  }

  InitializeLeakCheck(/*user_signed_in=*/GetParam());
  leak_check()->Start(LeakDetectionInitiator::kSignInCheck, GURL(kExampleCom),
                      kUsername16, kPassword16);

  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kTokenRequestFailure));
  identity_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.ObtainAccessTokenTime", kMockElapsedTime,
      1);
}

TEST_P(LeakDetectionCheckImplTest, PassesAPIKeys) {
  // For signed-in users API key is not passed.
  if (GetParam()) {
    return;
  }

  InitializeLeakCheck(/*user_signed_in=*/GetParam());
  leak_check()->Start(LeakDetectionInitiator::kSignInCheck, GURL(kExampleCom),
                      kUsername16, kPassword16);

  auto network_request = std::make_unique<MockLeakDetectionRequest>();
  EXPECT_CALL(
      *network_request,
      LookupSingleLeak(
          _, /*access_token=*/Eq(std::nullopt), Optional(Eq(kApiKey)),
          AllOf(Field(&LookupSingleLeakPayload::username_hash_prefix,
                      ElementsAre(0xBD, 0x74, 0xA9, 0x00)),
                Field(&LookupSingleLeakPayload::encrypted_payload, Ne(""))),
          _));
  EXPECT_CALL(*request_factory(), CreateNetworkRequest)
      .WillOnce(Return(ByMove(std::move(network_request))));

  // Crypto stuff is done here.
  task_env().RunUntilIdle();
}

// Perform the whole cycle of a leak check. The server returns data that
// can't be decrypted.
TEST_P(LeakDetectionCheckImplTest, ParseResponse_DecryptionError) {
  PayloadAndCallback payload_and_callback =
      ImitateNetworkRequest(/*user_signed_in=*/GetParam());
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  // Append trash bytes to force a decryption error.
  response->reencrypted_lookup_hash =
      *CipherReEncrypt(payload_and_callback.payload, &key_server) +
      "trash_bytes";
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(*CipherEncryptWithKey(
          *ScryptHashUsernameAndPassword("another_username", kPassword),
          key_server)));

  EXPECT_CALL(delegate(),
              OnLeakDetectionDone(false, GURL(kExampleCom), Eq(kUsername16),
                                  Eq(kPassword16)));
  std::move(payload_and_callback.callback)
      .Run(std::move(response), std::nullopt);
  task_env().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.AnalyzeSingleLeakResponseResult",
      AnalyzeResponseResult::kDecryptionError, 1);
  // Expect one sample for each of the response time histograms.
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.ReceiveSingleLeakResponseTime",
      kMockElapsedTime, 1);
}

// Perform the whole cycle of a leak check. The server returns data signalling
// that the password wasn't leaked.
TEST_P(LeakDetectionCheckImplTest, ParseResponse_NoLeak) {
  PayloadAndCallback payload_and_callback =
      ImitateNetworkRequest(/*user_signed_in=*/GetParam());
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  response->reencrypted_lookup_hash =
      *CipherReEncrypt(payload_and_callback.payload, &key_server);
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(*CipherEncryptWithKey(
          *ScryptHashUsernameAndPassword("another_username", kPassword),
          key_server)));

  EXPECT_CALL(delegate(),
              OnLeakDetectionDone(false, GURL(kExampleCom), Eq(kUsername16),
                                  Eq(kPassword16)));
  std::move(payload_and_callback.callback)
      .Run(std::move(response), std::nullopt);
  task_env().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.AnalyzeSingleLeakResponseResult",
      AnalyzeResponseResult::kNotLeaked, 1);
  // Expect one sample for each of the response time histograms.
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.ReceiveSingleLeakResponseTime",
      kMockElapsedTime, 1);
}

// Perform the whole cycle of a leak check. The server returns data signalling
// that the password was leaked.
TEST_P(LeakDetectionCheckImplTest, ParseResponse_Leak) {
  PayloadAndCallback payload_and_callback =
      ImitateNetworkRequest(/*user_signed_in=*/GetParam());
  ASSERT_TRUE(!payload_and_callback.payload.empty());

  // |canonicalized_username| is passed to ScryptHashUsernameAndPassword() to
  // make sure the canonicalization logic works correctly. Assert that
  // CanonicalizeUsername() was not a no-op.
  std::string canonicalized_username = CanonicalizeUsername(kUsername);
  ASSERT_NE(kUsername, canonicalized_username);

  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_server;
  response->reencrypted_lookup_hash =
      *CipherReEncrypt(payload_and_callback.payload, &key_server);
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(*CipherEncryptWithKey(
          *ScryptHashUsernameAndPassword(canonicalized_username, kPassword),
          key_server)));

  EXPECT_CALL(delegate(),
              OnLeakDetectionDone(true, GURL(kExampleCom), Eq(kUsername16),
                                  Eq(kPassword16)));
  std::move(payload_and_callback.callback)
      .Run(std::move(response), std::nullopt);
  task_env().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.AnalyzeSingleLeakResponseResult",
      AnalyzeResponseResult::kLeaked, 1);
  // Expect one sample for each of the response time histograms.
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.ReceiveSingleLeakResponseTime",
      kMockElapsedTime, 1);
}

INSTANTIATE_TEST_SUITE_P(, LeakDetectionCheckImplTest, testing::Bool());

}  // namespace password_manager
