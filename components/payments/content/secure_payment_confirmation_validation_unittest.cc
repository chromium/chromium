// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_validation.h"

#include <string>
#include <vector>

#include "components/payments/core/native_error_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
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

TEST(SecurePaymentConfirmationValidationTest, IsValidRequest) {
  auto request = CreateValidRequest();
  std::string error_message;
  EXPECT_TRUE(IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_TRUE(error_message.empty());
}

TEST(SecurePaymentConfirmationValidationTest, EmptyCredentialIds) {
  auto request = CreateValidRequest();
  request->credential_ids.clear();
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kCredentialIdsRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyCredentialId) {
  auto request = CreateValidRequest();
  request->credential_ids.emplace_back();
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kCredentialIdsRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyChallenge) {
  auto request = CreateValidRequest();
  request->challenge.clear();
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kChallengeRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyDisplayName) {
  auto request = CreateValidRequest();
  request->instrument->display_name.clear();
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kInstrumentDisplayNameRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyInstrumentIcon) {
  auto request = CreateValidRequest();
  request->instrument->icon = GURL();
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kValidInstrumentIconRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, InvalidInstrumentIcon) {
  auto request = CreateValidRequest();
  request->instrument->icon = GURL("not-a-url");
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kValidInstrumentIconRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, NonUtf8InstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = {'\xEF', '\xB7', '\xAF'};
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kNonUtf8InstrumentDetailsString, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyInstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = "";
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kEmptyInstrumentDetailsString, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, TooLongInstrumentDetails) {
  auto request = CreateValidRequest();
  request->instrument->details = std::string(4097, '.');
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kTooLongInstrumentDetailsString, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, InvalidRpId) {
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
    std::string error_message;
    EXPECT_FALSE(
        IsValidSecurePaymentConfirmationRequest(request, &error_message));
    EXPECT_EQ(errors::kRpIdRequired, error_message) << "rp_id: " << rp_id;
  }
}

TEST(SecurePaymentConfirmationValidationTest, MissingPayeeNameAndPayeeOrigin) {
  auto request = CreateValidRequest();
  request->payee_name.reset();
  request->payee_origin.reset();
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kPayeeOriginOrPayeeNameRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyPayeeName) {
  auto request = CreateValidRequest();
  request->payee_name = "";
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kPayeeOriginOrPayeeNameRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, NonHttpsPayeeOrigin) {
  auto request = CreateValidRequest();
  request->payee_origin = url::Origin::Create(GURL("http://site.example"));
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kPayeeOriginMustBeHttps, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, NullPaymentEntityLogo) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(nullptr);
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kNonNullPaymentEntityLogoRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyPaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL(), "Label"));
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kValidLogoUrlRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, InvalidPaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL("thisisnotaurl"), "Label"));
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kValidLogoUrlRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest,
     DisallowedSchemePaymentEntityLogoUrl) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      GURL("blob://blob.foo.com/logo.png"), "Label"));
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kValidLogoUrlSchemeRequired, error_message);
}

TEST(SecurePaymentConfirmationValidationTest, EmptyPaymentEntityLogoLabel) {
  auto request = CreateValidRequest();
  request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      GURL("https://entity.example/icon.png"), ""));
  std::string error_message;
  EXPECT_FALSE(
      IsValidSecurePaymentConfirmationRequest(request, &error_message));
  EXPECT_EQ(errors::kLogoLabelRequired, error_message);
}

}  // namespace
}  // namespace payments
