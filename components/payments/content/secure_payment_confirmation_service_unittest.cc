// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_service.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key.h"
#include "components/payments/content/browser_binding/fake_browser_bound_key_store.h"
#include "components/payments/content/mock_web_payments_web_data_service.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "components/webauthn/core/browser/mock_internal_authenticator.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace payments {

using ::testing::_;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Pointer;

using payments::mojom::SecurePaymentConfirmationAvailabilityEnum;

namespace {

struct SecurePaymentConfirmationServiceDeleter {
  void operator()(SecurePaymentConfirmationService* spc_service) {
    spc_service->ResetAndDeleteThis();
  }
};

#if !BUILDFLAG(IS_IOS)
static const int32_t kAlgorithmIdentifier = 1;
static const int32_t kAnotherAlgorithmIdentifier = 2;

constexpr bool is_win = !!BUILDFLAG(IS_WIN);

#endif

}  // namespace

// Base class for unittests testing SecurePaymentConfirmationService, which
// provides general test environment setup and management.
//
// Tests that derive from this class should setup their feature flags as needed,
// and then call `InitializeSecurePaymentConfirmationService` to create the SPC
// service under test.
class SecurePaymentConfirmationServiceTestBase {
 public:
  SecurePaymentConfirmationServiceTestBase() = default;

 protected:
  void InitializeSecurePaymentConfirmationService(
      bool with_authenticator = true,
      bool is_off_the_record = false) {
    context_.set_is_off_the_record(is_off_the_record);
    web_contents_ = web_contents_factory_.CreateWebContents(&context_);
    CHECK(!mock_internal_authenticator_);
    CHECK(!spc_service_);

    mojo::PendingRemote<mojom::SecurePaymentConfirmationService> remote;
    mojo::PendingReceiver<mojom::SecurePaymentConfirmationService> receiver =
        remote.InitWithNewPipeAndPassReceiver();
    spc_service_ = std::unique_ptr<SecurePaymentConfirmationService,
                                   SecurePaymentConfirmationServiceDeleter>(
        new SecurePaymentConfirmationService(
            *web_contents_->GetPrimaryMainFrame(),
            /*receiver=*/std::move(receiver), mock_web_data_service_,
            with_authenticator ? CreateMockInternalAuthenticator() : nullptr,
            /*browser_bound_key_store_keychain_access_group=*/""));
  }

  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateMockInternalAuthenticator() {
    mock_internal_authenticator_ =
        new webauthn::MockInternalAuthenticator(web_contents_);
    return base::WrapUnique(static_cast<webauthn::InternalAuthenticator*>(
        &*mock_internal_authenticator_));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  scoped_refptr<payments::MockWebPaymentsWebDataService>
      mock_web_data_service_ =
          base::MakeRefCounted<MockWebPaymentsWebDataService>();
  // The `spc_service_` must be deleted after `mock_internal_authenticator_`, as
  // it owns the underlying std::unique_ptr.
  std::unique_ptr<SecurePaymentConfirmationService,
                  SecurePaymentConfirmationServiceDeleter>
      spc_service_;
  raw_ptr<webauthn::MockInternalAuthenticator> mock_internal_authenticator_;
  base::MockCallback<mojom::SecurePaymentConfirmationService::
                         SecurePaymentConfirmationAvailabilityCallback>
      mock_secure_payment_confirmation_availability_callback_;
};

class SecurePaymentConfirmationServiceTest
    : public SecurePaymentConfirmationServiceTestBase,
      public ::testing::Test {};

TEST_F(SecurePaymentConfirmationServiceTest,
       SecurePaymentConfirmationAvailabilityAPI) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {::features::kSecurePaymentConfirmation,
       features::kSecurePaymentConfirmationUseCredentialStoreAPIs},
      {});

  InitializeSecurePaymentConfirmationService();

  EXPECT_CALL(*mock_internal_authenticator_,
              IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_internal_authenticator_,
              IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(true));

  EXPECT_CALL(mock_secure_payment_confirmation_availability_callback_,
              Run(SecurePaymentConfirmationAvailabilityEnum::kAvailable));
  spc_service_->SecurePaymentConfirmationAvailability(
      mock_secure_payment_confirmation_availability_callback_.Get());
}

TEST_F(SecurePaymentConfirmationServiceTest,
       SecurePaymentConfirmationAvailabilityAPI_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {}, {::features::kSecurePaymentConfirmation,
           features::kSecurePaymentConfirmationUseCredentialStoreAPIs});

  InitializeSecurePaymentConfirmationService();

  EXPECT_CALL(mock_secure_payment_confirmation_availability_callback_,
              Run(SecurePaymentConfirmationAvailabilityEnum::
                      kUnavailableFeatureNotEnabled));
  spc_service_->SecurePaymentConfirmationAvailability(
      mock_secure_payment_confirmation_availability_callback_.Get());
}

TEST_F(
    SecurePaymentConfirmationServiceTest,
    SecurePaymentConfirmationAvailabilityAPI_SecurePaymentConfirmationDebugMode) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {::features::kSecurePaymentConfirmation,
       features::kSecurePaymentConfirmationUseCredentialStoreAPIs,
       ::features::kSecurePaymentConfirmationDebug},
      {});

  InitializeSecurePaymentConfirmationService(/*with_authenticator=*/false);

  // Here we haven't set up the authenticator, but since the debug flag is set
  // that does not matter; the API should still return true.
  EXPECT_CALL(mock_secure_payment_confirmation_availability_callback_,
              Run(SecurePaymentConfirmationAvailabilityEnum::kAvailable));
  spc_service_->SecurePaymentConfirmationAvailability(
      mock_secure_payment_confirmation_availability_callback_.Get());
}

TEST_F(SecurePaymentConfirmationServiceTest,
       SecurePaymentConfirmationAvailabilityAPI_NoAuthenticator) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {::features::kSecurePaymentConfirmation,
       features::kSecurePaymentConfirmationUseCredentialStoreAPIs},
      {});

  InitializeSecurePaymentConfirmationService(/*with_authenticator=*/false);

  EXPECT_CALL(mock_secure_payment_confirmation_availability_callback_,
              Run(SecurePaymentConfirmationAvailabilityEnum::
                      kUnavailableUnknownReason));
  spc_service_->SecurePaymentConfirmationAvailability(
      mock_secure_payment_confirmation_availability_callback_.Get());
}

TEST_F(
    SecurePaymentConfirmationServiceTest,
    SecurePaymentConfirmationAvailabilityAPI_GetMatchingCredentialIdsNotSupported) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {::features::kSecurePaymentConfirmation,
       features::kSecurePaymentConfirmationUseCredentialStoreAPIs},
      {});

  InitializeSecurePaymentConfirmationService();

  EXPECT_CALL(*mock_internal_authenticator_,
              IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(mock_secure_payment_confirmation_availability_callback_,
              Run(SecurePaymentConfirmationAvailabilityEnum::
                      kUnavailableUnknownReason));
  spc_service_->SecurePaymentConfirmationAvailability(
      mock_secure_payment_confirmation_availability_callback_.Get());
}

TEST_F(
    SecurePaymentConfirmationServiceTest,
    SecurePaymentConfirmationAvailabilityAPI_AuthenticatorIsNotUserVerifying) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {::features::kSecurePaymentConfirmation,
       features::kSecurePaymentConfirmationUseCredentialStoreAPIs},
      {});

  InitializeSecurePaymentConfirmationService();

  EXPECT_CALL(*mock_internal_authenticator_,
              IsGetMatchingCredentialIdsSupported())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_internal_authenticator_,
              IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(false));

  EXPECT_CALL(mock_secure_payment_confirmation_availability_callback_,
              Run(SecurePaymentConfirmationAvailabilityEnum::
                      kUnavailableNoUserVerifyingPlatformAuthenticator));
  spc_service_->SecurePaymentConfirmationAvailability(
      mock_secure_payment_confirmation_availability_callback_.Get());
}

#if !BUILDFLAG(IS_IOS)

struct CredentialTestParams {
  // The algorithm identifier supported by the fake browser bound key store.
  FakeBrowserBoundKey fake_key;
  std::vector<device::PublicKeyCredentialParams::CredentialInfo>
      public_key_parameters;
  std::optional<std::vector<device::PublicKeyCredentialParams::CredentialInfo>>
      browser_bound_key_cred_params;
  std::vector<uint8_t> expected_signature;
  std::vector<uint8_t> expected_browser_bound_key;
  std::string test_description;
};

class SecurePaymentConfirmationServiceCredentialTest
    : public SecurePaymentConfirmationServiceTestBase,
      public ::testing::TestWithParam<CredentialTestParams> {
 public:
  SecurePaymentConfirmationServiceCredentialTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kSecurePaymentConfirmationBrowserBoundKeys);
  }

  void SetUpTest(bool is_off_the_record) {
    InitializeSecurePaymentConfirmationService(/*with_authenticator=*/true,
                                               is_off_the_record);
    auto passkey_browser_binder = std::make_unique<PasskeyBrowserBinder>(
        fake_browser_bound_key_store_, mock_web_data_service_);
    passkey_browser_binder->SetRandomBytesAsVectorCallbackForTesting(
        base::BindRepeating(
            [](size_t length) { return GetParam().fake_key.GetIdentifier(); }));
    spc_service_->SetPasskeyBrowserBinderForTesting(
        std::move(passkey_browser_binder));
  }

 protected:
  const std::vector<uint8_t> fake_challenge_ = {0x01, 0x02, 0x03, 0x04};
  const std::vector<uint8_t> fake_credential_id_ = {0x10, 0x11, 0x12, 0x13};
  const std::vector<uint8_t> fake_client_data_json_ = {0x30, 0x31, 0x32, 0x33};
  const std::string fake_relying_party_id_ = "relying-party.example";

  ::blink::mojom::PublicKeyCredentialCreationOptionsPtr
  GetPublicKeyCredentialCreationOptions() {
    auto creation_options =
        ::blink::mojom::PublicKeyCredentialCreationOptions::New();
    creation_options->relying_party.id = fake_relying_party_id_;
    creation_options->is_payment_credential_creation = true;
    creation_options->challenge = fake_challenge_;
    creation_options->public_key_parameters = GetParam().public_key_parameters;
    creation_options->payment_browser_bound_key_parameters =
        GetParam().browser_bound_key_cred_params;
    return creation_options;
  }

  scoped_refptr<FakeBrowserBoundKeyStore> fake_browser_bound_key_store_ =
      base::MakeRefCounted<FakeBrowserBoundKeyStore>();
  base::MockCallback<
      mojom::SecurePaymentConfirmationService::MakePaymentCredentialCallback>
      mock_payment_credential_callback_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

static ::testing::Matcher<
    ::blink::mojom::MakeCredentialAuthenticatorResponsePtr>
AuthenticatorResponseWithBrowserBoundSignature(std::vector<uint8_t> signature) {
  return Pointee(Field(
      "payment", &::blink::mojom::MakeCredentialAuthenticatorResponse::payment,
      Pointee(Field("browser_bound_signature",
                    &::blink::mojom::AuthenticationExtensionsPaymentResponse::
                        browser_bound_signature,
                    Eq(signature)))));
}

static ::testing::Matcher<
    ::blink::mojom::MakeCredentialAuthenticatorResponsePtr>
AuthenticatorResponseWithoutBrowserBoundSignature() {
  return Pointee(Field(
      "payment", &::blink::mojom::MakeCredentialAuthenticatorResponse::payment,
      AnyOf(Pointer(nullptr),
            Pointee(
                Field("browser_bound_signature",
                      &::blink::mojom::AuthenticationExtensionsPaymentResponse::
                          browser_bound_signature,
                      ElementsAre())))));
}

INSTANTIATE_TEST_SUITE_P(
    SecurePaymentConfirmationServiceCredentialTest,
    SecurePaymentConfirmationServiceCredentialTest,
    ::testing::Values<CredentialTestParams>(
        CredentialTestParams{
            .fake_key = FakeBrowserBoundKey(
                /*browser_bound_key_id=*/{0x60, 0x61, 0x62, 0x63},
                /*public_key_as_cose_key=*/{0x50, 0x51, 0x52, 0x53},
                /*signature=*/{0x20, 0x21, 0x22, 0x23},
                kAlgorithmIdentifier,
                /*expected_client_data=*/{0x30, 0x31, 0x32, 0x33}),
            .public_key_parameters =
                {device::PublicKeyCredentialParams::CredentialInfo(
                    device::CredentialType::kPublicKey,
                    kAnotherAlgorithmIdentifier)},
            .browser_bound_key_cred_params =
                {{device::PublicKeyCredentialParams::CredentialInfo(
                    device::CredentialType::kPublicKey,
                    kAlgorithmIdentifier)}},
            .expected_signature = {0x20, 0x21, 0x22, 0x23},
            .expected_browser_bound_key = {0x50, 0x51, 0x52, 0x53},
            .test_description = "UsingPasskeyPubKeyCredParams",
        },
        CredentialTestParams{
            .fake_key = FakeBrowserBoundKey(
                /*browser_bound_key_id=*/{0x60, 0x61, 0x62, 0x63},
                /*public_key_as_cose_key=*/{0x50, 0x51, 0x52, 0x53},
                /*signature=*/{0x20, 0x21, 0x22, 0x23},
                kAlgorithmIdentifier,
                /*expected_client_data=*/{0x30, 0x31, 0x32, 0x33}),
            .public_key_parameters =
                {device::PublicKeyCredentialParams::CredentialInfo(
                    device::CredentialType::kPublicKey,
                    kAlgorithmIdentifier)},
            .browser_bound_key_cred_params = std::nullopt,
            .expected_signature = {0x20, 0x21, 0x22, 0x23},
            .expected_browser_bound_key = {0x50, 0x51, 0x52, 0x53},
            .test_description = "UsingBrowserBoundPubKeyCredParams",
        }),
    [](const ::testing::TestParamInfo<CredentialTestParams>& info) {
      return info.param.test_description;
    });

TEST_P(SecurePaymentConfirmationServiceCredentialTest,
       MakePaymentCredentialAddsBrowserBoundKey) {
  base::RunLoop run_loop;

  // Set up the SPC service in the test as otherwise, the RenderFrameHost
  // prematurely closes during run_loop.Run() when there are multiple threads.
  context_.set_is_off_the_record(false);
  web_contents_ = web_contents_factory_.CreateWebContents(&context_);

  mojo::PendingRemote<mojom::SecurePaymentConfirmationService> remote;
  mojo::PendingReceiver<mojom::SecurePaymentConfirmationService> receiver =
      remote.InitWithNewPipeAndPassReceiver();
  spc_service_ = std::unique_ptr<SecurePaymentConfirmationService,
                                 SecurePaymentConfirmationServiceDeleter>(
      new SecurePaymentConfirmationService(
          *web_contents_->GetPrimaryMainFrame(),
          /*receiver=*/std::move(receiver), mock_web_data_service_,
          CreateMockInternalAuthenticator(),
          /*browser_bound_key_store_keychain_access_group=*/""));

  auto passkey_browser_binder = std::make_unique<PasskeyBrowserBinder>(
      fake_browser_bound_key_store_, mock_web_data_service_);
  passkey_browser_binder->SetRandomBytesAsVectorCallbackForTesting(
      base::BindRepeating(
          [](size_t length) { return GetParam().fake_key.GetIdentifier(); }));
  spc_service_->SetPasskeyBrowserBinderForTesting(
      std::move(passkey_browser_binder));

  fake_browser_bound_key_store_->PutFakeKey(GetParam().fake_key);
  ::blink::mojom::PublicKeyCredentialCreationOptionsPtr creation_options =
      GetPublicKeyCredentialCreationOptions();
  auto fake_authenticator_response =
      ::blink::mojom::MakeCredentialAuthenticatorResponse::New();
  fake_authenticator_response->info =
      ::blink::mojom::CommonCredentialInfo::New();
  fake_authenticator_response->info->raw_id = fake_credential_id_;
  fake_authenticator_response->info->client_data_json = fake_client_data_json_;

  // Last used time is only set on Windows platform.
  std::optional<base::Time> last_used =
      is_win ? std::optional<base::Time>(base::Time::NowFromSystemTime())
             : std::nullopt;

  EXPECT_CALL(
      *mock_web_data_service_,
      SetBrowserBoundKey(fake_credential_id_, fake_relying_party_id_,
                         GetParam().fake_key.GetIdentifier(), last_used, _));

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
          [&fake_authenticator_response, &run_loop](
              ::blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
              ::blink::mojom::Authenticator::MakeCredentialCallback callback) {
            std::move(callback).Run(
                ::blink::mojom::AuthenticatorStatus::SUCCESS,
                fake_authenticator_response.Clone(),
                /*exception_details=*/nullptr);
            run_loop.Quit();
          });
  EXPECT_CALL(mock_payment_credential_callback_,
              Run(Eq(::blink::mojom::AuthenticatorStatus::SUCCESS),
                  AuthenticatorResponseWithBrowserBoundSignature(
                      GetParam().expected_signature),
                  _));

  spc_service_->MakePaymentCredential(creation_options.Clone(),
                                      mock_payment_credential_callback_.Get());
  run_loop.Run();

  ASSERT_FALSE(actual_payment_options.is_null());
  EXPECT_EQ(actual_payment_options->browser_bound_public_key,
            GetParam().expected_browser_bound_key);
}

TEST_P(SecurePaymentConfirmationServiceCredentialTest,
       MakePaymentCredentialDoesNotAddBrowserBoundKeyWhenOffTheRecord) {
  base::RunLoop run_loop;
  SetUpTest(/*is_off_the_record=*/true);
  fake_browser_bound_key_store_->PutFakeKey(GetParam().fake_key);
  ::blink::mojom::PublicKeyCredentialCreationOptionsPtr creation_options =
      GetPublicKeyCredentialCreationOptions();
  auto fake_authenticator_response =
      ::blink::mojom::MakeCredentialAuthenticatorResponse::New();
  fake_authenticator_response->info =
      ::blink::mojom::CommonCredentialInfo::New();
  fake_authenticator_response->info->raw_id = fake_credential_id_;
  fake_authenticator_response->info->client_data_json = fake_client_data_json_;

  EXPECT_CALL(*mock_web_data_service_, SetBrowserBoundKey).Times(0);
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
  EXPECT_CALL(mock_payment_credential_callback_,
              Run(Eq(::blink::mojom::AuthenticatorStatus::SUCCESS),
                  AuthenticatorResponseWithoutBrowserBoundSignature(), _))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  spc_service_->MakePaymentCredential(creation_options.Clone(),
                                      mock_payment_credential_callback_.Get());
  run_loop.Run();

  ASSERT_FALSE(actual_payment_options.is_null());
  EXPECT_FALSE(actual_payment_options->browser_bound_public_key.has_value());
}

#endif  // !BUILDFLAG(IS_IOS)

}  // namespace payments
