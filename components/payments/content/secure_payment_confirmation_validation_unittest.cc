// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_validation.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

// Creates and returns a minimal SecurePaymentConfirmationRequest object with
// only required fields filled in to pass validation.
mojom::SecurePaymentConfirmationRequestPtr CreateValidRequest() {
  auto spc_request = mojom::SecurePaymentConfirmationRequest::New();

  spc_request->credential_ids.push_back({1, 2, 3, 4});
  spc_request->challenge = {5, 6, 7, 8};
  spc_request->instrument = blink::mojom::PaymentCredentialInstrument::New();
  spc_request->instrument->display_name = "Display Name";
  spc_request->instrument->icon = GURL("https://site.example/icon.png");
  spc_request->payee_origin =
      url::Origin::Create(GURL("https://merchant.example"));
  spc_request->rp_id = "rp.example";

  return spc_request;
}

class SecurePaymentConfirmationValidationTest : public ::testing::Test {
 public:
  SecurePaymentConfirmationValidationTest() {
    feature_list_.InitAndDisableFeature(features::kSPCLocaleValidation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SecurePaymentConfirmationValidationTest, IsValidRequest) {
  auto request = CreateValidRequest();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyCredentialIds) {
  auto request = CreateValidRequest();
  request->credential_ids.clear();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kCredentialIdsRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyCredentialId) {
  auto request = CreateValidRequest();
  request->credential_ids.emplace_back();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kCredentialIdsRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyChallenge) {
  auto request = CreateValidRequest();
  request->challenge.clear();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kChallengeRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyDisplayName) {
  auto request = CreateValidRequest();
  request->instrument->display_name.clear();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kInstrumentDisplayNameRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyInstrumentIcon) {
  auto request = CreateValidRequest();
  request->instrument->icon = GURL();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kValidInstrumentIconRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, InvalidInstrumentIcon) {
  auto request = CreateValidRequest();
  request->instrument->icon = GURL("not-a-url");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kValidInstrumentIconRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, NonUtf8InstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = {'\xEF', '\xB7', '\xAF'};
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kNonUtf8InstrumentDetailsString);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyInstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = "";
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kEmptyInstrumentDetailsString);
}

TEST_F(SecurePaymentConfirmationValidationTest, TooLongInstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = std::string(4097, '.');
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kTooLongInstrumentDetailsString);
}

TEST_F(SecurePaymentConfirmationValidationTest, InvalidRpId) {
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
    auto request = CreateValidRequest();
    request->rp_id = rp_id;
    EXPECT_EQ(
        payments::IsValidSecurePaymentConfirmationRequest(
            request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
        SecurePaymentConfirmationRequestValidationError::kRpIdRequired)
        << "rp_id: " << rp_id;
  }
}

TEST_F(SecurePaymentConfirmationValidationTest,
       MissingPayeeNameAndPayeeOrigin) {
  auto request = CreateValidRequest();
  request->payee_name.reset();
  request->payee_origin.reset();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kPayeeOriginOrPayeeNameRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyPayeeName) {
  auto request = CreateValidRequest();
  request->payee_name = "";
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kPayeeOriginOrPayeeNameRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, NonHttpsPayeeOrigin) {
  auto request = CreateValidRequest();
  request->payee_origin = url::Origin::Create(GURL("http://site.example"));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kPayeeOriginMustBeHttps);
}

TEST_F(SecurePaymentConfirmationValidationTest, NullPaymentEntityLogo) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(nullptr);
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kNonNullPaymentEntityLogoRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyPaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL(), "Label"));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kValidLogoUrlRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, InvalidPaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL("thisisnotaurl"), "Label"));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kValidLogoUrlRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest,
       DisallowedSchemePaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      GURL("blob://blob.foo.com/logo.png"), "Label"));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kValidLogoUrlSchemeRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest, EmptyPaymentEntityLogoLabel) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      GURL("https://entity.example/icon.png"), ""));
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kLogoLabelRequired);
}

TEST_F(SecurePaymentConfirmationValidationTest,
       WebAuthnExtensionsAllowedForFirstParty) {
  auto request = CreateValidRequest();
  request->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  request->extensions->get_cred_blob = true;
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationTest,
       EmptyWebAuthnExtensionsAllowed) {
  auto request = CreateValidRequest();
  request->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();

  // An empty WebAuthn extensions dictionary is allowed for both first and third
  // party requests.
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://third-party.example")),
          "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationTest,
       WebAuthnExtensionsDisallowedForThirdParties) {
  // It is not feasible to test every WebAuthn extension field (as more may be
  // added in the future), so we test one representative field (get_cred_blob).
  auto request = CreateValidRequest();
  request->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  request->extensions->get_cred_blob = true;

  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://third-party.example")),
          "en-US"),
      SecurePaymentConfirmationRequestValidationError::
          kWebAuthnExtensionsNotSupported);
}

class SecurePaymentConfirmationValidationLocaleFeatureEnabledTest
    : public SecurePaymentConfirmationValidationTest {
 public:
  SecurePaymentConfirmationValidationLocaleFeatureEnabledTest() {
    feature_list_.InitAndEnableFeature(features::kSPCLocaleValidation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleMatchExact) {
  auto request = CreateValidRequest();
  request->locales.push_back("en-US");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleMatchLanguageOnly) {
  auto request = CreateValidRequest();
  request->locales.push_back("en");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-CA"),
      SecurePaymentConfirmationRequestValidationError::kOk);

  request->locales.clear();
  request->locales.push_back("en-CA");
  EXPECT_EQ(payments::IsValidSecurePaymentConfirmationRequest(
                request, url::Origin::Create(GURL("https://rp.example")), "en"),
            SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleMismatchLanguage) {
  auto request = CreateValidRequest();
  request->locales.push_back("fr");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en"),
      SecurePaymentConfirmationRequestValidationError::kLocaleDoesNotMatch);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleMismatchRegion) {
  auto request = CreateValidRequest();
  request->locales.push_back("en-CA");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kLocaleDoesNotMatch);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleMatchOneOfMultiple) {
  auto request = CreateValidRequest();
  request->locales.push_back("fr");
  request->locales.push_back("es");
  request->locales.push_back("en-US");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleMalformed) {
  auto request = CreateValidRequest();
  request->locales.push_back("en--US");
  request->locales.push_back("enUS");
  request->locales.push_back("en US");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kLocaleDoesNotMatch);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleEmptyList) {
  auto request = CreateValidRequest();
  request->locales.clear();
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleCaseInsensitiveMatch) {
  auto request = CreateValidRequest();
  request->locales.push_back("en-us");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleUnderscoreSeparatorMatch) {
  auto request = CreateValidRequest();
  request->locales.push_back("en_US");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocaleMalformedApplicationLocale) {
  auto request = CreateValidRequest();
  request->locales.push_back("en-US");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "---"),
      SecurePaymentConfirmationRequestValidationError::kLocaleDoesNotMatch);
}

TEST_F(SecurePaymentConfirmationValidationLocaleFeatureEnabledTest,
       LocalePartiallyMalformedList) {
  auto request = CreateValidRequest();
  request->locales.push_back("invalid_tag");
  request->locales.push_back("en-US");
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}

TEST_F(SecurePaymentConfirmationValidationTest, LocaleFeatureFlagDisabled) {
  auto request = CreateValidRequest();
  request->locales.push_back("fr-CA");
  // Even if it wouldn't match, the feature is disabled so it returns kOk.
  EXPECT_EQ(
      payments::IsValidSecurePaymentConfirmationRequest(
          request, url::Origin::Create(GURL("https://rp.example")), "en-US"),
      SecurePaymentConfirmationRequestValidationError::kOk);
}
}  // namespace
}  // namespace payments
