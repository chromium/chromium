// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/mock_payment_app_factory_delegate.h"
#include "components/payments/content/mock_secure_payment_confirmation_credential_finder.h"
#include "components/payments/content/mock_web_payments_web_data_service.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webauthn/core/browser/mock_internal_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_web_contents.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/payments/content/mock_content_payment_request_delegate.h"
#endif

namespace payments {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Pointer;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr std::string kRpId = "rp.example";
constexpr char kChallengeBase64[] = "aaaa";
constexpr char kCredentialIdBase64[] = "cccc";
constexpr int kDefaultFakeBitmapHeight = 32;

struct MockAuthenticatorOptions {
  bool is_user_verifying_platform_authenticator_available = true;
};

#if BUILDFLAG(IS_ANDROID)
std::optional<std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
NoMatchingCredentials() {
  return std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>();
}
#endif  // BUILDFLAG(IS_ANDROID)

std::optional<std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
GetMatchingCredentialsIsUnsupported() {
  return {};
}

class SecurePaymentConfirmationAppFactoryTest : public testing::Test {
 protected:
  const GURL kInstrumentIconUrl = GURL("https://site.example/icon.png");

  SecurePaymentConfirmationAppFactoryTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)),
        web_contents_(web_contents_factory_.CreateWebContents(&context_)) {}

  void SetUp() override {
    ASSERT_TRUE(base::Base64Decode(kChallengeBase64, &challenge_bytes_));
    ASSERT_TRUE(base::Base64Decode(kCredentialIdBase64, &credential_id_bytes_));
    secure_payment_confirmation_app_factory_ =
        std::make_unique<SecurePaymentConfirmationAppFactory>();

    auto mock_credential_finder =
        std::make_unique<MockSecurePaymentConfirmationCredentialFinder>();
    mock_credential_finder_ = mock_credential_finder.get();
    secure_payment_confirmation_app_factory_->SetCredentialFinderForTesting(
        std::move(mock_credential_finder));

    mock_authenticator_ = CreateMockInternalAuthenticator();
    mock_service_ = base::MakeRefCounted<MockWebPaymentsWebDataService>();
  }

  std::unique_ptr<MockPaymentAppFactoryDelegate> CreateMockDelegate(
      mojom::PaymentMethodDataPtr method_data) {
    auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
        web_contents_, std::move(method_data));
    EXPECT_CALL(*mock_delegate, CreateInternalAuthenticator())
        .WillOnce(Return(ByMove(std::move(mock_authenticator_))));
    EXPECT_CALL(*mock_delegate, GetWebPaymentsWebDataService())
        .WillRepeatedly(Return(mock_service_));
#if !BUILDFLAG(IS_ANDROID)
    ON_CALL(*mock_delegate, GetPaymentRequestDelegate())
        .WillByDefault(testing::Return(
            mock_content_payment_request_delegate_.GetContentWeakPtr()));
#endif

    return mock_delegate;
  }

  std::unique_ptr<webauthn::MockInternalAuthenticator>
  CreateMockInternalAuthenticator(MockAuthenticatorOptions options = {}) {
    auto mock_authenticator =
        std::make_unique<webauthn::MockInternalAuthenticator>(web_contents_);
    ON_CALL(*mock_authenticator,
            IsUserVerifyingPlatformAuthenticatorAvailable(_))
        .WillByDefault(RunOnceCallback<0>(
            options.is_user_verifying_platform_authenticator_available));
    return mock_authenticator;
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
    spc_request->instrument->icon = kInstrumentIconUrl;
    spc_request->payee_origin =
        url::Origin::Create(GURL("https://merchant.example"));
    spc_request->rp_id = kRpId;

    return spc_request;
  }

  void MockFindMatchingCredential(std::string credential_id_bytes) {
    std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
        credentials;
    std::vector<uint8_t> credential_id(credential_id_bytes.begin(),
                                       credential_id_bytes.end());
    credentials.emplace_back(
        std::make_unique<SecurePaymentConfirmationCredential>(
            std::move(credential_id), kRpId,
            /*user_id=*/std::vector<uint8_t>()));

    EXPECT_CALL(*mock_credential_finder_, GetMatchingCredentials)
        .WillOnce(RunOnceCallback<5>(std::move(credentials)));
  }

  // Using mock time in this environment to reduce flakiness in TSAN builders.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<SecurePaymentConfirmationAppFactory>
      secure_payment_confirmation_app_factory_;
  std::string challenge_bytes_;
  std::string credential_id_bytes_;
  std::unique_ptr<webauthn::MockInternalAuthenticator> mock_authenticator_;
  scoped_refptr<MockWebPaymentsWebDataService> mock_service_;
  // Owned by `secure_payment_confirmation_app_factory_`, so must be declared
  // after it to avoid a dangling raw_ptr during destruction.
  raw_ptr<MockSecurePaymentConfirmationCredentialFinder>
      mock_credential_finder_;

 private:
  crypto::ScopedFakeUnexportableKeyProvider scoped_key_provider_;
  // MockContentPaymentRequestDelegate is not available on Android.
#if !BUILDFLAG(IS_ANDROID)
  MockContentPaymentRequestDelegate mock_content_payment_request_delegate_;
#endif
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
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate,
              OnPaymentAppCreationError(errors::kCredentialIdsRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate,
              OnPaymentAppCreationError(errors::kCredentialIdsRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate,
              OnPaymentAppCreationError(errors::kChallengeRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kInstrumentDisplayNameRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kValidInstrumentIconRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kValidInstrumentIconRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid (not
// UTF8) instrument details encoding fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_NonUtf8InstrumentDetails) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  // Set the details string to an invalid UTF-8 string.
  method_data->secure_payment_confirmation->instrument->details = {
      '\xEF', '\xB7', '\xAF'};
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kNonUtf8InstrumentDetailsString, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a present but empty
// instrument details fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyInstrumentDetails) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->details = "";
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kEmptyInstrumentDetailsString, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a very long payment
// instrument details fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_TooLongInstrumentDetails) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  // Set the details string longer than 4096.
  static const std::string kTooLongInstrumentDetails = std::string(4097, '.');
  method_data->secure_payment_confirmation->instrument->details =
      kTooLongInstrumentDetails;
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kTooLongInstrumentDetailsString, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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
    EXPECT_CALL(*mock_delegate,
                OnPaymentAppCreationError(errors::kRpIdRequired, _));
    secure_payment_confirmation_app_factory_->Create(
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

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kPayeeOriginOrPayeeNameRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kPayeeOriginOrPayeeNameRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
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

  EXPECT_CALL(*mock_delegate,
              OnPaymentAppCreationError(errors::kPayeeOriginMustBeHttps, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a null
// PaymentEntityLogo fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_NullPaymentEntityLogo) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->payment_entities_logos.push_back(nullptr);
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(
      *mock_delegate,
      OnPaymentAppCreationError(errors::kNonNullPaymentEntityLogoRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// that has an empty url fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyPaymentEntityLogoUrl) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL(), "Label"));
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate,
              OnPaymentAppCreationError(errors::kValidLogoUrlRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// that has an invalid url fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_InvalidPaymentEntityLogoUrl) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL("thisisnotaurl"), "Label"));
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate,
              OnPaymentAppCreationError(errors::kValidLogoUrlRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// that has a url with a disallowed scheme fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_DisallowedSchemePaymentEntityLogoUrl) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      GURL("blob://blob.foo.com/logo.png"), "Label"));
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(
                                  errors::kValidLogoUrlSchemeRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a PaymentEntityLogo
// that has an empty label fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyPaymentEntityLogoLabel) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->payment_entities_logos.push_back(
      mojom::PaymentEntityLogo::New(GURL("https://entity.example/icon.png"),
                                    /*label=*/""));
  method_data->secure_payment_confirmation = std::move(spc_request);

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  EXPECT_CALL(*mock_delegate,
              OnPaymentAppCreationError(errors::kLogoLabelRequired, _));
  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

TEST_F(SecurePaymentConfirmationAppFactoryTest,
       AppDisabledIfCredentialFetchingFails) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));

  EXPECT_CALL(*mock_credential_finder_, GetMatchingCredentials)
      .WillOnce(RunOnceCallback<5>(GetMatchingCredentialsIsUnsupported()));

  // When the credential store APIs are unavailable, we do not create an SPC app
  // (which in turn makes canMakePayment() return false).
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_)).Times(0);
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _)).Times(0);
  EXPECT_CALL(*mock_delegate, OnDoneCreatingPaymentApps()).Times(1);

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Test that the payment instrument details string is made available to the
// SecurePaymentConfirmationApp.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_PaymentInstrumentDetails) {
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->details =
      "instrument details";
  std::vector<std::vector<uint8_t>> credential_ids =
      method_data->secure_payment_confirmation->credential_ids;
  ASSERT_EQ(credential_ids.size(), 1u);
  GURL icon = method_data->secure_payment_confirmation->instrument->icon;

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));

  std::unique_ptr<PaymentApp> secure_payment_confirmation_app;
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_))
      .WillOnce(MoveArg<0>(&secure_payment_confirmation_app));

  MockFindMatchingCredential(credential_id_bytes_);

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
  std::vector<gfx::Size> icon_sizes({{32, 32}});
  std::vector<SkBitmap> icon_bitmaps(1);
  icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/32);
  static_cast<content::TestWebContents*>(web_contents_.get())
      ->TestDidDownloadImage(icon, /*http_status_code=*/200,
                             std::move(icon_bitmaps), std::move(icon_sizes));

  ASSERT_TRUE(secure_payment_confirmation_app);
  EXPECT_EQ(secure_payment_confirmation_app->GetSublabel(),
            u"instrument details");
}

// Test that a the app is not created when the PaymentRequestSpec becomes null
// just prior to downloads finishing.
TEST_F(
    SecurePaymentConfirmationAppFactoryTest,
    SecureConfirmationPaymentRequest_WhenMissingPaymentRequestSpecDuringDownload) {
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->details =
      "instrument details";
  std::vector<std::vector<uint8_t>> credential_ids =
      method_data->secure_payment_confirmation->credential_ids;
  ASSERT_EQ(credential_ids.size(), 1u);
  GURL icon = method_data->secure_payment_confirmation->instrument->icon;

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_)).Times(0);
  EXPECT_CALL(*mock_delegate, OnDoneCreatingPaymentApps());

  MockFindMatchingCredential(credential_id_bytes_);

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
  std::vector<gfx::Size> icon_sizes({{32, 32}});
  std::vector<SkBitmap> icon_bitmaps(1);
  icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/32);

  mock_delegate->ResetSpec();
  static_cast<content::TestWebContents*>(web_contents_.get())
      ->TestDidDownloadImage(icon, /*http_status_code=*/200,
                             std::move(icon_bitmaps), std::move(icon_sizes));
}

// Test that SecurePaymentConfirmationAppFactory passes the input credentials,
// relying party, etc, down into the credential finder.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       CallsCredentialFinderWithCorrectParameters) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();

  std::vector<std::vector<uint8_t>> credential_ids =
      method_data->secure_payment_confirmation->credential_ids;
  std::string relying_party_id =
      method_data->secure_payment_confirmation->rp_id;
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));

  // Ensure that the SecurePaymentConfirmationAppFactory extracts and passes in
  // the correct set of credentials, relying party id, and caller origin. The
  // remaining parameters are the InternalAuthenticator, the
  // WebPaymentsWebDataService, and the result callback - these are not
  // important to verify.
  EXPECT_CALL(*mock_credential_finder_,
              GetMatchingCredentials(Eq(credential_ids), relying_party_id,
                                     caller_origin, _, _, _));

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
}

// Class wrapping tests relating to payment entity logos support in
// SecurePaymentConfirmationAppFactory.
class SecurePaymentConfirmationAppFactoryPaymentEntitiesLogosTest
    : public SecurePaymentConfirmationAppFactoryTest {
 protected:
  const GURL kPaymentEntity1LogoUrl =
      GURL("https://payment-entity-1.example/icon.png");
  const GURL kPaymentEntity2LogoUrl =
      GURL("https://payment-entity-2.example/icon.png");
  const GURL kPaymentEntity3LogoUrl =
      GURL("https://payment-entity-3.example/icon.png");

  SecurePaymentConfirmationAppFactoryPaymentEntitiesLogosTest() = default;

  // The height can be set here, and expected in a test using
  // IsSkBitmapWithHeight().
  void FakeImageDownloaded(GURL image_url,
                           bool succeeded = true,
                           int height = kDefaultFakeBitmapHeight) {
    std::vector<gfx::Size> icon_sizes({{32, height}});
    std::vector<SkBitmap> icon_bitmaps;
    if (succeeded) {
      icon_bitmaps.emplace_back();
      icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/height);
    }

    ASSERT_TRUE(static_cast<content::TestWebContents*>(web_contents_.get())
                    ->TestDidDownloadImage(image_url, /*http_status_code=*/200,
                                           std::move(icon_bitmaps),
                                           std::move(icon_sizes)));
  }

  base::test::ScopedFeatureList feature_list_{
      blink::features::kSecurePaymentConfirmationUxRefresh};
};

Matcher<const SkBitmap*> IsSkBitmapWithHeight(int height) {
  return Pointer(Property("height", &SkBitmap::height, height));
}

Matcher<PaymentApp::PaymentEntityLogo*> IsPaymentEntityLogo(
    const std::u16string& label,
    Matcher<const SkBitmap*> icon_matcher,
    GURL url) {
  return Pointer(
      AllOf(Field("label", &PaymentApp::PaymentEntityLogo::label, label),
            Field("icon", &PaymentApp::PaymentEntityLogo::icon,
                  Pointer(icon_matcher)),
            Field("url", &PaymentApp::PaymentEntityLogo::url, url)));
}

// Tests that at most two PaymentEntityLogos are accepted by
// SecurePaymentConfirmationAppFactory, and that additional logos are just
// silently dropped.
TEST_F(SecurePaymentConfirmationAppFactoryPaymentEntitiesLogosTest,
       MoreThanTwoPaymentEntityLogos) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  mojom::SecurePaymentConfirmationRequestPtr spc_request =
      CreateSecurePaymentConfirmationRequest();
  spc_request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      kPaymentEntity1LogoUrl, "Payment Entity 1"));
  spc_request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      kPaymentEntity2LogoUrl, "Payment Entity 2"));
  spc_request->payment_entities_logos.push_back(mojom::PaymentEntityLogo::New(
      kPaymentEntity3LogoUrl, "Payment Entity 3"));
  method_data->secure_payment_confirmation = std::move(spc_request);

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  url::Origin caller_origin = url::Origin::Create(GURL("https://rp.example"));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));

  std::unique_ptr<PaymentApp> created_payment_app;
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_))
      .WillOnce(MoveArg<0>(&created_payment_app));

  MockFindMatchingCredential(credential_id_bytes_);

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());

  FakeImageDownloaded(kInstrumentIconUrl);
  FakeImageDownloaded(kPaymentEntity1LogoUrl, /*succeeded=*/true,
                      /*height=*/50);
  FakeImageDownloaded(kPaymentEntity2LogoUrl, /*succeeded=*/true,
                      /*height=*/60);

  // Even though the third entity logo was not downloaded (and was not attempted
  // to be downloaded), the first two should be sufficient and the payment app
  // should be created.
  ASSERT_TRUE(created_payment_app);
  EXPECT_THAT(
      created_payment_app->GetPaymentEntitiesLogos(),
      ElementsAre(
          IsPaymentEntityLogo(u"Payment Entity 1", IsSkBitmapWithHeight(50),
                              kPaymentEntity1LogoUrl),
          IsPaymentEntityLogo(u"Payment Entity 2", IsSkBitmapWithHeight(60),
                              kPaymentEntity2LogoUrl)));
}

#if !BUILDFLAG(IS_IOS)
class SecurePaymentConfirmationAppFactoryBrowserBoundKeysTest
    : public SecurePaymentConfirmationAppFactoryTest {
 protected:
  scoped_refptr<FakeBrowserBoundKeyStore> browser_bound_key_store_ =
      base::MakeRefCounted<FakeBrowserBoundKeyStore>();

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kSecurePaymentConfirmationBrowserBoundKeys};
};

// Test that the browser bound key is retrieved
TEST_F(SecurePaymentConfirmationAppFactoryBrowserBoundKeysTest,
       ProvidesBrowserBoundingToSecurePaymentConfirmationApp) {
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));
  std::vector<uint8_t> browser_bound_key_id({0x11, 0x12, 0x13, 0x14});
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  std::vector<std::vector<uint8_t>> credential_ids =
      method_data->secure_payment_confirmation->credential_ids;
  ASSERT_EQ(credential_ids.size(), 1u);
  std::string relying_party_id =
      method_data->secure_payment_confirmation->rp_id;
  GURL icon = method_data->secure_payment_confirmation->instrument->icon;

  secure_payment_confirmation_app_factory_->SetBrowserBoundKeyStoreForTesting(
      browser_bound_key_store_);

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));
  std::unique_ptr<PaymentApp> secure_payment_confirmation_app;
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_))
      .WillOnce(MoveArg<0>(&secure_payment_confirmation_app));

  MockFindMatchingCredential(credential_id_bytes_);

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
  std::vector<gfx::Size> icon_sizes({{32, 32}});
  std::vector<SkBitmap> icon_bitmaps(1);
  icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/32);
  static_cast<content::TestWebContents*>(web_contents_.get())
      ->TestDidDownloadImage(icon, /*https_status_code=*/200,
                             std::move(icon_bitmaps), std::move(icon_sizes));

  ASSERT_TRUE(secure_payment_confirmation_app);
  PasskeyBrowserBinder* passkey_browser_binder =
      static_cast<SecurePaymentConfirmationApp*>(
          secure_payment_confirmation_app.get())
          ->GetPasskeyBrowserBinderForTesting();
  ASSERT_TRUE(passkey_browser_binder);
  EXPECT_EQ(browser_bound_key_store_.get(),
            passkey_browser_binder->GetBrowserBoundKeyStoreForTesting());
  EXPECT_EQ(mock_service_.get(),
            passkey_browser_binder->GetWebDataServiceForTesting());
}
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
class SecurePaymentConfirmationAppFactoryFallbackTest
    : public SecurePaymentConfirmationAppFactoryTest {
 public:
  SecurePaymentConfirmationAppFactoryFallbackTest() {
    feature_list_.InitAndEnableFeature(
        features::kSecurePaymentConfirmationFallback);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the SecurePaymentConfirmationApp can be created without credentials
// for the fallback flow, with HasEnrolledInstrument false.
TEST_F(SecurePaymentConfirmationAppFactoryFallbackTest,
       Fallback_NoCredentials) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  GURL icon = method_data->secure_payment_confirmation->instrument->icon;

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));
  std::unique_ptr<PaymentApp> secure_payment_confirmation_app;
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_))
      .WillOnce(MoveArg<0>(&secure_payment_confirmation_app));
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _)).Times(0);
  EXPECT_CALL(*mock_delegate, OnDoneCreatingPaymentApps()).Times(1);

  EXPECT_CALL(*mock_credential_finder_, GetMatchingCredentials)
      .WillOnce(RunOnceCallback<5>(NoMatchingCredentials()));

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
  std::vector<gfx::Size> icon_sizes({{32, 32}});
  std::vector<SkBitmap> icon_bitmaps(1);
  icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/32);
  static_cast<content::TestWebContents*>(web_contents_.get())
      ->TestDidDownloadImage(icon, /*http_status_code=*/200,
                             std::move(icon_bitmaps), std::move(icon_sizes));

  ASSERT_TRUE(secure_payment_confirmation_app);
  EXPECT_FALSE(secure_payment_confirmation_app->HasEnrolledInstrument());
}

class SecurePaymentConfirmationAppFactoryUxRefreshTest
    : public SecurePaymentConfirmationAppFactoryTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kSecurePaymentConfirmationUxRefresh};
};

// Test that the SecurePaymentConfirmationApp can be created even when there is
// no user-verify platform authenticator available. This will ultimately create
// a fallback experience for the user.
TEST_F(SecurePaymentConfirmationAppFactoryUxRefreshTest,
       Fallback_NoUserVerifyingPlatformAuthenticator) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  GURL icon = method_data->secure_payment_confirmation->instrument->icon;

  std::unique_ptr<webauthn::MockInternalAuthenticator> mock_authenticator =
      CreateMockInternalAuthenticator(
          {.is_user_verifying_platform_authenticator_available = false});

  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      web_contents_, std::move(method_data));

  scoped_refptr<MockWebPaymentsWebDataService> mock_service =
      base::MakeRefCounted<MockWebPaymentsWebDataService>();
  EXPECT_CALL(*mock_delegate, CreateInternalAuthenticator())
      .WillOnce(Return(ByMove(std::move(mock_authenticator))));
  EXPECT_CALL(*mock_delegate, GetWebPaymentsWebDataService())
      .WillRepeatedly(Return(mock_service));
  std::unique_ptr<PaymentApp> secure_payment_confirmation_app;
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_))
      .WillOnce(MoveArg<0>(&secure_payment_confirmation_app));
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _)).Times(0);
  EXPECT_CALL(*mock_delegate, OnDoneCreatingPaymentApps()).Times(1);

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
  std::vector<gfx::Size> icon_sizes({{32, 32}});
  std::vector<SkBitmap> icon_bitmaps(1);
  icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/32);
  static_cast<content::TestWebContents*>(web_contents_.get())
      ->TestDidDownloadImage(icon, /*http_status_code=*/200,
                             std::move(icon_bitmaps), std::move(icon_sizes));

  ASSERT_TRUE(secure_payment_confirmation_app);
  EXPECT_FALSE(secure_payment_confirmation_app->HasEnrolledInstrument());
}

// Test that the SecurePaymentConfirmationApp can be created without credentials
// for the fallback flow, with HasEnrolledInstrument false.
TEST_F(SecurePaymentConfirmationAppFactoryUxRefreshTest,
       Fallback_NoCredentials) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  GURL icon = method_data->secure_payment_confirmation->instrument->icon;

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));
  std::unique_ptr<PaymentApp> secure_payment_confirmation_app;
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_))
      .WillOnce(MoveArg<0>(&secure_payment_confirmation_app));
  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _)).Times(0);
  EXPECT_CALL(*mock_delegate, OnDoneCreatingPaymentApps()).Times(1);

  EXPECT_CALL(*mock_credential_finder_, GetMatchingCredentials)
      .WillOnce(RunOnceCallback<5>(NoMatchingCredentials()));

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
  std::vector<gfx::Size> icon_sizes({{32, 32}});
  std::vector<SkBitmap> icon_bitmaps(1);
  icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/32);
  static_cast<content::TestWebContents*>(web_contents_.get())
      ->TestDidDownloadImage(icon, /*http_status_code=*/200,
                             std::move(icon_bitmaps), std::move(icon_sizes));

  ASSERT_TRUE(secure_payment_confirmation_app);
  EXPECT_FALSE(secure_payment_confirmation_app->HasEnrolledInstrument());
}

// Test that a the app is not created when the PaymentRequestSpec becomes null
// just prior to downloads finishing.
TEST_F(
    SecurePaymentConfirmationAppFactoryUxRefreshTest,
    SecureConfirmationPaymentRequest_WhenMissingPaymentRequestSpecDuringDownload) {
  url::Origin caller_origin = url::Origin::Create(GURL("https://site.example"));
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->details =
      "instrument details";
  std::vector<std::vector<uint8_t>> credential_ids =
      method_data->secure_payment_confirmation->credential_ids;
  ASSERT_EQ(credential_ids.size(), 1u);
  GURL icon = method_data->secure_payment_confirmation->instrument->icon;

  std::unique_ptr<MockPaymentAppFactoryDelegate> mock_delegate =
      CreateMockDelegate(std::move(method_data));
  EXPECT_CALL(*mock_delegate, GetFrameSecurityOrigin())
      .WillOnce(ReturnRef(caller_origin));

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreated(_)).Times(0);
  EXPECT_CALL(*mock_delegate, OnDoneCreatingPaymentApps());

  // Mock out not having credential, so that the flag checks are relevant.
  EXPECT_CALL(*mock_credential_finder_, GetMatchingCredentials)
      .WillOnce(RunOnceCallback<5>(NoMatchingCredentials()));

  secure_payment_confirmation_app_factory_->Create(mock_delegate->GetWeakPtr());
  std::vector<gfx::Size> icon_sizes({{32, 32}});
  std::vector<SkBitmap> icon_bitmaps(1);
  icon_bitmaps[0].allocN32Pixels(/*width=*/32, /*height=*/32);

  mock_delegate->ResetSpec();
  static_cast<content::TestWebContents*>(web_contents_.get())
      ->TestDidDownloadImage(icon, /*http_status_code=*/200,
                             std::move(icon_bitmaps), std::move(icon_sizes));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace payments
