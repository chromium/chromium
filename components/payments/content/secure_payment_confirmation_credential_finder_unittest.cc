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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/payments/content/mock_web_payments_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
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

using MatchedCredentials =
    SecurePaymentConfirmationCredentialFinder::MatchedCredentials;

const std::vector<std::vector<uint8_t>> kInputCredentialIds = {
    {0x01, 0x02, 0x03},
    {0x04, 0x05, 0x06}};
const std::vector<std::vector<uint8_t>> kAvailableCredentialIds = {
    {0x04, 0x05, 0x06}};
const std::string kRelyingPartyId = "rp.example";

constexpr char kOSStoreUpliftHistogram[] =
    "PaymentRequest.SecurePaymentConfirmation.CredentialFinder.OSStoreUplift";
constexpr char kWebDatabaseHasOrphanedCredentialsHistogram[] =
    "PaymentRequest.SecurePaymentConfirmation.CredentialFinder."
    "WebDatabaseHasOrphanedCredentials";

class SecurePaymentConfirmationCredentialFinderTest : public testing::Test {
 protected:
  SecurePaymentConfirmationCredentialFinderTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)),
        mock_authenticator_(
            std::make_unique<webauthn::MockInternalAuthenticator>(
                web_contents_)),
        mock_service_(base::MakeRefCounted<MockWebPaymentsWebDataService>()) {}

  static std::unique_ptr<WDTypedResult> CreateWebDatabaseResult(
      const std::vector<std::vector<uint8_t>>& credential_ids,
      const std::string& relying_party_id = kRelyingPartyId) {
    std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
        credentials;
    for (const auto& id : credential_ids) {
      credentials.push_back(
          std::make_unique<SecurePaymentConfirmationCredential>(
              id, relying_party_id, /*user_id=*/std::vector<uint8_t>()));
    }
    return std::make_unique<WDResult<
        std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>>(
        SECURE_PAYMENT_CONFIRMATION, std::move(credentials));
  }

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
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSecurePaymentConfirmationCredentialDiscoveryMode,
        {{"mode", features::CredentialDiscoveryModeToString(
                      features::CredentialDiscoveryMode::kUserDatabaseOnly)}});
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

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      /*authenticator=*/nullptr, mock_service_, std::move(callback));

  // Simulate the web data service returning the credentials.
  ASSERT_FALSE(web_data_service_callback.is_null());
  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult(kAvailableCredentialIds));

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
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSecurePaymentConfirmationCredentialDiscoveryMode,
        {{"mode", features::CredentialDiscoveryModeToString(
                      features::CredentialDiscoveryMode::kOsOnly)}});
    ON_CALL(*mock_authenticator_, IsGetMatchingCredentialIdsSupported())
        .WillByDefault(Return(true));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the credential finder uses the credential store API path, and that
// returned credentials are propagated to the callback.
TEST_F(SecurePaymentConfirmationCredentialFinderCredentialStoreApisTest,
       ReturnsCredentialsOnSuccess) {
  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

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

// Tests for hybrid discovery mode.
class SecurePaymentConfirmationCredentialFinderHybridModeTest
    : public SecurePaymentConfirmationCredentialFinderTest {
 protected:
  SecurePaymentConfirmationCredentialFinderHybridModeTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSecurePaymentConfirmationCredentialDiscoveryMode,
        {{"mode", features::CredentialDiscoveryModeToString(
                      features::CredentialDiscoveryMode::kHybrid)}});
    ON_CALL(*mock_authenticator_, IsGetMatchingCredentialIdsSupported())
        .WillByDefault(Return(true));
  }

  void ExpectAuthenticatorQueryAndReturn(
      const std::vector<std::vector<uint8_t>>& returned_credential_ids) {
    EXPECT_CALL(
        *mock_authenticator_,
        GetMatchingCredentialIds(kRelyingPartyId, Eq(kInputCredentialIds),
                                 /*require_third_party_payment_bit=*/false, _))
        .WillOnce(RunOnceCallback<3>(returned_credential_ids));
  }

  WebDataServiceBase::Handle ExpectWebDataServiceQueryAndCaptureCallback(
      WebDataServiceRequestCallback* callback) {
    constexpr WebDataServiceBase::Handle kDefaultHandle = 1234;
    EXPECT_CALL(*mock_service_,
                GetSecurePaymentConfirmationCredentials(
                    Eq(kInputCredentialIds), kRelyingPartyId, /*callback=*/_))
        .WillOnce(MoveArgAndReturn<2>(callback, kDefaultHandle));
    return kDefaultHandle;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the case where both stores match the exact same credentials.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       BothStoresMatchSameCredentials) {
  base::HistogramTester histogram_tester;

  ExpectAuthenticatorQueryAndReturn(kAvailableCredentialIds);

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Run the WebDataService query to complete the barrier.
  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult(kAvailableCredentialIds));

  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);

  // No uplift or orphans here.
  histogram_tester.ExpectUniqueSample(kOSStoreUpliftHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kWebDatabaseHasOrphanedCredentialsHistogram, false, 1);
}

// Tests the case where only the OS store finds matching credentials and the web
// database returns none.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       OnlyOSStoreMatchesCredentials) {
  base::HistogramTester histogram_tester;

  ExpectAuthenticatorQueryAndReturn(kAvailableCredentialIds);

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Web database returns empty results.
  std::move(web_data_service_callback).Run(handle, CreateWebDatabaseResult({}));

  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);

  // This case has uplift, since we would have failed with just the user profile
  // database.
  histogram_tester.ExpectUniqueSample(kOSStoreUpliftHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(
      kWebDatabaseHasOrphanedCredentialsHistogram, false, 1);
}

// Tests the case where the OS store finds no matching credentials but the web
// database does (only orphaned credentials).
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       OnlyWebDatabaseMatchesCredentials) {
  base::HistogramTester histogram_tester;

  // OS store returns no matching credentials.
  ExpectAuthenticatorQueryAndReturn({});

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Web database returns a matching credential.
  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult(kAvailableCredentialIds));

  // The OS store is authoritative, so we should return an empty list.
  ASSERT_TRUE(actual_credentials.has_value());
  EXPECT_TRUE(actual_credentials->empty());

  // There are orphans here.
  histogram_tester.ExpectUniqueSample(kOSStoreUpliftHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kWebDatabaseHasOrphanedCredentialsHistogram, true, 1);
}

// Tests the case where both stores return matching credentials, but the sets
// are disjoint (user profile database returns only orphans).
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       BothStoresMatchDisjointCredentials) {
  base::HistogramTester histogram_tester;

  // OS store returns credential 0.
  ExpectAuthenticatorQueryAndReturn({kInputCredentialIds[0]});

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Web database returns credential 1.
  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult({kInputCredentialIds[1]}));

  // Credential 0 should be the returned one, as the OS store is authoritative.
  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id, kInputCredentialIds[0]);

  // We have both uplift (because credential 1 would have been a false-positive
  // match previously) and orphans (because credential 1 is an orphan).
  histogram_tester.ExpectUniqueSample(kOSStoreUpliftHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(
      kWebDatabaseHasOrphanedCredentialsHistogram, true, 1);
}

// Tests the case where the web database returns both a valid credential and an
// orphan.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       WebDatabaseMatchesOrphansAndValidCredentials) {
  base::HistogramTester histogram_tester;

  // OS store returns credential 0.
  ExpectAuthenticatorQueryAndReturn({kInputCredentialIds[0]});

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Web database returns credential 0 and credential 1.
  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult(kInputCredentialIds));

  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id, kInputCredentialIds[0]);

  // There is no uplift here as SPC would still have worked with credential 0
  // before, so adding the OS API doesn't help.
  //
  // Note: This ignores the nuance that we might have chosen the orphaned one,
  //       but we made that trade-off for metric simplicity.
  histogram_tester.ExpectUniqueSample(kOSStoreUpliftHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kWebDatabaseHasOrphanedCredentialsHistogram, true, 1);
}

// Tests the case where the OS store finds additional credentials alongside a
// valid DB credential.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       OSStoreMatchesAdditionalCredentials) {
  base::HistogramTester histogram_tester;

  // OS store returns credential 0 and credential 1.
  ExpectAuthenticatorQueryAndReturn(kInputCredentialIds);

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Web database returns credential 0.
  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult({kInputCredentialIds[0]}));

  // The OS store is authoritative so we should return both credentials.
  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 2u);

  // There is no uplift here; the OS store matched more credentials but we would
  // already have succeeded with the user profile database.
  histogram_tester.ExpectUniqueSample(kOSStoreUpliftHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kWebDatabaseHasOrphanedCredentialsHistogram, false, 1);
}

// Tests the case where neither store finds matching credentials.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       NeitherStoreMatchesCredentials) {
  base::HistogramTester histogram_tester;

  // OS store returns no matching credentials.
  ExpectAuthenticatorQueryAndReturn({});

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Web database returns empty results.
  std::move(web_data_service_callback).Run(handle, CreateWebDatabaseResult({}));

  ASSERT_TRUE(actual_credentials.has_value());
  EXPECT_TRUE(actual_credentials->empty());

  // Trivially no uplift or orphans.
  histogram_tester.ExpectUniqueSample(kOSStoreUpliftHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kWebDatabaseHasOrphanedCredentialsHistogram, false, 1);
}

// Tests that first-party SPC in hybrid mode falls back to the web data service
// when OS listing is not supported.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       FallsBackToWebDataWhenNotSupported) {
  ON_CALL(*mock_authenticator_, IsGetMatchingCredentialIdsSupported())
      .WillByDefault(Return(false));

  EXPECT_CALL(*mock_authenticator_, GetMatchingCredentialIds).Times(0);

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult(kAvailableCredentialIds));

  // Returns the result from the web database as the only thing available.
  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);
}

// Tests that third-party SPC only uses the web data service in hybrid mode.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       ThirdPartyUsesWebDataService) {
  base::HistogramTester histogram_tester;
  // OS APIs are supported but should not be called.
  EXPECT_CALL(*mock_authenticator_, GetMatchingCredentialIds).Times(0);

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin =
      url::Origin::Create(GURL("https://third-party.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  std::move(web_data_service_callback)
      .Run(handle, CreateWebDatabaseResult(kAvailableCredentialIds));

  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);

  // No uplift or orphan metrics recorded, because we didn't have results from
  // both stores.
  histogram_tester.ExpectTotalCount(kOSStoreUpliftHistogram, 0);
  histogram_tester.ExpectTotalCount(kWebDatabaseHasOrphanedCredentialsHistogram,
                                    0);
}

// Tests that if WebDataService returns null (e.g. error/failure), the barrier
// still completes cleanly and returns the OS credentials.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       WebDataServiceError) {
  base::HistogramTester histogram_tester;

  ExpectAuthenticatorQueryAndReturn(kAvailableCredentialIds);

  WebDataServiceRequestCallback web_data_service_callback;
  WebDataServiceBase::Handle handle =
      ExpectWebDataServiceQueryAndCaptureCallback(&web_data_service_callback);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), mock_service_, std::move(callback));

  // Return a null result to simulate DB failure.
  std::move(web_data_service_callback).Run(handle, nullptr);

  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);

  // No uplift or orphan metrics recorded, because we didn't have results from
  // both stores.
  histogram_tester.ExpectTotalCount(kOSStoreUpliftHistogram, 0);
  histogram_tester.ExpectTotalCount(kWebDatabaseHasOrphanedCredentialsHistogram,
                                    0);
}

// Tests handling of null WebDataService in hybrid mode; should query only the
// OS store.
TEST_F(SecurePaymentConfirmationCredentialFinderHybridModeTest,
       NullWebDataService) {
  base::HistogramTester histogram_tester;

  ExpectAuthenticatorQueryAndReturn(kAvailableCredentialIds);

  MatchedCredentials actual_credentials;
  auto callback = base::BindLambdaForTesting(
      [&actual_credentials](MatchedCredentials result) {
        actual_credentials = std::move(result);
      });

  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  credential_finder_.GetMatchingCredentials(
      kInputCredentialIds, kRelyingPartyId, caller_origin,
      mock_authenticator_.get(), /*web_data_service=*/nullptr,
      std::move(callback));

  ASSERT_TRUE(actual_credentials.has_value());
  ASSERT_EQ(actual_credentials->size(), 1u);
  EXPECT_EQ((*actual_credentials)[0]->credential_id,
            kAvailableCredentialIds[0]);

  // No uplift or orphan metrics recorded, because we didn't have results from
  // both stores.
  histogram_tester.ExpectTotalCount(kOSStoreUpliftHistogram, 0);
  histogram_tester.ExpectTotalCount(kWebDatabaseHasOrphanedCredentialsHistogram,
                                    0);
}

}  // namespace
}  // namespace payments
