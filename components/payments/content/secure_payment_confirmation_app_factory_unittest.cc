// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <vector>

#include "base/base64.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/payments/content/mock_payment_app_factory_delegate.h"
#include "components/payments/content/mock_payment_manifest_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/webauthn/core/browser/mock_internal_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;

static constexpr char kChallengeBase64[] = "aaaa";
static constexpr char kCredentialIdBase64[] = "cccc";

class SecurePaymentConfirmationAppFactoryTest : public testing::Test {
 protected:
  SecurePaymentConfirmationAppFactoryTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)),
        web_contents_(web_contents_factory_.CreateWebContents(&context_)) {}

  void SetUp() override {
    ASSERT_TRUE(base::Base64Decode(kChallengeBase64, &challenge_bytes_));
    ASSERT_TRUE(base::Base64Decode(kCredentialIdBase64, &credential_id_bytes_));
  }

  // Creates and returns a minimal SecurePaymentConfirmationRequest object with
  // only required fields filled in to pass parsing.
  //
  // Note that this method adds a payee_origin but *not* a payee_name, as only
  // one of the two are required.
  mojom::SecurePaymentConfirmationRequestPtr
  CreateSecurePaymentConfirmationRequest() {
    auto spc_request = mojom::SecurePaymentConfirmationRequest::New();

    spc_request->credential_ids.emplace_back(credential_id_bytes_.begin(),
                                             credential_id_bytes_.end());
    spc_request->challenge =
        std::vector<uint8_t>(challenge_bytes_.begin(), challenge_bytes_.end());
    spc_request->instrument = blink::mojom::PaymentCredentialInstrument::New();
    spc_request->instrument->display_name = "1234";
    spc_request->instrument->icon = GURL("https://site.example/icon.png");
    spc_request->payee_origin =
        url::Origin::Create(GURL("https://merchant.example"));
    spc_request->rp_id = "rp.example";

    return spc_request;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  SecurePaymentConfirmationAppFactory secure_payment_confirmation_app_factory_;
  std::string challenge_bytes_;
  std::string credential_id_bytes_;
};

// Test that parsing a valid SecureConfirmationPaymentRequest succeeds.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_IsValid) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _)).Times(0);
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// credentialIds field fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyCredentialIds) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->credential_ids.clear();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty ID inside
// the credentialIds field fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyId) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->credential_ids.emplace_back();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty challenge
// fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyChallenge) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->challenge.clear();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// displayName fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyDisplayName) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->display_name.clear();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// instrument icon fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyInstrumentIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->icon = GURL();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid
// instrument icon URL fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_InvalidInstrumentIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->icon =
      GURL("not-a-url");
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid RP
// domain fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_InvalidRpId) {
  std::string invalid_cases[] = {
      "",
      "domains cannot have spaces.example",
      "https://bank.example",
      "username:password@bank.example",
      "bank.example/has/a/path",
      "139.56.146.66",
      "9d68:ea08:fc14:d8be:344c:60a0:c4db:e478",
  };
  for (const std::string& rp_id : invalid_cases) {
    auto method_data = mojom::PaymentMethodData::New();
    method_data->supported_method = "secure-payment-confirmation";
    method_data->secure_payment_confirmation =
        CreateSecurePaymentConfirmationRequest();
    method_data->secure_payment_confirmation->rp_id = rp_id;
    auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
        web_contents_, std::move(method_data));

    // EXPECT_CALL doesn't support <<, so to make it clear which rp_id was being
    // tested in a failure case we use SCOPED_TRACE.
    SCOPED_TRACE(rp_id);
    EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
    secure_payment_confirmation_app_factory_.Create(
        mock_delegate->GetWeakPtr());
  }
}

// Test that parsing a SecurePaymentConfirmationRequest with a missing payeeName
// and payeeOrigin fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_MissingPayeeNameAndPayeeOrigin) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->payee_name.reset();
  method_data->secure_payment_confirmation->payee_origin.reset();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a present but empty
// payeeName fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyPayeeName) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->payee_name = "";
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a non-HTTPS
// payeeOrigin fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_NonHttpsPayeeOrigin) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->payee_origin =
      url::Origin::Create(GURL("http://site.example"));
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// network name fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyNetworkName) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->network_info = mojom::NetworkOrIssuerInformation::New();
  spc_request->network_info->name = "";
  spc_request->network_info->icon = GURL("https://network.example/icon.png");
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// network icon fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyNetworkIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->network_info = mojom::NetworkOrIssuerInformation::New();
  spc_request->network_info->name = "Network Name";
  spc_request->network_info->icon = GURL();
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid
// network icon URL fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_InvalidNetworkIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->network_info = mojom::NetworkOrIssuerInformation::New();
  spc_request->network_info->name = "Network Name";
  spc_request->network_info->icon = GURL("not-a-url");
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// issuer name fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyIssuerName) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->issuer_info = mojom::NetworkOrIssuerInformation::New();
  spc_request->issuer_info->name = "";
  spc_request->issuer_info->icon = GURL("https://issuer.example/icon.png");
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// issuer icon fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyIssuerIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->issuer_info = mojom::NetworkOrIssuerInformation::New();
  spc_request->issuer_info->name = "Issuer Name";
  spc_request->issuer_info->icon = GURL();
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid
// issuer icon URL fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_InvalidIssuerIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->issuer_info = mojom::NetworkOrIssuerInformation::New();
  spc_request->issuer_info->name = "Issuer Name";
  spc_request->issuer_info->icon = GURL("not-a-url");
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

class SecurePaymentConfirmationAppFactoryUsingCredentialStoreAPIsTest
    : public SecurePaymentConfirmationAppFactoryTest {
 public:
  SecurePaymentConfirmationAppFactoryUsingCredentialStoreAPIsTest() {
    feature_list_.InitAndEnableFeature(
        features::kSecurePaymentConfirmationUseCredentialStoreAPIs);
  }

  // Tests that the third-party payment bit is set as required or not correctly
  // for a given origin. The RP ID for this setup is 'rp.example'.
  void TestThirdPartyPaymentBitSetCorrectly(
      url::Origin caller_origin,
      bool expected_require_third_party_payment_bit) {
    auto method_data = mojom::PaymentMethodData::New();
    method_data->supported_method = "secure-payment-confirmation";
    method_data->secure_payment_confirmation =
        CreateSecurePaymentConfirmationRequest();

    auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
        web_contents_, std::move(method_data));

    auto mock_authenticator =
        std::make_unique<webauthn::MockInternalAuthenticator>(web_contents_);
    EXPECT_CALL(*mock_authenticator,
                IsUserVerifyingPlatformAuthenticatorAvailable(_))
        .WillOnce(RunOnceCallback<0>(true));
    EXPECT_CALL(*mock_authenticator, IsGetMatchingCredentialIdsSupported())
        .WillOnce(Return(true));

    // This is the core 'test' line of this method. It ensures that the
    // authenticator device is asked for the right RP ID and credentials, and
    // that the 'third-party payment bit required' flag is set as expected.
    std::vector<std::vector<uint8_t>> expected_credential_ids;
    expected_credential_ids.emplace_back(credential_id_bytes_.begin(),
                                         credential_id_bytes_.end());
    EXPECT_CALL(
        *mock_authenticator,
        GetMatchingCredentialIds("rp.example", Eq(expected_credential_ids),
                                 expected_require_third_party_payment_bit, _))
        .Times(1);

    scoped_refptr<MockPaymentManifestWebDataService> mock_service =
        base::MakeRefCounted<MockPaymentManifestWebDataService>();

    EXPECT_CALL(*mock_delegate, CreateInternalAuthenticator())
        .WillOnce(Return(ByMove(std::move(mock_authenticator))));
    EXPECT_CALL(*mock_delegate, GetPaymentManifestWebDataService())
        .WillRepeatedly(Return(mock_service));
    EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
        .WillOnce(ReturnRef(caller_origin));

    secure_payment_confirmation_app_factory_.Create(
        mock_delegate->GetWeakPtr());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    SecurePaymentConfirmationAppFactoryUsingCredentialStoreAPIsTest,
    CorrectlyCalculatesThirdPartyPaymentRequirement_OriginDifferentFromRpId) {
  // Because the RP ID is 'rp.example', and our origin is
  // 'https://site.example', this is a third-party payment authentication.
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));
  TestThirdPartyPaymentBitSetCorrectly(
      caller_origin, /*expected_require_third_party_payment_bit=*/true);
}

TEST_F(SecurePaymentConfirmationAppFactoryUsingCredentialStoreAPIsTest,
       CorrectlyCalculatesThirdPartyPaymentRequirement_OriginSameAsRpId) {
  // Because the RP ID is 'rp.example', and our origin is 'https://rp.example'
  // too, this is a first-party payment authentication.
  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  TestThirdPartyPaymentBitSetCorrectly(
      caller_origin, /*expected_require_third_party_payment_bit=*/false);
}

TEST_F(SecurePaymentConfirmationAppFactoryUsingCredentialStoreAPIsTest,
       CorrectlyCalculatesThirdPartyPaymentRequirement_OriginSameDomainAsRpId) {
  // Because the RP ID is 'rp.example', and our origin is
  // 'https://www.rp.example', this is a first-party payment authentication.
  url::Origin caller_origin =
      url::Origin::Create(GURL("https://www.rp.example"));
  TestThirdPartyPaymentBitSetCorrectly(
      caller_origin, /*expected_require_third_party_payment_bit=*/false);
}

TEST_F(SecurePaymentConfirmationAppFactoryUsingCredentialStoreAPIsTest,
       AppDisabledIfCredentialStoreAPIsUnavailable) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  auto mock_authenticator =
      std::make_unique<webauthn::MockInternalAuthenticator>(web_contents_);
  EXPECT_CALL(*mock_authenticator,
              IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillOnce(RunOnceCallback<0>(true));
  // Make it so that the credential store API support is unavailable.
  EXPECT_CALL(*mock_authenticator, IsGetMatchingCredentialIdsSupported())
      .WillOnce(Return(false));

  scoped_refptr<MockPaymentManifestWebDataService> mock_service =
      base::MakeRefCounted<MockPaymentManifestWebDataService>();

  EXPECT_CALL(*mock_delegate, CreateInternalAuthenticator())
      .WillOnce(Return(ByMove(std::move(mock_authenticator))));
  EXPECT_CALL(*mock_delegate, GetPaymentManifestWebDataService())
      .WillRepeatedly(Return(mock_service));

  // When the credential store APIs are unavailable, we do not create an SPC app
  // (which in turn makes canMakePayment() return false).
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_)).Times(0);
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _)).Times(0);
  EXPECT_CALL(*mock_delegate, OnDoneCreatingPaymentApps()).Times(1);

  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

}  // namespace
}  // namespace payments
