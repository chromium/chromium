// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_web_authentication_delegate.h"
#include "chrome/browser/webauthn/fake_password_credential_fetcher.h"
#include "chrome/browser/webauthn/immediate_request_rate_limiter_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/password_credential_fetcher.h"
#include "chrome/browser/webauthn/password_credential_ui_controller.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/immediate_request_rate_limiter.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/test/base/testing_profile.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

static constexpr char kRpId[] = "example.com";
static constexpr char kOrigin[] = "https://example.com";

constexpr int kRequestPassword =
    static_cast<int>(blink::mojom::CredentialTypeFlags::kPassword);
constexpr int kRequestPublicKey =
    static_cast<int>(blink::mojom::CredentialTypeFlags::kPublicKey);

using TransportAvailabilityInfo =
    device::FidoRequestHandlerBase::TransportAvailabilityInfo;
using UIPresentation =
    content::AuthenticatorRequestClientDelegate::UIPresentation;

class Observer : public testing::NiceMock<
                     ChromeAuthenticatorRequestDelegate::TestObserver> {
 public:
  MOCK_METHOD(void,
              Created,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
  MOCK_METHOD(void,
              OnTransportAvailabilityEnumerated,
              (ChromeAuthenticatorRequestDelegate * delegate,
               TransportAvailabilityInfo* tai),
              (override));
  MOCK_METHOD(void,
              UIShown,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
  MOCK_METHOD(void,
              CableV2ExtensionSeen,
              (base::span<const uint8_t> server_link_data),
              (override));
};

class MockPasswordCredentialUIController
    : public PasswordCredentialUIController {
 public:
  MockPasswordCredentialUIController(
      content::GlobalRenderFrameHostId render_frame_host_id,
      AuthenticatorRequestDialogModel* model)
      : PasswordCredentialUIController(render_frame_host_id, model) {}

  MOCK_METHOD(
      void,
      SetPasswordSelectedCallback,
      (content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback),
      (override));
};

class MockCableDiscoveryFactory : public device::FidoDiscoveryFactory {
 public:
  void set_cable_data(
      device::FidoRequestType request_type,
      std::vector<device::CableDiscoveryData> data,
      const std::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
          qr_generator_key) override {
    cable_data = std::move(data);
    qr_key = qr_generator_key;
  }

  std::vector<device::CableDiscoveryData> cable_data;
  std::optional<std::array<uint8_t, device::cablev2::kQRKeySize>> qr_key;
};

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeAuthenticatorRequestDelegateTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PasskeyModelFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer_);

    ImmediateRequestRateLimiterFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<webauthn::ImmediateRequestRateLimiter>();
        }));
  }

  void TearDown() override {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  Observer observer_;
};

class TestAuthenticatorModelObserver final
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit TestAuthenticatorModelObserver(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    last_step_ = model_->step();
  }
  ~TestAuthenticatorModelObserver() override {
    if (model_) {
      model_->observers.RemoveObserver(this);
    }
  }

  AuthenticatorRequestDialogModel::Step last_step() { return last_step_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { last_step_ = model_->step(); }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  AuthenticatorRequestDialogModel::Step last_step_;
};

TEST_F(ChromeAuthenticatorRequestDelegateTest, CableConfiguration) {
  const std::array<uint8_t, 16> eid = {1, 2, 3, 4};
  const std::array<uint8_t, 32> prekey = {5, 6, 7, 8};
  const device::CableDiscoveryData v1_extension(
      device::CableDiscoveryData::Version::V1, eid, eid, prekey);

  device::CableDiscoveryData v2_extension;
  v2_extension.version = device::CableDiscoveryData::Version::V2;
  v2_extension.v2.emplace(std::vector<uint8_t>(prekey.begin(), prekey.end()),
                          std::vector<uint8_t>());

  enum class Result {
    kNone,
    kV1,
    kServerLink,
    k3rdParty,
  };

#if BUILDFLAG(IS_LINUX)
  // On Linux, some configurations aren't supported because of bluez
  // limitations. This macro maps the expected result in that case.
#define NONE_ON_LINUX(r) (Result::kNone)
#else
#define NONE_ON_LINUX(r) (r)
#endif

  const struct {
    const char* origin;
    std::vector<device::CableDiscoveryData> extensions;
    device::FidoRequestType request_type;
    std::optional<device::ResidentKeyRequirement> resident_key_requirement;
    Result expected_result;
  } kTests[] = {
      {
          "https://example.com",
          {},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v1_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v2_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
      },
      {
          // a.g.c should still be able to get 3rd-party caBLE
          // if it doesn't send an extension in an assertion request.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::k3rdParty,
      },
      {
          // ... but not for non-discoverable registration.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kDiscouraged,
          Result::kNone,
      },
      {
          // ... but yes for rk=preferred
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kPreferred,
          Result::k3rdParty,
      },
      {
          // ... or rk=required.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kRequired,
          Result::k3rdParty,
      },
      {
          "https://accounts.google.com",
          {v1_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          NONE_ON_LINUX(Result::kV1),
      },
      {
          "https://accounts.google.com",
          {v2_extension},
          device::FidoRequestType::kGetAssertion,
          std::nullopt,
          Result::kServerLink,
      },
  };

  unsigned test_case = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_case);
    test_case++;

    MockCableDiscoveryFactory discovery_factory;
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(kRpId);
    delegate.ConfigureDiscoveries(
        url::Origin::Create(GURL(test.origin)), test.origin,
        content::AuthenticatorRequestClientDelegate::RequestSource::
            kWebAuthentication,
        test.request_type, test.resident_key_requirement,
        device::UserVerificationRequirement::kRequired,
        /*user_name=*/std::nullopt, test.extensions,
        /*is_enclave_authenticator_available=*/false, &discovery_factory);

    switch (test.expected_result) {
      case Result::kNone:
        EXPECT_FALSE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.cable_data.empty());
        break;

      case Result::kV1:
        EXPECT_FALSE(discovery_factory.qr_key.has_value());
        EXPECT_FALSE(discovery_factory.cable_data.empty());
        EXPECT_EQ(delegate.dialog_model()->cable_ui_type,
                  AuthenticatorRequestDialogModel::CableUIType::CABLE_V1);
        break;

      case Result::kServerLink:
        EXPECT_TRUE(discovery_factory.qr_key.has_value());
        EXPECT_FALSE(discovery_factory.cable_data.empty());
        EXPECT_EQ(
            delegate.dialog_model()->cable_ui_type,
            AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK);
        break;

      case Result::k3rdParty:
        EXPECT_TRUE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.cable_data.empty());
        EXPECT_EQ(
            delegate.dialog_model()->cable_ui_type,
            AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR);
        break;
    }
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, NoExtraDiscoveriesWithoutUI) {
  for (const bool disable_ui : {false, true}) {
    SCOPED_TRACE(disable_ui);

    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(kRpId);
    if (disable_ui) {
      delegate.SetUIPresentation(UIPresentation::kDisabled);
    }
    MockCableDiscoveryFactory discovery_factory;
    delegate.ConfigureDiscoveries(
        url::Origin::Create(GURL(kOrigin)), kOrigin,
        content::AuthenticatorRequestClientDelegate::RequestSource::
            kWebAuthentication,
        device::FidoRequestType::kMakeCredential,
        device::ResidentKeyRequirement::kPreferred,
        device::UserVerificationRequirement::kRequired,
        /*user_name=*/std::nullopt, {},
        /*is_enclave_authenticator_available=*/false, &discovery_factory);

    EXPECT_EQ(discovery_factory.qr_key.has_value(), !disable_ui);

    // `discovery_factory.nswindow_` won't be set in any case because it depends
    // on the `RenderFrameHost` having a `BrowserWindow`, which it doesn't in
    // this context.
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, ConditionalUI) {
  // The RenderFrame has to be live for the ChromeWebAuthnCredentialsDelegate to
  // be created.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com"));
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());

  // Enabling conditional mode should cause the modal dialog to stay hidden at
  // the beginning of a request. An omnibar icon might be shown instead.
  for (bool conditional_ui : {true, false}) {
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetUIPresentation(conditional_ui ? UIPresentation::kAutofill
                                              : UIPresentation::kModal);
    delegate.SetRelyingPartyId(kRpId);
    AuthenticatorRequestDialogModel* model = delegate.dialog_model();
    TestAuthenticatorModelObserver observer(model);
    model->observers.AddObserver(&observer);
    EXPECT_EQ(observer.last_step(),
              AuthenticatorRequestDialogModel::Step::kNotStarted);
    TransportAvailabilityInfo transports_info;
    transports_info.request_type = device::FidoRequestType::kGetAssertion;
    delegate.OnTransportAvailabilityEnumerated(std::move(transports_info));
    EXPECT_EQ(observer.last_step() ==
                  AuthenticatorRequestDialogModel::Step::kPasskeyAutofill,
              conditional_ui);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, FilterGoogleComPasskeys) {
  auto HasCreds = device::FidoRequestHandlerBase::RecognizedCredential::
      kHasRecognizedCredential;
  auto NoCreds = device::FidoRequestHandlerBase::RecognizedCredential::
      kNoRecognizedCredential;
  auto UnknownCreds =
      device::FidoRequestHandlerBase::RecognizedCredential::kUnknown;
  constexpr char kGoogle[] = "google.com";
  constexpr char kOtherRpId[] = "example.com";
  struct {
    std::string rp_id;
    device::FidoRequestHandlerBase::RecognizedCredential recognized_credential;
    std::vector<std::string> user_ids;

    device::FidoRequestHandlerBase::RecognizedCredential
        expected_recognized_credential;
    std::vector<std::string> expected_user_ids;
  } kTests[] = {
      {kOtherRpId,
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2"},
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2"}},
      {kGoogle,
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2", "AUTOFILL_AUTH:c3"},
       HasCreds,
       {"GOOGLE_ACCOUNT:c1"}},
      {kGoogle, HasCreds, {"c2", "AUTOFILL_AUTH:c3"}, NoCreds, {}},
      {kGoogle, UnknownCreds, {}, UnknownCreds, {}},
      {kGoogle, HasCreds, {}, HasCreds, {}},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message() << "rp_id=" << test.rp_id);
    SCOPED_TRACE(::testing::Message()
                 << "creds=" << base::JoinString(test.user_ids, ","));
    TransportAvailabilityInfo data;
    data.has_empty_allow_list = true;
    data.request_type = device::FidoRequestType::kGetAssertion;
    TransportAvailabilityInfo result;
    EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
        .WillOnce([&result](const auto* _, const auto* new_tai) {
          result = std::move(*new_tai);
        });

    for (const std::string& user_id : test.user_ids) {
      data.recognized_credentials.emplace_back(
          device::AuthenticatorType::kOther, test.rp_id,
          std::vector<uint8_t>{0},
          device::PublicKeyCredentialUserEntity(
              std::vector<uint8_t>(user_id.begin(), user_id.end())),
          /*provider_name=*/std::nullopt);
    }
    data.has_platform_authenticator_credential = test.recognized_credential;

    // Mix in an icloud keychain credential. These should not be filtered or
    // affect setting the recognized credentials flag.
    data.recognized_credentials.emplace_back(
        device::AuthenticatorType::kICloudKeychain, test.rp_id,
        std::vector<uint8_t>{0}, device::PublicKeyCredentialUserEntity({1}),
        /*provider_name=*/std::nullopt);
    data.has_icloud_keychain_credential = device::FidoRequestHandlerBase::
        RecognizedCredential::kHasRecognizedCredential;

    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(test.rp_id);
    delegate.RegisterActionCallbacks(
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing(), base::DoNothing(), base::DoNothing());
    delegate.OnTransportAvailabilityEnumerated(std::move(data));

    EXPECT_EQ(result.has_platform_authenticator_credential,
              test.expected_recognized_credential);
    EXPECT_EQ(result.has_icloud_keychain_credential,
              device::FidoRequestHandlerBase::RecognizedCredential::
                  kHasRecognizedCredential);
    ASSERT_EQ(result.recognized_credentials.size(),
              test.expected_user_ids.size() + 1);
    for (size_t i = 0; i < test.expected_user_ids.size(); ++i) {
      std::string expected_id = test.expected_user_ids[i];
      EXPECT_EQ(result.recognized_credentials[i].user.id,
                std::vector<uint8_t>(expected_id.begin(), expected_id.end()));
    }
    EXPECT_EQ(result.recognized_credentials.back().source,
              device::AuthenticatorType::kICloudKeychain);
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       FilterGoogleComPasskeysWithNonEmptyAllowList) {
  // Regression test for crbug.com/40071851, b/366128135.
  constexpr char kGoogleRpId[] = "google.com";
  TransportAvailabilityInfo data;
  data.has_empty_allow_list = false;
  data.request_type = device::FidoRequestType::kGetAssertion;
  TransportAvailabilityInfo result;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&result](const auto* _, const auto* new_tai) {
        result = std::move(*new_tai);
      });

  // User ID doesn't start with the `GOOGLE_ACCOUNT:` prefix that distinguishes
  // them as suitable for login auth.
  std::string user_id = "test user id";
  data.recognized_credentials.emplace_back(
      device::AuthenticatorType::kOther, kGoogleRpId, std::vector<uint8_t>{0},
      device::PublicKeyCredentialUserEntity(
          std::vector<uint8_t>(user_id.begin(), user_id.end())),
      /*provider_name=*/std::nullopt);
  data.has_platform_authenticator_credential = device::FidoRequestHandlerBase::
      RecognizedCredential::kHasRecognizedCredential;

  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetRelyingPartyId(kGoogleRpId);
  delegate.RegisterActionCallbacks(
      base::DoNothing(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing());
  delegate.OnTransportAvailabilityEnumerated(std::move(data));

  // Despite lacking the user ID prefix, credentials are not filtered from
  // `recognized_credentials` because the request has a non-empty allow list.
  // The RecognizedCredential status isn't adjusted either.
  ASSERT_EQ(result.recognized_credentials.size(), 1u);
  EXPECT_EQ(result.has_platform_authenticator_credential,
            device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential);
  testing::Mock::VerifyAndClearExpectations(&observer_);
}

class EnclaveAuthenticatorRequestDelegateTest
    : public ChromeAuthenticatorRequestDelegateTest {
 public:
  void SetUp() override {
    ChromeAuthenticatorRequestDelegateTest::SetUp();
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }
};

TEST_F(EnclaveAuthenticatorRequestDelegateTest,
       BrowserProvidedPasskeysAvailable) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "hikari@example.com",
                                      signin::ConsentLevel::kSignin);
  struct {
    bool is_syncing_passwords;
    bool has_unexportable_keys;
    bool expected_passkeys_available;
  } kTestCases[] = {
      // sync unexp result
      {true, true, true},
      {false, true, false},
      {true, false, false},
  };
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "is_syncing_passwords=" << test.is_syncing_passwords);
    SCOPED_TRACE(testing::Message()
                 << "has_unexportable_keys=" << test.has_unexportable_keys);
    ChromeWebAuthenticationDelegate delegate;

    auto* test_sync_service = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->GetForProfile(profile()));
    test_sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPasswords, test.is_syncing_passwords);

    std::variant<crypto::ScopedNullUnexportableKeyProvider,
                 crypto::ScopedFakeUnexportableKeyProvider>
        unexportable_key_provider;
    if (test.has_unexportable_keys) {
      unexportable_key_provider
          .emplace<crypto::ScopedFakeUnexportableKeyProvider>();
    }

    base::test::TestFuture<bool> future;
    delegate.BrowserProvidedPasskeysAvailable(browser_context(),
                                              future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(future.Get(), test.expected_passkeys_available);
  }
}

// This test is separated from BrowserProvidedPasskeysAvailable because ChromeOS
// does not support clearing the primary account once Chrome is running.
TEST_F(EnclaveAuthenticatorRequestDelegateTest,
       BrowserProvidedPasskeysAvailable_NoAccount) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  auto* test_sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->GetForProfile(profile()));
  test_sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);
  crypto::ScopedFakeUnexportableKeyProvider unexportable_key_provider;

  {
    base::test::TestFuture<bool> future;
    ChromeWebAuthenticationDelegate delegate;
    delegate.BrowserProvidedPasskeysAvailable(browser_context(),
                                              future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_FALSE(future.Get());
  }
  signin::MakePrimaryAccountAvailable(identity_manager, "hikari@example.com",
                                      signin::ConsentLevel::kSignin);
  {
    base::test::TestFuture<bool> future;
    ChromeWebAuthenticationDelegate delegate;
    delegate.BrowserProvidedPasskeysAvailable(browser_context(),
                                              future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get());
  }
}

// Regression test for crbug.com/377724726.
// Tests that being signed in is enough to have
// BrowserProvidedPasskeysAvailable() return true. Sync-the-feature should not
// be necessary as long as the user consented to using passwords and passkeys
// from their Google account.
TEST_F(EnclaveAuthenticatorRequestDelegateTest,
       BrowserProvidedPasskeysAvailableForSignedInUsers) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "hikari@example.com",
                                      signin::ConsentLevel::kSignin);
  auto* test_sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->GetForProfile(profile()));

  // ConsentLevel::kSignin + passwords syncing should be enough.
  test_sync_service->SetSignedIn(signin::ConsentLevel::kSignin);
  test_sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);
  crypto::ScopedFakeUnexportableKeyProvider unexportable_key_provider;

  base::test::TestFuture<bool> future;
  ChromeWebAuthenticationDelegate delegate;
  delegate.BrowserProvidedPasskeysAvailable(browser_context(),
                                            future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
}

#if BUILDFLAG(IS_MAC)
std::string TouchIdMetadataSecret(ChromeWebAuthenticationDelegate& delegate,
                                  content::BrowserContext* browser_context) {
  return delegate.GetTouchIdAuthenticatorConfig(browser_context)
      ->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  ChromeWebAuthenticationDelegate delegate;
  std::string secret = TouchIdMetadataSecret(delegate, GetBrowserContext());
  EXPECT_EQ(secret.size(), 32u);
  // The secret should be stable.
  EXPECT_EQ(secret, TouchIdMetadataSecret(delegate, GetBrowserContext()));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  // Different delegates on the same BrowserContext (Profile) should return
  // the same secret.
  ChromeWebAuthenticationDelegate delegate1;
  ChromeWebAuthenticationDelegate delegate2;
  EXPECT_EQ(TouchIdMetadataSecret(delegate1, GetBrowserContext()),
            TouchIdMetadataSecret(delegate2, GetBrowserContext()));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  // Different profiles have different secrets.
  auto other_browser_context = CreateBrowserContext();
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_NE(TouchIdMetadataSecret(delegate, GetBrowserContext()),
            TouchIdMetadataSecret(delegate, other_browser_context.get()));
  // Ensure this second secret is actually valid.
  EXPECT_EQ(
      32u, TouchIdMetadataSecret(delegate, other_browser_context.get()).size());
}

#endif  // BUILDFLAG(IS_MAC)

class ChromeAuthenticatorRequestDelegateTestWithPassword
    : public ChromeAuthenticatorRequestDelegateTest,
      public testing::WithParamInterface<bool> {};

TEST_P(ChromeAuthenticatorRequestDelegateTestWithPassword, DiscoverPasswords) {
  bool enable_password = GetParam();
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kOrigin));
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  auto* password_fetcher = new FakePasswordCredentialFetcher(main_rfh());
  PasswordCredentialFetcher::SetInstanceForTesting(password_fetcher);
  auto password_ui_controller =
      std::make_unique<testing::NiceMock<MockPasswordCredentialUIController>>(
          main_rfh()->GetGlobalId(), delegate.dialog_model());
  delegate.SetPasswordUIControllerForTesting(std::move(password_ui_controller));
  delegate.SetUIPresentation(enable_password ? UIPresentation::kModalImmediate
                                             : UIPresentation::kModal);
  delegate.SetCredentialTypes((enable_password
                                   ? (kRequestPassword | kRequestPublicKey)
                                   : (kRequestPublicKey)));
  delegate.SetRelyingPartyId(kRpId);
  MockCableDiscoveryFactory discovery_factory;

  delegate.ConfigureDiscoveries(url::Origin::Create(GURL(kOrigin)), kOrigin,
                                content::AuthenticatorRequestClientDelegate::
                                    RequestSource::kWebAuthentication,
                                device::FidoRequestType::kGetAssertion,
                                device::ResidentKeyRequirement::kPreferred,
                                device::UserVerificationRequirement::kRequired,
                                /*user_name=*/std::nullopt, {},
                                /*is_enclave_authenticator_available=*/false,
                                &discovery_factory);
  EXPECT_EQ(password_fetcher->fetch_passwords_called(), enable_password);

  PasswordCredentialFetcher::SetInstanceForTesting(nullptr);
  // when passwords are not requested, the fetcher is not used.
  if (!enable_password) {
    delete password_fetcher;
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeAuthenticatorRequestDelegateTestWithPassword,
                         testing::Bool());

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TryToShowUiNoImmediateCredentials) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kOrigin));
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  auto* password_fetcher = new FakePasswordCredentialFetcher(main_rfh());
  PasswordCredentialFetcher::SetInstanceForTesting(password_fetcher);
  auto password_ui_controller =
      std::make_unique<testing::NiceMock<MockPasswordCredentialUIController>>(
          main_rfh()->GetGlobalId(), delegate.dialog_model());
  delegate.SetPasswordUIControllerForTesting(std::move(password_ui_controller));
  base::MockCallback<base::OnceClosure> mock_closure;
  delegate.RegisterActionCallbacks(
      base::DoNothing(), mock_closure.Get(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing());
  delegate.SetUIPresentation(UIPresentation::kModalImmediate);
  delegate.SetCredentialTypes(kRequestPassword | kRequestPublicKey);
  delegate.SetRelyingPartyId(kRpId);
  MockCableDiscoveryFactory discovery_factory;
  PasswordCredentialFetcher::PasswordCredentialsReceivedCallback callback;
  delegate.ConfigureDiscoveries(url::Origin::Create(GURL(kOrigin)), kOrigin,
                                content::AuthenticatorRequestClientDelegate::
                                    RequestSource::kWebAuthentication,
                                device::FidoRequestType::kGetAssertion,
                                device::ResidentKeyRequirement::kPreferred,
                                device::UserVerificationRequirement::kRequired,
                                /*user_name=*/std::nullopt, {},
                                /*is_enclave_authenticator_available=*/false,
                                &discovery_factory);
  TransportAvailabilityInfo transports_info;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;

  // still waiting for passwords.
  EXPECT_CALL(mock_closure, Run).Times(0);
  delegate.OnTransportAvailabilityEnumerated(std::move(transports_info));

  EXPECT_CALL(mock_closure, Run).Times(1);
  password_fetcher->InvokeCallback();

  PasswordCredentialFetcher::SetInstanceForTesting(nullptr);
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TryToShowUiHasImmediateCredentials) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kOrigin));
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  auto* password_fetcher = new FakePasswordCredentialFetcher(main_rfh());
  PasswordCredentialFetcher::SetInstanceForTesting(password_fetcher);
  auto password_ui_controller =
      std::make_unique<testing::NiceMock<MockPasswordCredentialUIController>>(
          main_rfh()->GetGlobalId(), delegate.dialog_model());
  delegate.SetPasswordUIControllerForTesting(std::move(password_ui_controller));
  base::MockCallback<base::OnceClosure> mock_closure;
  delegate.RegisterActionCallbacks(
      base::DoNothing(), mock_closure.Get(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing());
  delegate.SetUIPresentation(UIPresentation::kModalImmediate);
  delegate.SetCredentialTypes(kRequestPassword | kRequestPublicKey);
  delegate.SetRelyingPartyId(kRpId);
  MockCableDiscoveryFactory discovery_factory;
  delegate.ConfigureDiscoveries(url::Origin::Create(GURL(kOrigin)), kOrigin,
                                content::AuthenticatorRequestClientDelegate::
                                    RequestSource::kWebAuthentication,
                                device::FidoRequestType::kGetAssertion,
                                device::ResidentKeyRequirement::kPreferred,
                                device::UserVerificationRequirement::kRequired,
                                /*user_name=*/std::nullopt, {},
                                /*is_enclave_authenticator_available=*/false,
                                &discovery_factory);
  TransportAvailabilityInfo transports_info;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.recognized_credentials = {
      device::DiscoverableCredentialMetadata(
          device::AuthenticatorType::kEnclave, kRpId, {},
          device::PublicKeyCredentialUserEntity(),
          /*provider_name=*/std::nullopt)};

  // still waiting for passwords.
  EXPECT_CALL(mock_closure, Run).Times(0);
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated).Times(0);
  delegate.OnTransportAvailabilityEnumerated(std::move(transports_info));

  EXPECT_CALL(mock_closure, Run).Times(0);
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated).Times(1);
  password_fetcher->InvokeCallback();

  PasswordCredentialFetcher::SetInstanceForTesting(nullptr);
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OnPasswordSelectedUpdatesDateLastUsed) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kOrigin));
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  auto* password_fetcher = new FakePasswordCredentialFetcher(main_rfh());
  PasswordCredentialFetcher::SetInstanceForTesting(password_fetcher);
  bool update_date_last_used_called = false;
  password_fetcher->set_update_date_last_used_called_ptr(
      &update_date_last_used_called);
  auto password_ui_controller =
      std::make_unique<testing::NiceMock<MockPasswordCredentialUIController>>(
          main_rfh()->GetGlobalId(), delegate.dialog_model());

  content::AuthenticatorRequestClientDelegate::PasswordSelectedCallback
      password_selected_and_date_last_used_callback;
  EXPECT_CALL(*password_ui_controller, SetPasswordSelectedCallback)
      .WillOnce(
          testing::SaveArg<0>(&password_selected_and_date_last_used_callback));
  delegate.SetPasswordUIControllerForTesting(std::move(password_ui_controller));
  delegate.SetUIPresentation(UIPresentation::kModalImmediate);
  delegate.SetCredentialTypes(kRequestPassword | kRequestPublicKey);
  delegate.SetRelyingPartyId(kRpId);
  MockCableDiscoveryFactory discovery_factory;

  delegate.RegisterActionCallbacks(
      base::DoNothing(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing(), base::DoNothing());

  delegate.ConfigureDiscoveries(url::Origin::Create(GURL(kOrigin)), kOrigin,
                                content::AuthenticatorRequestClientDelegate::
                                    RequestSource::kWebAuthentication,
                                device::FidoRequestType::kGetAssertion,
                                device::ResidentKeyRequirement::kPreferred,
                                device::UserVerificationRequirement::kRequired,
                                /*user_name=*/std::nullopt, {},
                                /*is_enclave_authenticator_available=*/false,
                                &discovery_factory);

  // Invoke the callback set on the UI controller, which in turn calls
  // ChromeAuthenticatorRequestDelegate::OnPasswordSelected.
  password_manager::CredentialInfo credential_info;
  credential_info.id = u"test_user";
  credential_info.password = u"test_password";
  password_selected_and_date_last_used_callback.Run(credential_info);

  EXPECT_TRUE(update_date_last_used_called);

  PasswordCredentialFetcher::SetInstanceForTesting(nullptr);
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, ImmediateMediationRateLimit) {
  constexpr base::TimeDelta kWindowSize = base::Minutes(1);
  constexpr int kMaxRequestsPerWindow = 2;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      device::kWebAuthnImmediateRequestRateLimit,
      {{"max_requests", base::NumberToString(kMaxRequestsPerWindow)},
       {"window_seconds", "60"}});
  // Navigate to commit the origin.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kOrigin));
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetRelyingPartyId(kRpId);
  delegate.SetUIPresentation(UIPresentation::kModalImmediate);

  // Register a mock callback for the immediate_not_found case.
  // This is called when MaybeHandleImmediateMediation returns true (e.g., rate
  // limited).
  base::MockCallback<base::OnceClosure> mock_immediate_not_found_callback;
  delegate.RegisterActionCallbacks(
      /*cancel_callback=*/base::DoNothing(),
      mock_immediate_not_found_callback.Get(),
      /*start_over_callback=*/base::DoNothing(),
      /*account_preselected_callback=*/base::DoNothing(),
      /*password_selected_callback=*/base::DoNothing(),
      /*request_callback=*/base::DoNothing(),
      /*cancel_ui_timeout_callback=*/base::DoNothing(),
      /*bluetooth_adapter_power_on_callback=*/base::DoNothing(),
      /*bluetooth_query_status_callback=*/base::DoNothing());

  TransportAvailabilityInfo transports_info;
  transports_info.request_type = device::FidoRequestType::kGetAssertion;
  transports_info.recognized_credentials = {
      device::DiscoverableCredentialMetadata(
          device::AuthenticatorType::kEnclave, kRpId, {},
          device::PublicKeyCredentialUserEntity(),
          /*provider_name=*/std::nullopt)};

  for (int i = 0; i < kMaxRequestsPerWindow; ++i) {
    SCOPED_TRACE(testing::Message() << "Request " << i + 1);
    EXPECT_CALL(mock_immediate_not_found_callback, Run).Times(0);
    // Need to pass a copy as OnTransportAvailabilityEnumerated takes by value.
    TransportAvailabilityInfo info_copy = transports_info;
    delegate.OnTransportAvailabilityEnumerated(std::move(info_copy));
    testing::Mock::VerifyAndClearExpectations(
        &mock_immediate_not_found_callback);
  }

  // The next request should be rate-limited (callback is called).
  {
    SCOPED_TRACE(testing::Message() << "Request " << kMaxRequestsPerWindow + 1);
    EXPECT_CALL(mock_immediate_not_found_callback, Run).Times(1);
    TransportAvailabilityInfo info_copy = transports_info;
    delegate.OnTransportAvailabilityEnumerated(std::move(info_copy));
    testing::Mock::VerifyAndClearExpectations(
        &mock_immediate_not_found_callback);
  }

  // Advance time beyond the window.
  task_environment()->FastForwardBy(kWindowSize + base::Seconds(1));

  // The next request should be allowed again (callback not called).
  {
    SCOPED_TRACE(testing::Message() << "Request after time window");
    EXPECT_CALL(mock_immediate_not_found_callback, Run).Times(0);
    TransportAvailabilityInfo info_copy = transports_info;
    delegate.OnTransportAvailabilityEnumerated(std::move(info_copy));
    testing::Mock::VerifyAndClearExpectations(
        &mock_immediate_not_found_callback);
  }
}

}  // namespace

#if BUILDFLAG(IS_MAC)

// These test functions are outside of the anonymous namespace so that
// `FRIEND_TEST_ALL_PREFIXES` works to let them test private functions.

class ChromeAuthenticatorRequestDelegatePrivateTest : public testing::Test {
  // A `BrowserTaskEnvironment` needs to be in-scope in order to create a
  // `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest, DaysSinceDate) {
  const base::Time now = base::Time::FromTimeT(1691188997);  // 2023-08-04
  const struct {
    char input[16];
    std::optional<int> expected_result;
  } kTestCases[] = {
      {"", std::nullopt},          //
      {"2023-08-", std::nullopt},  //
      {"2023-08-04", 0},           //
      {"2023-08-03", 1},           //
      {"2023-8-3", 1},             //
      {"2023-07-04", 31},          //
      {"2001-11-23", 7924},        //
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.input);
    const std::optional<int> result =
        ChromeAuthenticatorRequestDelegate::DaysSinceDate(test.input, now);
    EXPECT_EQ(result, test.expected_result);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest, GetICloudKeychainPref) {
  TestingProfile profile;

  // We use a boolean preference as a tristate, so it's worth checking that
  // an unset preference is recognised as such.
  EXPECT_FALSE(ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
                   profile.GetPrefs())
                   .has_value());
  profile.GetPrefs()->SetBoolean(prefs::kCreatePasskeysInICloudKeychain, true);
  EXPECT_EQ(*ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
                profile.GetPrefs()),
            true);
}

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest,
       ShouldCreateInICloudKeychain) {
  // Safety check that SPC requests never default to iCloud Keychain.
  EXPECT_FALSE(ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
      ChromeAuthenticatorRequestDelegate::RequestSource::
          kSecurePaymentConfirmation,
      /*is_active_profile_authenticator_user=*/false,
      /*has_icloud_drive_enabled=*/true, /*request_is_for_google_com=*/true,
      /*preference=*/true));

  // For the valid request type, the preference should be controlling if set.
  for (const bool preference : {false, true}) {
    EXPECT_EQ(preference,
              ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
                  ChromeAuthenticatorRequestDelegate::RequestSource::
                      kWebAuthentication,
                  /*is_active_profile_authenticator_user=*/false,
                  /*has_icloud_drive_enabled=*/true,
                  /*request_is_for_google_com=*/true,
                  /*preference=*/preference));

    // Otherwise the default is controlled by several feature flags. Testing
    // them would just be a change detector.
  }
}

#endif
