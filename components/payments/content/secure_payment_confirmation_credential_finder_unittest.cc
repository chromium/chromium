// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_credential_finder.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/payments/content/mock_web_payments_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webauthn/core/browser/mock_internal_authenticator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

const std::vector<std::vector<uint8_t>> kInputCredentialIds = {
    {0x01, 0x02, 0x03},
    {0x04, 0x05, 0x06}};
const std::vector<std::vector<uint8_t>> kAvailableCredentialIds = {
    {0x04, 0x05, 0x06}};
const std::string kRelyingPartyId = "rp.example";

class SecurePaymentConfirmationCredentialFinderTest : public testing::Test {
 protected:
  SecurePaymentConfirmationCredentialFinderTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)),
        mock_authenticator_(
            std::make_unique<webauthn::MockInternalAuthenticator>(
                web_contents_)),
        mock_service_(base::MakeRefCounted<MockWebPaymentsWebDataService>()) {}

  // Required for test environment setup.
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  // Mocks of the underlying authenticator and user database service.
  std::unique_ptr<webauthn::MockInternalAuthenticator> mock_authenticator_;
  scoped_refptr<MockWebPaymentsWebDataService> mock_service_;

  // The class under test.
  SecurePaymentConfirmationCredentialFinder credential_finder_;
};

// Tests for the user profile database fetching path.
class SecurePaymentConfirmationCredentialFinderUserDatabaseTest
    : public SecurePaymentConfirmationCredentialFinderTest {
 protected:
  SecurePaymentConfirmationCredentialFinderUserDatabaseTest() {
    feature_list_.InitAndDisableFeature(
        features::kSecurePaymentConfirmationUseCredentialStoreAPIs);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the credential finder uses the database service and that returned
// credentials are propagated to the callback.
TEST_F(SecurePaymentConfirmationCredentialFinderUserDatabaseTest,
       ReturnsCredentialsOnSuccess) {
  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle = 1234;
  EXPECT_CALL(*mock_service_,
              GetSecurePaymentConfirmationCredentials(
                  Eq(kInputCredentialIds), kRelyingPartyId, /*callback=*/_))
      .WillOnce(MoveArgAndReturn<2>(&web_data_service_callback, handle));

  std::optional<
      std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
      actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](
          std::optional<
              std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
              result) { actual_credentials = std::move(result); });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      /*authenticator=*/nullptr, mock_service_, std::move(callback));

  // Simulate the web data service returning the credentials.
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>> credentials;
  credentials.emplace_back(
      std::make_unique<SecurePaymentConfirmationCredential>(
          kAvailableCredentialIds[0], kRelyingPartyId,
          /*user_id=*/std::vector<uint8_t>()));
  auto result = std::make_unique<WDResult<
      std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>>(
      SECURE_PAYMENT_CONFIRMATION, std::move(credentials));
  std::move(web_data_service_callback).Run(handle, std::move(result));

  // The credential finder should have received the credentials and sent them
  // back to the callback.
  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);
  EXPECT_EQ((*actual_credentials)[0]->relying_party_id, kRelyingPartyId);
}

// Tests that if the web data service returns a result that is not for SPC, we
// return a std::nullopt.
TEST_F(SecurePaymentConfirmationCredentialFinderUserDatabaseTest,
       ReturnsNulloptOnFailure) {
  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle = 1234;
  EXPECT_CALL(*mock_service_,
              GetSecurePaymentConfirmationCredentials(
                  Eq(kInputCredentialIds), kRelyingPartyId, /*callback=*/_))
      .WillOnce(MoveArgAndReturn<2>(&web_data_service_callback, handle));

  base::MockCallback<SecurePaymentConfirmationCredentialFinder::
                         SecurePaymentConfirmationCredentialFinderCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(Eq(std::nullopt)));

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      /*authenticator=*/nullptr, mock_service_, mock_callback.Get());

  // Simulate the web data service returning a non-SPC result; this should
  // not generally happen, but the finder should handle it and return
  // std::nullopt.
  ASSERT_FALSE(web_data_service_callback.is_null());
  auto result = std::make_unique<WDResult<int>>(AUTOFILL_PROFILES_RESULT, 0);
  std::move(web_data_service_callback).Run(handle, std::move(result));
}

// Tests for the credential store APIs path.
class SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest
    : public SecurePaymentConfirmationCredentialFinderTest {
 protected:
  SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest() {
    ON_CALL(*mock_authenticator_, IsGetMatchingCredentialIdsSupported())
        .WillByDefault(Return(true));
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kSecurePaymentConfirmationUseCredentialStoreAPIs};
};

// Tests that the credential finder uses the credential store API path, and that
// returned credentials are propagated to the callback.
TEST_F(SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest,
       ReturnsCredentialsOnSuccess) {
  std::optional<
      std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
      actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](
          std::optional<
              std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
              result) { actual_credentials = std::move(result); });

  EXPECT_CALL(
      *mock_authenticator_,
      GetMatchingCredentialIds(kRelyingPartyId, Eq(kInputCredentialIds), _, _))
      .WillOnce(RunOnceCallback<3>(kAvailableCredentialIds));

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // The credential finder should have received the credentials, converted them,
  // and sent them back to the callback.
  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);
  EXPECT_EQ((*actual_credentials)[0]->relying_party_id, kRelyingPartyId);
}

// Test that if the credential store APIs are not available, the finder
// returns a std::nullopt to the callback.
TEST_F(SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest,
       ReturnsNulloptWhenNotSupported) {
  EXPECT_CALL(*mock_authenticator_, IsGetMatchingCredentialIdsSupported())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_authenticator_, GetMatchingCredentialIds).Times(0);

  base::MockCallback<SecurePaymentConfirmationCredentialFinder::
                         SecurePaymentConfirmationCredentialFinderCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(Eq(std::nullopt)));

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, mock_callback.Get());
}

TEST_F(
    SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest,
    CorrectlyCalculatesThirdPartyPaymentRequirement_OriginDifferentFromRpId) {
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));

  // Because the RP ID is 'rp.example', and our origin is
  // 'https://site.example', this is a third-party payment authentication.
  EXPECT_CALL(*mock_authenticator_,
              GetMatchingCredentialIds(
                  _, _, /*require_third_party_payment_bit=*/true, _));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, base::DoNothing());
}

TEST_F(SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest,
       CorrectlyCalculatesThirdPartyPaymentRequirement_OriginSameAsRpId) {
  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));

  // Because the RP ID is 'rp.example', and our origin is 'https://rp.example'
  // too, this is a first-party payment authentication.
  EXPECT_CALL(*mock_authenticator_,
              GetMatchingCredentialIds(
                  _, _, /*require_third_party_payment_bit=*/false, _));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, base::DoNothing());
}

TEST_F(SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest,
       CorrectlyCalculatesThirdPartyPaymentRequirement_OriginSameDomainAsRpId) {
  url::Origin caller_origin =
      url::Origin::Create(GURL("https://subdomain.rp.example"));

  // Because the RP ID is 'rp.example', and our origin is
  // 'https://subdomain.rp.example' (a registrable-domain-match), this is a
  // first-party payment authentication.
  EXPECT_CALL(*mock_authenticator_,
              GetMatchingCredentialIds(
                  _, _, /*require_third_party_payment_bit=*/false, _));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, base::DoNothing());
}

}  // namespace
}  // namespace payments
