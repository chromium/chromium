// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/chromeos/passkey_dialog_controller.h"
#include "chrome/browser/webauthn/chromeos/passkey_in_session_auth.h"
#include "chrome/browser/webauthn/chromeos/passkey_service.h"
#include "chrome/browser/webauthn/chromeos/passkey_service_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "components/trusted_vault/test/mock_trusted_vault_connection.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#endif

using testing::_;

namespace chromeos {
namespace {

constexpr std::string_view kGetAssertionRequest = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

constexpr std::array<uint8_t, 32> kTrustedVaultKey{'k'};
constexpr uint8_t kTrustedVaultKeyVersion = 0;

constexpr std::string_view kRpId = "www.example.com";

// ScopedInSessionAuthOverride disables the InSessionAuth dialog that the
// authenticator uses to assert UV.
class ScopedInSessionAuthOverride : public PasskeyInSessionAuthProvider {
 public:
  ScopedInSessionAuthOverride() {
    PasskeyInSessionAuthProvider::SetInstanceForTesting(this);
  }

  ~ScopedInSessionAuthOverride() override {
    PasskeyInSessionAuthProvider::SetInstanceForTesting(nullptr);
  }

  void ShowPasskeyInSessionAuthDialog(
      aura::Window* window,
      const std::string& rp_id,
      base::OnceCallback<void(bool)> result_callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback), true));
  }
};

// TestObserver lets us inspect and instrument the UI by observing the
// ChromeAuthenticatorRequestDelegate and its associated
// AuthenticatorRequestDialogModel.
class TestObserver : public ChromeAuthenticatorRequestDelegate::TestObserver,
                     public AuthenticatorRequestDialogModel::Observer {
 public:
  TestObserver() {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(this);
  }

  ~TestObserver() override {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
  }

  void WaitForUI() {
    if (ui_shown_) {
      return;
    }
    wait_ui_loop_ = std::make_unique<base::RunLoop>();
    wait_ui_loop_->Run();
    wait_ui_loop_.reset();
    CHECK(ui_shown_);
  }

  bool gpm_ready() { return gpm_ready_; }

  void WaitForGPMReady() {
    if (gpm_ready_) {
      return;
    }
    gpm_ready_loop_.Run();
  }

  void WaitForStep(AuthenticatorRequestDialogModel::Step step) {
    CHECK(request_delegate_);
    while (request_delegate_->dialog_model()->step() != step) {
      wait_step_loop_ = std::make_unique<base::RunLoop>();
      wait_step_loop_->Run();
    }
  }

  ChromeAuthenticatorRequestDelegate& request_delegate() {
    CHECK(request_delegate_) << "No WebAuthn request in progress?";
    return *request_delegate_;
  }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override {
    if (wait_step_loop_) {
      wait_step_loop_->QuitWhenIdle();
    }
  }

  void OnChromeOSGPMRequestReady() override {
    CHECK(!gpm_ready_);
    gpm_ready_ = true;
    gpm_ready_loop_.QuitWhenIdle();
  }

  // ChromeAuthenticatorRequestDelegate::TestObserver:
  void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
    CHECK(!ui_shown_);
    ui_shown_ = true;
    if (wait_ui_loop_) {
      wait_ui_loop_->QuitWhenIdle();
    }
  }

  void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
    CHECK(!request_delegate_);
    request_delegate_ = delegate;
    request_delegate_->dialog_model()->observers.AddObserver(this);
  }

  void OnDestroy(ChromeAuthenticatorRequestDelegate* delegate) override {
    CHECK(request_delegate_);
    request_delegate_->dialog_model()->observers.RemoveObserver(this);
    request_delegate_ = nullptr;
  }

  std::vector<std::unique_ptr<device::cablev2::Pairing>>
  GetCablePairingsFromSyncedDevices() override {
    return {};
  }

  void OnTransportAvailabilityEnumerated(
      ChromeAuthenticatorRequestDelegate* delegate,
      device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai) override {
  }

  void CableV2ExtensionSeen(
      base::span<const uint8_t> server_link_data) override {}

  void AccountSelectorShown(
      const std::vector<device::AuthenticatorGetAssertionResponse>& responses)
      override {}

 private:
  bool ui_shown_ = false;
  std::unique_ptr<base::RunLoop> wait_ui_loop_;
  bool gpm_ready_ = false;
  base::RunLoop gpm_ready_loop_;
  std::unique_ptr<base::RunLoop> wait_step_loop_;

  raw_ptr<ChromeAuthenticatorRequestDelegate> request_delegate_ = nullptr;
};

class ChromeOSPasskeyBrowserTest : public SyncTest {
 public:
  ChromeOSPasskeyBrowserTest() : SyncTest(SINGLE_CLIENT) {}
  ~ChromeOSPasskeyBrowserTest() override = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    SyncTest::CreatedBrowserMainParts(browser_main_parts);
    // Initialize a FakeHidManager. Otherwise, the FidoHidDiscovery instantiated
    // for the WebAuthn request fails to enumerate devices and holds up the
    // request indefinitely.
    mojo::PendingRemote<device::mojom::HidManager> pending_remote;
    fake_hid_manager_.Bind(pending_remote.InitWithNewPipeAndPassReceiver());
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        std::move(pending_remote));
  }
#endif

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitch(switches::kDisableFakeServerFailureOutput);
  }

  void SetUp() override {
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.InitializeAndListen());

    // Disable Bluetooth to avoid requests handlers attempting to enumerate
    // BLE-based authenticators. This significantly speeds up the tests.
    bluetooth_values_for_testing_ =
        device::BluetoothAdapterFactory::Get()->InitGlobalValuesForTesting();
    bluetooth_values_for_testing_->SetLESupported(false);

    SyncTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    test_observer_.reset();
    trusted_vault_connection_ = nullptr;
    passkey_service_ = nullptr;
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    SyncTest::TearDownOnMainThread();
  }

  TestObserver& test_observer() { return *test_observer_; }

  webauthn::PasskeyModel* passkey_model() {
    return PasskeyModelFactory::GetInstance()->GetForProfile(GetProfile(0));
  }

  ChromeAuthenticatorRequestDelegate& request_delegate() {
    return test_observer().request_delegate();
  }

  AuthenticatorRequestDialogModel& dialog_model() {
    return *test_observer().request_delegate().dialog_model();
  }

  sync_pb::WebauthnCredentialSpecifics InjectTestPasskey() {
    std::vector<uint8_t> unused_public_key_spki_der;
    return passkey_model()->CreatePasskey(
        kRpId,
        webauthn::PasskeyModel::UserEntity(std::vector<uint8_t>(32, 'u'),
                                           "example user", "user@example.com"),
        kTrustedVaultKey, kTrustedVaultKeyVersion, &unused_public_key_spki_der);
  }

  void SimulateTrustedVaultRecovery() {
    const std::vector<std::vector<uint8_t>> trusted_vault_keys = {
        {kTrustedVaultKey.begin(), kTrustedVaultKey.end()}};
    fake_trusted_vault_client_->server()->StoreKeysOnServer(
        GetSyncService(0)->GetAccountInfo().gaia, trusted_vault_keys);
    fake_trusted_vault_client_->StoreKeys(
        GetSyncService(0)->GetAccountInfo().gaia, trusted_vault_keys,
        kTrustedVaultKeyVersion);
  }

  [[nodiscard]] bool SetupSyncAndPasskeyService() {
    // Set up sync and enable password datatype.
    if (!SetupClients()) {
      LOG(ERROR) << "SetupClients() failed";
      return false;
    }
    if (!GetClient(0)->SignInPrimaryAccount() ||
        !GetClient(0)->AwaitSyncTransportActive()) {
      LOG(ERROR) << "SignInPrimaryAccount() failed";
      return false;
    }
    if (!GetClient(0)->SetupSync(base::BindLambdaForTesting(
            [](syncer::SyncUserSettings* user_settings) {
              user_settings->SetSelectedTypes(
                  /*sync_everything=*/false,
                  /*types=*/{syncer::UserSelectableType::kPasswords});
            }))) {
      LOG(ERROR) << "SetupSync() failed";
      return false;
    }

    // Set up the passkey service.
    // TODO(crbug.com/40187814): Use the real service instances here and point
    // them to a `FakeSecurityDomainsServer`.
    passkey_service_ = reinterpret_cast<PasskeyService*>(
        PasskeyServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            GetProfile(0),
            base::BindRepeating(
                &ChromeOSPasskeyBrowserTest::CreatePasskeyService,
                base::Unretained(this))));
    test_observer_ = std::make_unique<TestObserver>();
    return true;
  }

  std::unique_ptr<KeyedService> CreatePasskeyService(
      content::BrowserContext* browser_context) {
    CHECK(!passkey_service_) << "PasskeyServiceFactory invoked twice";
    Profile* profile = Profile::FromBrowserContext(browser_context);
    CHECK_EQ(profile, GetProfile(0));
    scoped_in_session_auth_override_ =
        std::make_unique<ScopedInSessionAuthOverride>();
    trusted_vault_connection_holder_ =
        std::make_unique<trusted_vault::MockTrustedVaultConnection>();
    trusted_vault_connection_ = trusted_vault_connection_holder_.get();
    fake_trusted_vault_client_ =
        std::make_unique<trusted_vault::FakeTrustedVaultClient>(
            /*auto_complete_requests=*/true);
    return std::make_unique<PasskeyService>(
        IdentityManagerFactory::GetForProfile(profile), GetSyncService(0),
        fake_trusted_vault_client_.get(),
        std::move(trusted_vault_connection_holder_));
  }

  void SetAuthFactorRegistrationState(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::State
          state) {
    ON_CALL(*trusted_vault_connection_,
            DownloadAuthenticationFactorsRegistrationState(_, _))
        .WillByDefault(
            [state](
                const CoreAccountInfo&,
                base::OnceCallback<void(
                    trusted_vault::
                        DownloadAuthenticationFactorsRegistrationStateResult)>
                    callback) {
              trusted_vault::
                  DownloadAuthenticationFactorsRegistrationStateResult result;
              result.state = state;
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), std::move(result)));
              return std::make_unique<
                  trusted_vault::TrustedVaultConnection::Request>();
            });
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<TestObserver> test_observer_;
  std::unique_ptr<ScopedInSessionAuthOverride> scoped_in_session_auth_override_;
  raw_ptr<PasskeyService> passkey_service_;
  std::unique_ptr<trusted_vault::MockTrustedVaultConnection>
      trusted_vault_connection_holder_;
  raw_ptr<trusted_vault::MockTrustedVaultConnection> trusted_vault_connection_;
  std::unique_ptr<trusted_vault::FakeTrustedVaultClient>
      fake_trusted_vault_client_;

  base::test::ScopedFeatureList scoped_feature_list_{device::kChromeOsPasskeys};
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  device::FakeHidManager fake_hid_manager_;
#endif
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalValuesForTesting>
      bluetooth_values_for_testing_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ChromeOSPasskeyBrowserTest, GetAssertion_Success) {
  ASSERT_TRUE(SetupSyncAndPasskeyService());
  chrome::NewTab(GetBrowser(0));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      GetBrowser(0), https_server_.GetURL("www.example.com", "/title1.html")));

  SetAuthFactorRegistrationState(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
          State::kRecoverable);
  sync_pb::WebauthnCredentialSpecifics passkey = InjectTestPasskey();

  content::WebContents* web_contents =
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionRequest);

  test_observer().WaitForUI();

  EXPECT_EQ(dialog_model().step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);

  EXPECT_EQ(request_delegate()
                .chromeos_passkey_controller_for_testing()
                .account_state_for_testing(),
            PasskeyService::AccountState::kNeedsRecovery);

  dialog_model().OnUserConfirmedPriorityMechanism();

  test_observer().WaitForStep(
      AuthenticatorRequestDialogController::Step::kRecoverSecurityDomain);
  ASSERT_FALSE(test_observer().gpm_ready());
  SimulateTrustedVaultRecovery();
  test_observer().WaitForGPMReady();

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

}  // namespace chromeos
