// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/mock_payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/webauthn/core/browser/mock_internal_authenticator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace payments {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;

#if BUILDFLAG(IS_ANDROID)
static constexpr char kAlgorithmIdentifier = 1;
#endif  // BUILDFLAG(IS_ANDROID)
static constexpr char kChallengeBase64[] = "aaaa";
static constexpr char kCredentialIdBase64[] = "cccc";

class SecurePaymentConfirmationAppTest : public testing::Test,
                                         public PaymentApp::Delegate {
 protected:
  SecurePaymentConfirmationAppTest()
      : payment_instrument_label_(u"test instrument"),
        web_contents_(web_contents_factory_.CreateWebContents(&context_)) {
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    details->total = mojom::PaymentItem::New();
    details->total->amount = mojom::PaymentCurrencyAmount::New();
    details->total->amount->currency = "USD";
    details->total->amount->value = "1.25";
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    spec_ = std::make_unique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), std::move(details),
        std::move(method_data), /*observer=*/nullptr, /*app_locale=*/"en-US");
  }

  void SetUp() override {
    ASSERT_TRUE(base::Base64Decode(kChallengeBase64, &challenge_bytes_));
    ASSERT_TRUE(base::Base64Decode(kCredentialIdBase64, &credential_id_bytes_));
  }

  mojom::SecurePaymentConfirmationRequestPtr MakeRequest(
      std::optional<
          std::vector<device::PublicKeyCredentialParams::CredentialInfo>>
          credential_parameters = std::nullopt) {
    auto request = mojom::SecurePaymentConfirmationRequest::New();
    request->challenge =
        std::vector<uint8_t>(challenge_bytes_.begin(), challenge_bytes_.end());
    if (credential_parameters) {
      request->browser_bound_pub_key_cred_params =
          std::move(*credential_parameters);
    }
    return request;
  }

  // PaymentApp::Delegate:
  void OnInstrumentDetailsReady(const std::string& method_name,
                                const std::string& stringified_details,
                                const PayerData& payer_data) override {
    EXPECT_EQ(method_name, methods::kSecurePaymentConfirmation);
    EXPECT_EQ(stringified_details, "{}");
    EXPECT_EQ(payer_data.payer_name, "");
    EXPECT_EQ(payer_data.payer_email, "");
    EXPECT_EQ(payer_data.payer_phone, "");
    EXPECT_TRUE(payer_data.shipping_address.is_null());
    EXPECT_EQ(payer_data.selected_shipping_option_id, "");

    on_instrument_details_ready_called_ = true;
  }

  void OnInstrumentDetailsError(const std::string& error_message) override {
    EXPECT_EQ(error_message,
              "The operation either timed out or was not allowed. See: "
              "https://www.w3.org/TR/webauthn-2/"
              "#sctn-privacy-considerations-client.");
    on_instrument_details_error_called_ = true;
  }

  std::u16string payment_instrument_label_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  std::string challenge_bytes_;
  std::string credential_id_bytes_;
  bool on_instrument_details_ready_called_ = false;
  bool on_instrument_details_error_called_ = false;

  scoped_refptr<FakeBrowserBoundKeyStore> browser_bound_key_store_ =
      base::MakeRefCounted<FakeBrowserBoundKeyStore>();
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  base::WeakPtrFactory<SecurePaymentConfirmationAppTest> weak_ptr_factory_{
      this};
};

TEST_F(SecurePaymentConfirmationAppTest, Smoke) {
  std::vector<uint8_t> credential_id(credential_id_bytes_.begin(),
                                     credential_id_bytes_.end());

  auto authenticator =
      std::make_unique<webauthn::MockInternalAuthenticator>(web_contents_);
  webauthn::MockInternalAuthenticator* mock_authenticator = authenticator.get();

  SecurePaymentConfirmationApp app(
      web_contents_, "effective_rp.example", payment_instrument_label_,
      /*payment_instrument_icon=*/std::make_unique<SkBitmap>(),
      std::move(credential_id),
      /*passkey_browser_binder=*/nullptr,
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), std::move(authenticator),
      /*network_label=*/u"", /*network_icon=*/std::make_unique<SkBitmap>(),
      /*issuer_label=*/u"", /*issuer_icon=*/std::make_unique<SkBitmap>());

  std::vector<uint8_t> expected_bytes =
      std::vector<uint8_t>(challenge_bytes_.begin(), challenge_bytes_.end());

  EXPECT_CALL(*mock_authenticator, GetAssertion(_, _))
      .WillOnce(
          [&expected_bytes](
              blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
              webauthn::InternalAuthenticator::GetAssertionCallback callback) {
            EXPECT_EQ(options->challenge, expected_bytes);
            auto struct_ptr_is_not_null = Property(
                &mojo::StructPtr<
                    blink::mojom::AuthenticationExtensionsClientInputs>::
                    is_null,
                false);
            EXPECT_THAT(options->extensions, struct_ptr_is_not_null);
            std::move(callback).Run(
                blink::mojom::AuthenticatorStatus::SUCCESS,
                blink::mojom::GetAssertionAuthenticatorResponse::New(),
                /*dom_exception_details=*/nullptr);
          });
  app.InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());
  EXPECT_TRUE(on_instrument_details_ready_called_);
  EXPECT_FALSE(on_instrument_details_error_called_);
}

#if BUILDFLAG(IS_ANDROID)
struct BrowserBoundKeyTestParams {
  std::optional<
      std::vector<::device::PublicKeyCredentialParams::CredentialInfo>>
      credential_parameters;
  int32_t algorithm_identifier = 0;
  bool is_new_bbk = false;
  bool is_off_the_record = false;
  bool expect_browser_bound_key = false;
  std::string test_name_suffix;
};

class SecurePaymentConfirmationAppBrowserBindingTest
    : public SecurePaymentConfirmationAppTest,
      public ::testing::WithParamInterface<BrowserBoundKeyTestParams> {};

INSTANTIATE_TEST_SUITE_P(
    SecurePaymentConfirmationAppBrowserBindingTest,
    SecurePaymentConfirmationAppBrowserBindingTest,
    ::testing::Values(
        BrowserBoundKeyTestParams{
            .credential_parameters =
                {{device::PublicKeyCredentialParams::CredentialInfo(
                    device::CredentialType::kPublicKey,
                    kAlgorithmIdentifier)}},
            .algorithm_identifier = kAlgorithmIdentifier,
            .is_new_bbk = false,
            .is_off_the_record = false,
            .expect_browser_bound_key = true,
            .test_name_suffix = "WithSpecifiedAlgorithm",
        },
        BrowserBoundKeyTestParams{
            .credential_parameters =
                {{device::PublicKeyCredentialParams::CredentialInfo(
                    device::CredentialType::kPublicKey,
                    kAlgorithmIdentifier)}},
            .algorithm_identifier = kAlgorithmIdentifier,
            .is_new_bbk = true,
            .is_off_the_record = false,
            .expect_browser_bound_key = true,
            .test_name_suffix = "WithoutPreExistingKey",
        },
        BrowserBoundKeyTestParams{
            .credential_parameters =
                {{device::PublicKeyCredentialParams::CredentialInfo(
                    device::CredentialType::kPublicKey,
                    kAlgorithmIdentifier)}},
            .algorithm_identifier = kAlgorithmIdentifier,
            .is_new_bbk = false,
            .is_off_the_record = true,
            .expect_browser_bound_key = true,
            .test_name_suffix = "WhenOffTheRecordWithPreExistingKey",
        },
        BrowserBoundKeyTestParams{
            .credential_parameters =
                {{device::PublicKeyCredentialParams::CredentialInfo(
                    device::CredentialType::kPublicKey,
                    kAlgorithmIdentifier)}},
            .algorithm_identifier = kAlgorithmIdentifier,
            .is_new_bbk = true,
            .is_off_the_record = true,
            .expect_browser_bound_key = false,
            .test_name_suffix = "WhenOffTheRecordWithoutPreExistingKey",
        },
        BrowserBoundKeyTestParams{
            .credential_parameters = std::nullopt,
            .algorithm_identifier = base::strict_cast<int32_t>(
                device::CoseAlgorithmIdentifier::kEs256),
            .is_new_bbk = true,
            .is_off_the_record = false,
            .expect_browser_bound_key = true,
            .test_name_suffix = "Es256WithDefaults",
        },
        BrowserBoundKeyTestParams{
            .credential_parameters = std::nullopt,
            .algorithm_identifier = base::strict_cast<int32_t>(
                device::CoseAlgorithmIdentifier::kRs256),
            .is_new_bbk = true,
            .is_off_the_record = false,
            .expect_browser_bound_key = true,
            .test_name_suffix = "Rs256WithDefaults",
        },
        BrowserBoundKeyTestParams{
            .credential_parameters = std::nullopt,
            .algorithm_identifier = kAlgorithmIdentifier,
            .is_new_bbk = true,
            .is_off_the_record = false,
            .expect_browser_bound_key = false,
            .test_name_suffix = "WithNonDefaultAlgorithm",
        }),
    [](const ::testing::TestParamInfo<BrowserBoundKeyTestParams>& info) {
      return info.param.test_name_suffix;
    });

auto InvokeAuthenticatorCallback(std::vector<uint8_t> client_data_json) {
  auto authenticator_response =
      blink::mojom::GetAssertionAuthenticatorResponse::New();
  authenticator_response->info = blink::mojom::CommonCredentialInfo::New();
  authenticator_response->info->client_data_json = client_data_json;
  authenticator_response->extensions =
      blink::mojom::AuthenticationExtensionsClientOutputs::New();
  return base::test::RunOnceCallback<1>(
      blink::mojom::AuthenticatorStatus::SUCCESS,
      std::move(authenticator_response),
      /*dom_exception_details=*/nullptr);
}

TEST_P(SecurePaymentConfirmationAppBrowserBindingTest,
       AddsBrowserBoundKeyAndSignature) {
  context_.set_is_off_the_record(GetParam().is_off_the_record);
  base::test::ScopedFeatureList features(
      blink::features::kSecurePaymentConfirmationBrowserBoundKeys);
  auto authenticator =
      std::make_unique<webauthn::MockInternalAuthenticator>(web_contents_);
  webauthn::MockInternalAuthenticator* mock_authenticator = authenticator.get();
  std::vector<uint8_t> credential_id(credential_id_bytes_.begin(),
                                     credential_id_bytes_.end());
  const std::vector<uint8_t> client_data_json({0x01, 0x02, 0x03, 0x04});
  const std::vector<uint8_t> public_key_as_cose_key({0x05, 0x06, 0x07, 0x08});
  const std::vector<uint8_t> signature({0x09, 0x0a, 0x0b, 0x0c});
  const std::vector<uint8_t> browser_bound_key_id({0x0d, 0x0e, 0x0f, 0x10});
  scoped_refptr<MockPaymentManifestWebDataService> mock_service =
      base::MakeRefCounted<MockPaymentManifestWebDataService>();
  auto binder = std::make_unique<PasskeyBrowserBinder>(browser_bound_key_store_,
                                                       mock_service);
  binder->SetRandomBytesAsVectorCallbackForTesting(base::BindRepeating(
      [](const std::vector<uint8_t>& value, size_t) { return value; },
      browser_bound_key_id));
  SecurePaymentConfirmationApp app(
      web_contents_, "effective_rp.example", payment_instrument_label_,
      /*payment_instrument_icon=*/std::make_unique<SkBitmap>(), credential_id,
      std::move(binder), url::Origin::Create(GURL("https://merchant.example")),
      spec_->AsWeakPtr(), MakeRequest(GetParam().credential_parameters),
      std::move(authenticator),
      /*network_label=*/u"", /*network_icon=*/std::make_unique<SkBitmap>(),
      /*issuer_label=*/u"", /*issuer_icon=*/std::make_unique<SkBitmap>());
  browser_bound_key_store_->PutFakeKey(FakeBrowserBoundKey(
      browser_bound_key_id, public_key_as_cose_key, signature,
      GetParam().algorithm_identifier, client_data_json,
      /*is_new=*/GetParam().is_new_bbk));
  WebDataServiceConsumer* web_data_service_consumer = nullptr;
  WebDataServiceBase::Handle web_data_service_handle = 1234;
  EXPECT_CALL(*mock_service, GetBrowserBoundKey(Eq(credential_id),
                                                Eq("effective_rp.example"), _))
      .WillOnce(DoAll(SaveArg<2>(&web_data_service_consumer),
                      Return(web_data_service_handle)));

  EXPECT_CALL(
      *mock_authenticator,
      SetPaymentOptions(Pointee(Field(
          "browser_bound_public_key",
          &blink::mojom::PaymentOptions::browser_bound_public_key,
          GetParam().expect_browser_bound_key
              ? std::optional<std::vector<uint8_t>>(public_key_as_cose_key)
              : std::nullopt))));
  EXPECT_CALL(*mock_authenticator, GetAssertion(_, _))
      .WillOnce(InvokeAuthenticatorCallback(client_data_json));
  app.InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());

  // Simulate the retrieval of an existing browser bound key.
  ASSERT_TRUE(web_data_service_consumer);
  auto metadata_result =
      std::make_unique<WDResult<std::optional<std::vector<uint8_t>>>>(
          WDResultType::BROWSER_BOUND_KEY, GetParam().is_new_bbk
                                               ? std::vector<uint8_t>()
                                               : browser_bound_key_id);
  web_data_service_consumer->OnWebDataServiceRequestDone(
      web_data_service_handle, std::move(metadata_result));

  ASSERT_TRUE(on_instrument_details_ready_called_);
  mojom::PaymentResponsePtr payment_response =
      app.SetAppSpecificResponseFields(mojom::PaymentResponse::New());

  EXPECT_THAT(
      payment_response->get_assertion_authenticator_response->extensions
          ->payment,
      Pointee(Field("browser_bound_signature",
                    &blink::mojom::AuthenticationExtensionsPaymentResponse::
                        browser_bound_signature,
                    ElementsAreArray(GetParam().expect_browser_bound_key
                                         ? signature
                                         : std::vector<uint8_t>()))));
}
#endif  // BUILDFLAG(IS_ANDROID)

// Test that OnInstrumentDetailsError is called when the authenticator returns
// an error.
TEST_F(SecurePaymentConfirmationAppTest, OnInstrumentDetailsError) {
  std::vector<uint8_t> credential_id(credential_id_bytes_.begin(),
                                     credential_id_bytes_.end());

  auto authenticator =
      std::make_unique<webauthn::MockInternalAuthenticator>(web_contents_);
  webauthn::MockInternalAuthenticator* mock_authenticator = authenticator.get();

  SecurePaymentConfirmationApp app(
      web_contents_, "effective_rp.example", payment_instrument_label_,
      /*payment_instrument_icon=*/std::make_unique<SkBitmap>(),
      std::move(credential_id),
      /*passkey_browser_binder=*/nullptr,
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), std::move(authenticator),
      /*network_label=*/u"", /*network_icon=*/std::make_unique<SkBitmap>(),
      /*issuer_label=*/u"", /*issuer_icon=*/std::make_unique<SkBitmap>());

  EXPECT_CALL(*mock_authenticator, GetAssertion(_, _))
      .WillOnce(RunOnceCallback<1>(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR,
          blink::mojom::GetAssertionAuthenticatorResponse::New(),
          /*dom_exception_details=*/nullptr));
  app.InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());
  EXPECT_FALSE(on_instrument_details_ready_called_);
  EXPECT_TRUE(on_instrument_details_error_called_);
}

class SecurePaymentConfirmationAppFallbackTest
    : public SecurePaymentConfirmationAppTest {
 public:
  SecurePaymentConfirmationAppFallbackTest() {
    feature_list_.InitAndEnableFeature(
        features::kSecurePaymentConfirmationFallback);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the SPC app can be created without credentials.
TEST_F(SecurePaymentConfirmationAppFallbackTest, NoCredentials) {
  SecurePaymentConfirmationApp app(
      web_contents_, "effective_rp.example", payment_instrument_label_,
      /*payment_instrument_icon=*/std::make_unique<SkBitmap>(),
      /*credential_id=*/std::vector<uint8_t>(),
      /*passkey_browser_binder=*/nullptr,
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), /*authenticator=*/nullptr,
      /*network_label=*/u"", /*network_icon=*/std::make_unique<SkBitmap>(),
      /*issuer_label=*/u"", /*issuer_icon=*/std::make_unique<SkBitmap>());

  EXPECT_FALSE(app.HasEnrolledInstrument());
  EXPECT_EQ(app.GetId(), "spc");
}

// Test that the SPC app returns HasEnrolledInstrument true when the fallback
// feature is enabled but there are credentials (i.e. no fallback).
TEST_F(SecurePaymentConfirmationAppFallbackTest, WithCredentials) {
  std::vector<uint8_t> credential_id(credential_id_bytes_.begin(),
                                     credential_id_bytes_.end());
  SecurePaymentConfirmationApp app(
      web_contents_, "effective_rp.example", payment_instrument_label_,
      /*payment_instrument_icon=*/std::make_unique<SkBitmap>(), credential_id,
      /*passkey_browser_binder=*/nullptr,
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(),
      std::make_unique<webauthn::MockInternalAuthenticator>(web_contents_),
      /*network_label=*/u"", /*network_icon=*/std::make_unique<SkBitmap>(),
      /*issuer_label=*/u"", /*issuer_icon=*/std::make_unique<SkBitmap>());

  EXPECT_TRUE(app.HasEnrolledInstrument());
  EXPECT_EQ(app.GetId(), base::Base64Encode(credential_id));
}

}  // namespace
}  // namespace payments
