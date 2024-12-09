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
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "components/payments/content/payment_request_spec.h"
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
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Property;

static constexpr char kChallengeBase64[] = "aaaa";
static constexpr char kCredentialIdBase64[] = "cccc";

class FakeBrowserBoundKey : public BrowserBoundKey {
 public:
  FakeBrowserBoundKey() = default;
  FakeBrowserBoundKey(std::vector<uint8_t> public_key_as_cose_key,
                      std::vector<uint8_t> signature,
                      std::vector<uint8_t> expected_client_data)
      : public_key_as_cose_key_(public_key_as_cose_key),
        signature_(signature),
        expected_client_data_(expected_client_data) {}
  FakeBrowserBoundKey(const FakeBrowserBoundKey& other)
      : public_key_as_cose_key_(other.public_key_as_cose_key_),
        signature_(other.signature_),
        expected_client_data_(other.expected_client_data_) {}
  FakeBrowserBoundKey& operator=(const FakeBrowserBoundKey& other) {
    public_key_as_cose_key_ = other.public_key_as_cose_key_;
    signature_ = other.signature_;
    expected_client_data_ = other.expected_client_data_;
    return *this;
  }
  ~FakeBrowserBoundKey() override = default;

  std::vector<uint8_t> Sign(const std::vector<uint8_t>& client_data) override {
    if (client_data == expected_client_data_) {
      return signature_;
    }
    return {};
  }
  std::vector<uint8_t> GetPublicKeyAsCoseKey() override {
    return public_key_as_cose_key_;
  }

 private:
  std::vector<uint8_t> public_key_as_cose_key_;
  std::vector<uint8_t> signature_;
  std::vector<uint8_t> expected_client_data_;
};

class FakeBrowserBoundKeyStore : public BrowserBoundKeyStore {
 public:
  FakeBrowserBoundKeyStore() = default;
  ~FakeBrowserBoundKeyStore() override = default;

  std::unique_ptr<BrowserBoundKey> GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id) override {
    if (key_map_.find(credential_id) == key_map_.end()) {
      key_map_[credential_id] = FakeBrowserBoundKey();
    }
    return std::unique_ptr<BrowserBoundKey>(
        new FakeBrowserBoundKey(key_map_[credential_id]));
  }

  void PutFakeKey(const std::vector<uint8_t>& credential_id,
                  FakeBrowserBoundKey bbk) {
    key_map_[credential_id] = bbk;
  }

  base::WeakPtr<FakeBrowserBoundKeyStore> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::map<std::vector<uint8_t>, FakeBrowserBoundKey> key_map_;
  base::WeakPtrFactory<FakeBrowserBoundKeyStore> weak_ptr_factory_{this};
};

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

  mojom::SecurePaymentConfirmationRequestPtr MakeRequest() {
    auto request = mojom::SecurePaymentConfirmationRequest::New();
    request->challenge =
        std::vector<uint8_t>(challenge_bytes_.begin(), challenge_bytes_.end());
    return request;
  }

  std::unique_ptr<BrowserBoundKeyStore> MakeFakeBrowserBoundKeyStore() {
    FakeBrowserBoundKeyStore* key_store = new FakeBrowserBoundKeyStore();
    browser_bound_key_store_ = key_store->GetWeakPtr();
    return base::WrapUnique(static_cast<BrowserBoundKeyStore*>(key_store));
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

  base::WeakPtr<FakeBrowserBoundKeyStore> browser_bound_key_store_;
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
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), std::move(authenticator),
      /*network_label=*/u"", /*network_icon=*/SkBitmap(),
      /*issuer_label=*/u"", /*issuer_icon=*/SkBitmap());

  std::vector<uint8_t> expected_bytes =
      std::vector<uint8_t>(challenge_bytes_.begin(), challenge_bytes_.end());

  EXPECT_CALL(*mock_authenticator, GetAssertion(_, _))
      .WillOnce(
          [&expected_bytes](
              blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
              blink::mojom::Authenticator::GetAssertionCallback callback) {
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
TEST_F(SecurePaymentConfirmationAppTest, AddsBrowserBoundKeyAndSignature) {
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
  FakeBrowserBoundKey browser_bound_key(public_key_as_cose_key, signature,
                                        client_data_json);
  SecurePaymentConfirmationApp app(
      web_contents_, "effective_rp.example", payment_instrument_label_,
      /*payment_instrument_icon=*/std::make_unique<SkBitmap>(), credential_id,
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), std::move(authenticator),
      /*network_label=*/u"", /*network_icon=*/SkBitmap(),
      /*issuer_label=*/u"", /*issuer_icon=*/SkBitmap());
  app.SetBrowserBoundKeyStoreForTesting(MakeFakeBrowserBoundKeyStore());
  browser_bound_key_store_->PutFakeKey(credential_id, browser_bound_key);

  EXPECT_CALL(*mock_authenticator,
              SetPaymentOptions(Pointee(
                  Field("browser_bound_public_key",
                        &blink::mojom::PaymentOptions::browser_bound_public_key,
                        Optional(ElementsAreArray(public_key_as_cose_key))))));
  EXPECT_CALL(*mock_authenticator, GetAssertion(_, _))
      .WillOnce(
          [client_data_json](
              blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
              blink::mojom::Authenticator::GetAssertionCallback callback) {
            auto authenticator_response =
                blink::mojom::GetAssertionAuthenticatorResponse::New();
            authenticator_response->info =
                blink::mojom::CommonCredentialInfo::New();
            authenticator_response->info->client_data_json = client_data_json;
            authenticator_response->extensions =
                blink::mojom::AuthenticationExtensionsClientOutputs::New();
            std::move(callback).Run(blink::mojom::AuthenticatorStatus::SUCCESS,
                                    std::move(authenticator_response),
                                    /*dom_exception_details=*/nullptr);
          });
  app.InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(on_instrument_details_ready_called_);
  mojom::PaymentResponsePtr payment_response =
      app.SetAppSpecificResponseFields(mojom::PaymentResponse::New());

  EXPECT_THAT(
      payment_response->get_assertion_authenticator_response->extensions
          ->payment,
      Pointee(Field("browser_bound_signatures",
                    &blink::mojom::AuthenticationExtensionsPaymentResponse::
                        browser_bound_signatures,
                    ElementsAre(ElementsAreArray(signature)))));
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
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), std::move(authenticator),
      /*network_label=*/u"", /*network_icon=*/SkBitmap(),
      /*issuer_label=*/u"", /*issuer_icon=*/SkBitmap());

  EXPECT_CALL(*mock_authenticator, GetAssertion(_, _))
      .WillOnce(RunOnceCallback<1>(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR,
          blink::mojom::GetAssertionAuthenticatorResponse::New(),
          /*dom_exception_details=*/nullptr));
  app.InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());
  EXPECT_FALSE(on_instrument_details_ready_called_);
  EXPECT_TRUE(on_instrument_details_error_called_);
}

}  // namespace
}  // namespace payments
