// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/gpm_browser_authenticator_controller.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/gpm/gpm_browser_authenticator.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "device/fido/public/features.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using AccountState = GpmBrowserAuthenticatorController::AccountState;
using AuthenticationFactorsResult =
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
using Step = AuthenticatorRequestDialogModel::Step;

constexpr char kRpId[] = "example.com";
constexpr char kOrigin[] = "https://example.com";
constexpr char kUserEmail[] = "user@example.com";
constexpr int32_t kKeyVersion = 7;

const std::vector<uint8_t> kTrustedVaultKey = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

class GpmBrowserAuthenticatorControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  GpmBrowserAuthenticatorControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        {device::kWebAuthnLocalGpmPasskeys,
         trusted_vault::kTrustedVaultSharedRecoveryFactorsAndEncryption},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(kOrigin));

    PasskeyModelFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));

    TrustedVaultServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            &GpmBrowserAuthenticatorControllerTest::BuildTrustedVaultService,
            base::Unretained(this)));

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    signin::MakePrimaryAccountAvailable(identity_manager, kUserEmail,
                                        signin::ConsentLevel::kSignin);
    account_info_ =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  }

  void TearDown() override {
    passkeys_vault_client_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<KeyedService> BuildTrustedVaultService(
      content::BrowserContext* context) {
    auto chrome_sync_client =
        std::make_unique<trusted_vault::FakeTrustedVaultClient>(
            /*auto_complete_requests=*/true);
    auto passkeys_client =
        std::make_unique<trusted_vault::FakeTrustedVaultClient>(
            /*auto_complete_requests=*/true);
    passkeys_vault_client_ = passkeys_client.get();
    return std::make_unique<trusted_vault::TrustedVaultService>(
        std::move(chrome_sync_client), std::move(passkeys_client));
  }

  void SetRegistrationStateResult(
      AuthenticationFactorsResult::State state,
      std::optional<int32_t> key_version = kKeyVersion) {
    AuthenticationFactorsResult result;
    result.state = state;
    result.key_version = key_version;
    auto connection = std::make_unique<testing::NiceMock<
        trusted_vault::MockTrustedVaultThrottlingConnection>>();
    EXPECT_CALL(*connection,
                DownloadAuthenticationFactorsRegistrationState(
                    testing::_, trusted_vault::SecurityDomainId::kPasskeys,
                    testing::_, testing::_))
        .WillOnce([result = std::move(result)](
                      const CoreAccountInfo&, trusted_vault::SecurityDomainId,
                      trusted_vault::TrustedVaultConnection::
                          DownloadAuthenticationFactorsRegistrationStateCallback
                              callback,
                      base::RepeatingClosure) mutable {
          std::move(callback).Run(std::move(result));
          return std::make_unique<
              trusted_vault::TrustedVaultConnection::Request>();
        });
    GpmTrustedVaultConnectionProvider::SetOverrideForFrame(
        main_rfh(), std::move(connection));
  }

  sync_pb::WebauthnCredentialSpecifics AddTestPasskey(
      std::vector<uint8_t> user_id = {1, 2, 3, 4},
      bool hidden = false) {
    auto [passkey, public_key_spki_der] =
        webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
            kRpId,
            webauthn::PasskeyModel::UserEntity(std::move(user_id), "user",
                                               "User"),
            kTrustedVaultKey, kKeyVersion,
            /*device_authorization_key=*/std::nullopt,
            webauthn::passkey_model_utils::ExtensionInputData(),
            /*extension_output_data=*/nullptr);
    passkey.set_hidden(hidden);
    PasskeyModelFactory::GetForProfile(profile())->CreatePasskey(passkey);
    return passkey;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  CoreAccountInfo account_info_;
  raw_ptr<trusted_vault::FakeTrustedVaultClient> passkeys_vault_client_ =
      nullptr;
};

TEST_F(GpmBrowserAuthenticatorControllerTest, MakeCredentialReady) {
  sync_pb::WebauthnCredentialSpecifics existing = AddTestPasskey();
  auto model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  passkeys_vault_client_->StoreKeys(account_info_.gaia, {kTrustedVaultKey},
                                    kKeyVersion, std::nullopt);
  SetRegistrationStateResult(AuthenticationFactorsResult::State::kRecoverable);

  base::test::TestFuture<GpmBrowserAuthenticatorRequest> request_future;
  GpmBrowserAuthenticatorController controller(
      *main_rfh(), *model, kRpId, device::FidoRequestType::kMakeCredential,
      device::UserVerificationRequirement::kRequired,
      request_future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return controller.account_state_for_testing() == AccountState::kReady;
  }));

  controller.OnGPMCreationSelected();
  EXPECT_EQ(model->step(), Step::kGPMCreatePasskey);

  controller.OnGPMCreationConfirmed();
  GpmBrowserAuthenticatorRequest request = request_future.Take();
  EXPECT_FALSE(request.user_verified);
  const auto* make_data =
      std::get_if<GpmBrowserAuthenticatorRequest::MakeCredential>(
          &request.data);
  ASSERT_TRUE(make_data);
  EXPECT_EQ(
      make_data->trusted_vault_key,
      trusted_vault::TrustedVaultKeyAndVersion(kTrustedVaultKey, kKeyVersion));
  EXPECT_THAT(make_data->existing_cred_ids,
              testing::ElementsAre(base::ToVector(
                  base::as_byte_span(existing.credential_id()))));

  auto [new_passkey, public_key_spki_der] =
      webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          kRpId,
          webauthn::PasskeyModel::UserEntity(std::vector<uint8_t>{5, 6, 7, 8},
                                             "user2", "User 2"),
          kTrustedVaultKey, kKeyVersion,
          /*device_authorization_key=*/std::nullopt,
          webauthn::passkey_model_utils::ExtensionInputData(),
          /*extension_output_data=*/nullptr);
  request.save_passkey_callback.Run(new_passkey);

  std::vector<sync_pb::WebauthnCredentialSpecifics> saved =
      PasskeyModelFactory::GetForProfile(profile())->GetPasskeys(
          kRpId, webauthn::PasskeyModel::ShadowedCredentials::kExclude);
  EXPECT_EQ(saved.size(), 2u);
}

TEST_F(GpmBrowserAuthenticatorControllerTest, GetAssertionReady) {
  sync_pb::WebauthnCredentialSpecifics passkey = AddTestPasskey();
  auto model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  passkeys_vault_client_->StoreKeys(account_info_.gaia, {kTrustedVaultKey},
                                    kKeyVersion, std::nullopt);

  base::test::TestFuture<GpmBrowserAuthenticatorRequest> request_future;
  GpmBrowserAuthenticatorController controller(
      *main_rfh(), *model, kRpId, device::FidoRequestType::kGetAssertion,
      device::UserVerificationRequirement::kRequired,
      request_future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return controller.account_state_for_testing() == AccountState::kReady;
  }));

  controller.OnGPMPasskeySelected(
      base::ToVector(base::as_byte_span(passkey.credential_id())));
  GpmBrowserAuthenticatorRequest request = request_future.Take();
  const auto* get_data =
      std::get_if<GpmBrowserAuthenticatorRequest::GetAssertion>(&request.data);
  ASSERT_TRUE(get_data);
  EXPECT_EQ(get_data->trusted_vault_keys,
            std::vector<std::vector<uint8_t>>{kTrustedVaultKey});
  EXPECT_EQ(get_data->entity.credential_id(), passkey.credential_id());
}

TEST_F(GpmBrowserAuthenticatorControllerTest,
       GetAssertionIgnoresHiddenCredentials) {
  AddTestPasskey(/*user_id=*/{1, 2, 3, 4}, /*hidden=*/true);
  auto model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  GpmBrowserAuthenticatorController controller(
      *main_rfh(), *model, kRpId, device::FidoRequestType::kGetAssertion,
      device::UserVerificationRequirement::kRequired, base::DoNothing());

  EXPECT_TRUE(controller.creds().empty());
  EXPECT_FALSE(controller.is_active());
}

TEST_F(GpmBrowserAuthenticatorControllerTest, RecoveryFlow) {
  sync_pb::WebauthnCredentialSpecifics passkey = AddTestPasskey();
  auto model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  SetRegistrationStateResult(AuthenticationFactorsResult::State::kRecoverable);

  base::test::TestFuture<GpmBrowserAuthenticatorRequest> request_future;
  GpmBrowserAuthenticatorController controller(
      *main_rfh(), *model, kRpId, device::FidoRequestType::kGetAssertion,
      device::UserVerificationRequirement::kRequired,
      request_future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return controller.account_state_for_testing() == AccountState::kRecoverable;
  }));

  controller.OnGPMPasskeySelected(
      base::ToVector(base::as_byte_span(passkey.credential_id())));
  EXPECT_EQ(model->step(), Step::kGPMTrustThisComputerAssertion);

  controller.OnGPMTrustThisComputer();
  EXPECT_EQ(model->step(), Step::kGPMRecoverSecurityDomain);

  passkeys_vault_client_->StoreKeys(account_info_.gaia, {kTrustedVaultKey},
                                    kKeyVersion, std::nullopt);
  GpmBrowserAuthenticatorRequest request = request_future.Take();
  const auto* get_data =
      std::get_if<GpmBrowserAuthenticatorRequest::GetAssertion>(&request.data);
  ASSERT_TRUE(get_data);
  EXPECT_EQ(get_data->trusted_vault_keys,
            std::vector<std::vector<uint8_t>>{kTrustedVaultKey});
}

TEST_F(GpmBrowserAuthenticatorControllerTest, LoadingTimeout) {
  GpmTickAndTaskRunnerProvider::SetOverrideForFrame(
      main_rfh(), task_environment()->GetMockTickClock(),
      task_environment()->GetMainThreadTaskRunner());
  auto connection = std::make_unique<
      testing::NiceMock<trusted_vault::MockTrustedVaultThrottlingConnection>>();
  trusted_vault::TrustedVaultConnection::
      DownloadAuthenticationFactorsRegistrationStateCallback pending_callback;
  EXPECT_CALL(*connection,
              DownloadAuthenticationFactorsRegistrationState(
                  testing::_, trusted_vault::SecurityDomainId::kPasskeys,
                  testing::_, testing::_))
      .WillOnce([&pending_callback](
                    const CoreAccountInfo&, trusted_vault::SecurityDomainId,
                    trusted_vault::TrustedVaultConnection::
                        DownloadAuthenticationFactorsRegistrationStateCallback
                            callback,
                    base::RepeatingClosure) {
        pending_callback = std::move(callback);
        return std::make_unique<
            trusted_vault::TrustedVaultConnection::Request>();
      });
  GpmTrustedVaultConnectionProvider::SetOverrideForFrame(main_rfh(),
                                                         std::move(connection));

  auto model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  GpmBrowserAuthenticatorController controller(
      *main_rfh(), *model, kRpId, device::FidoRequestType::kMakeCredential,
      device::UserVerificationRequirement::kRequired, base::DoNothing());

  controller.OnGPMCreationSelected();
  EXPECT_EQ(model->step(), Step::kNotStarted);

  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(model->step(), Step::kMechanismSelection);
}

TEST_F(GpmBrowserAuthenticatorControllerTest,
       DuplicateStartTransactionIgnored) {
  auto model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  passkeys_vault_client_->StoreKeys(account_info_.gaia, {kTrustedVaultKey},
                                    kKeyVersion, std::nullopt);
  SetRegistrationStateResult(AuthenticationFactorsResult::State::kRecoverable);

  auto discovery = std::make_unique<GpmBrowserAuthenticatorDiscovery>();
  testing::NiceMock<device::MockFidoDiscoveryObserver> observer;
  discovery->set_observer(&observer);
  discovery->Start();

  GpmBrowserAuthenticatorController controller(
      *main_rfh(), *model, kRpId, device::FidoRequestType::kMakeCredential,
      device::UserVerificationRequirement::kRequired,
      discovery->GetAddAuthenticatorCallback());

  ASSERT_TRUE(base::test::RunUntil([&] {
    return controller.account_state_for_testing() == AccountState::kReady;
  }));

  EXPECT_CALL(observer, AuthenticatorAdded(discovery.get(), testing::_))
      .Times(1);

  controller.OnGPMCreationSelected();
  EXPECT_EQ(model->step(), Step::kGPMCreatePasskey);

  controller.OnGPMCreationConfirmed();
  controller.OnGPMCreationConfirmed();
}

}  // namespace
