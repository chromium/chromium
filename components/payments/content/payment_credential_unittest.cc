// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"
#include "components/payments/content/mock_payment_manifest_web_data_service.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "components/webauthn/core/browser/mock_internal_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace payments {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pointee;

namespace {

struct PaymentCredentialDeleter {
  void operator()(PaymentCredential* payment_credential) {
    payment_credential->ResetAndDeleteThis();
  }
};

}  // namespace

class PaymentCredentialTest : public ::testing::Test {
 public:
  PaymentCredentialTest() = default;

  void SetUp() override {
    web_contents_ = web_contents_factory_.CreateWebContents(&context_);
  }

 protected:
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateMockInternalAuthenticator() {
    mock_internal_authenticator_ =
        new webauthn::MockInternalAuthenticator(web_contents_);
    return base::WrapUnique(static_cast<webauthn::InternalAuthenticator*>(
        &*mock_internal_authenticator_));
  }

  std::unique_ptr<BrowserBoundKeyStore> CreateFakeBrowserBoundKeyStore() {
    auto key_store = std::make_unique<FakeBrowserBoundKeyStore>();
    fake_browser_bound_key_store_ = key_store->GetWeakPtr();
    return base::WrapUnique<BrowserBoundKeyStore>(key_store.release());
  }

  std::unique_ptr<PaymentCredential, PaymentCredentialDeleter>
  CreatePaymentCredential() {
    mojo::PendingRemote<mojom::PaymentCredential> remote;
    mojo::PendingReceiver<mojom::PaymentCredential> receiver =
        remote.InitWithNewPipeAndPassReceiver();
    auto payment_credential =
        std::unique_ptr<PaymentCredential, PaymentCredentialDeleter>(
            new PaymentCredential(*web_contents_->GetPrimaryMainFrame(),
                                  /*receiver=*/std::move(receiver),
                                  mock_web_data_service_,
                                  CreateMockInternalAuthenticator()));
    payment_credential->SetBrowserBoundKeyStoreForTesting(
        CreateFakeBrowserBoundKeyStore());
    payment_credential->SetRandomBytesAsVectorForTesting(
        base::BindLambdaForTesting([this](size_t length) {
          EXPECT_EQ(length, 32u);
          return fake_browser_bound_key_id_;
        }));
    return payment_credential;
  }

  const std::vector<uint8_t> fake_challenge_ = {0x01, 0x02, 0x03, 0x04};
  const std::vector<uint8_t> fake_credential_id_ = {0x10, 0x11, 0x12, 0x13};
  const std::vector<uint8_t> fake_signature_ = {0x20, 0x21, 0x22, 0x23};
  const std::vector<uint8_t> fake_client_data_json_ = {0x30, 0x31, 0x32, 0x33};
  const std::vector<uint8_t> fake_browser_bound_key_id_ = {0x40, 0x41, 0x42,
                                                           0x43};
  const std::vector<uint8_t> fake_public_key_ = {0x50, 0x51, 0x52, 0x53};
  const std::string fake_relying_party_id_ = "relying-party.example";
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  scoped_refptr<payments::MockPaymentManifestWebDataService>
      mock_web_data_service_ =
          base::MakeRefCounted<MockPaymentManifestWebDataService>();
  raw_ptr<webauthn::MockInternalAuthenticator> mock_internal_authenticator_;
  base::WeakPtr<FakeBrowserBoundKeyStore> fake_browser_bound_key_store_;
  base::MockCallback<mojom::PaymentCredential::MakePaymentCredentialCallback>
      mock_payment_credential_callback_;
};

static testing::Matcher<::blink::mojom::MakeCredentialAuthenticatorResponsePtr>
AuthenticatorResponseWithBrowserBoundSignature(std::vector<uint8_t> signature) {
  return Pointee(Field(
      "payment", &::blink::mojom::MakeCredentialAuthenticatorResponse::payment,
      Pointee(Field("browser_bound_signatures",
                    &::blink::mojom::AuthenticationExtensionsPaymentResponse::
                        browser_bound_signatures,
                    ElementsAre(signature)))));
}

TEST_F(PaymentCredentialTest, MakePaymentCredentialAddsBrowserBoundKey) {
  base::test::ScopedFeatureList features(
      blink::features::kSecurePaymentConfirmationBrowserBoundKeys);
  std::unique_ptr<PaymentCredential, PaymentCredentialDeleter>
      payment_credential = CreatePaymentCredential();
  fake_browser_bound_key_store_->PutFakeKey(
      fake_browser_bound_key_id_,
      FakeBrowserBoundKey(fake_public_key_, fake_signature_,
                          fake_client_data_json_));
  auto creation_options =
      blink::mojom::PublicKeyCredentialCreationOptions::New();
  creation_options->relying_party.id = fake_relying_party_id_;
  creation_options->is_payment_credential_creation = true;
  creation_options->challenge = fake_challenge_;
  auto fake_authenticator_response =
      ::blink::mojom::MakeCredentialAuthenticatorResponse::New();
  fake_authenticator_response->info =
      ::blink::mojom::CommonCredentialInfo::New();
  fake_authenticator_response->info->raw_id = fake_credential_id_;
  fake_authenticator_response->info->client_data_json = fake_client_data_json_;

  EXPECT_CALL(*mock_web_data_service_,
              SetBrowserBoundKey(fake_credential_id_, fake_relying_party_id_,
                                 fake_browser_bound_key_id_, _));
  ::blink::mojom::PaymentOptionsPtr actual_payment_options;
  EXPECT_CALL(*mock_internal_authenticator_, SetPaymentOptions(_))
      .Times(1)
      .WillOnce([&actual_payment_options](
                    ::blink::mojom::PaymentOptionsPtr payment_options) {
        actual_payment_options = payment_options.Clone();
      });
  EXPECT_CALL(*mock_internal_authenticator_,
              MakeCredential(Eq(std::ref(creation_options)), _))
      .WillRepeatedly(
          [&fake_authenticator_response](
              ::blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
              ::blink::mojom::Authenticator::MakeCredentialCallback callback) {
            std::move(callback).Run(
                ::blink::mojom::AuthenticatorStatus::SUCCESS,
                fake_authenticator_response.Clone(),
                /*exception_details=*/nullptr);
          });
  EXPECT_CALL(
      mock_payment_credential_callback_,
      Run(Eq(::blink::mojom::AuthenticatorStatus::SUCCESS),
          AuthenticatorResponseWithBrowserBoundSignature(fake_signature_), _));

  payment_credential->MakePaymentCredential(
      creation_options.Clone(), mock_payment_credential_callback_.Get());
  ASSERT_FALSE(actual_payment_options.is_null());
  EXPECT_EQ(actual_payment_options->browser_bound_public_key, fake_public_key_);
}

}  // namespace payments
