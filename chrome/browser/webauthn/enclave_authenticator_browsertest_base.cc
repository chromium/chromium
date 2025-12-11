// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_authenticator_browsertest_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/buildflag.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/enclave_keys_waiter.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/fake_recovery_key_store.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/features.h"
#include "enclave_authenticator_browsertest_base.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"
#include "device/fido/mac/fake_icloud_keychain.h"
#include "device/fido/mac/util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

static constexpr char kIsUVPAA[] = R"((() => {
  window.PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable().
    then(result => window.domAutomationController.send('IsUVPAA: ' + result),
         error  => window.domAutomationController.send('error '    + error));
})())";

}  // namespace

// Helper struct for managing a temporary directory.
struct TempDir {
 public:
  TempDir() { CHECK(dir_.CreateUniqueTempDir()); }
  base::FilePath GetPath() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

EnclaveAuthenticatorTestBase::EnclaveAuthenticatorTestBase()
    : SyncTest(SINGLE_CLIENT),
      timer_task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
      temp_dir_(std::make_unique<TempDir>()),
      process_and_port_(StartWebAuthnEnclave(temp_dir_->GetPath())),
      enclave_override_(TestWebAuthnEnclaveIdentity(process_and_port_.second)),
      security_domain_service_(FakeSecurityDomainService::New(kSecretVersion)),
#if BUILDFLAG(IS_WIN)
      fake_webauthn_dll_(std::make_unique<device::FakeWinWebAuthnApi>()),
      webauthn_dll_override_(
          std::make_unique<device::WinWebAuthnApi::ScopedOverride>(
              fake_webauthn_dll_.get())),
#endif
      recovery_key_store_(FakeRecoveryKeyStore::New()),
      fake_hw_provider_(
          std::make_unique<WebAuthnScopedFakeUnexportableKeyProvider>()) {
#if BUILDFLAG(IS_WIN)
  fake_webauthn_dll_->set_available(false);
  biometrics_override_ =
      std::make_unique<device::fido::win::ScopedBiometricsOverride>(false);
#elif BUILDFLAG(IS_MAC)
  biometrics_override_ =
      std::make_unique<device::fido::mac::ScopedBiometricsOverride>(false);
  if (__builtin_available(macOS 13.5, *)) {
    fake_icloud_keychain_ = device::fido::icloud_keychain::NewFake();
  }
  scoped_icloud_drive_override_ = OverrideICloudDriveEnabled(false);
#endif
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{device::kWebAuthnLargeBlobForGPM,
                            device::kWebAuthnSignalApiHidePasskeys,
                            device::kWebAuthnWrapCohortData},
      /*disabled_features=*/{});
  OSCryptMocker::SetUp();
  scoped_vmodule_.InitWithSwitches("device_event_log_impl=2");

  auto security_domain_service_callback =
      security_domain_service_->GetCallback();
  auto recovery_key_store_callback = recovery_key_store_->GetCallback();
  url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [sds_callback = std::move(security_domain_service_callback),
       rks_callback = std::move(recovery_key_store_callback),
       this](const network::ResourceRequest& request) {
        std::optional<std::pair<net::HttpStatusCode, std::string>> response =
            sds_callback.Run(request);
        if (!response) {
          response = rks_callback.Run(request);
        }
        if (response) {
          url_loader_factory_.AddResponse(
              request.url.spec(), std::move(response->second), response->first);
        }
      }));

  fake_uv_provider_.emplace<crypto::ScopedNullUserVerifyingKeyProvider>();

  bluetooth_values_for_testing_ =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bluetooth_values_for_testing_->SetLESupported(false);
}

EnclaveAuthenticatorTestBase::~EnclaveAuthenticatorTestBase() {
  EnclaveManagerFactory::SetUrlLoaderFactoryForTesting(nullptr);
  CHECK(process_and_port_.first.Terminate(/*exit_code=*/1, /*wait=*/true));
  OSCryptMocker::TearDown();
}

base::FilePath EnclaveAuthenticatorTestBase::GetTempDirPath() {
  return temp_dir_->GetPath();
}

void EnclaveAuthenticatorTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  SyncTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
}

void EnclaveAuthenticatorTestBase::SetUp() {
  ASSERT_TRUE(https_server_.InitializeAndListen());
  EnclaveManagerFactory::SetUrlLoaderFactoryForTesting(
      url_loader_factory_.GetSafeWeakWrapper().get());
  SyncTest::SetUp();
}

void EnclaveAuthenticatorTestBase::SetUpInProcessBrowserTestFixture() {
  SyncTest::SetUpInProcessBrowserTestFixture();
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating([](content::BrowserContext* context) {
                IdentityTestEnvironmentProfileAdaptor::
                    SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                        context);
              }));
}

void EnclaveAuthenticatorTestBase::SetUpOnMainThread() {
  SyncTest::SetUpOnMainThread();

  identity_test_env_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
          browser()->profile());
  identity_test_env().SetAutomaticIssueOfAccessTokens(true);

  sync_harness_ = SyncServiceImplHarness::Create(
      browser()->profile(), SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  if (sync_feature_enabled_) {
    ASSERT_TRUE(sync_harness_->SetupSync());
  } else {
    ASSERT_TRUE(sync_harness_->SignInPrimaryAccount());
  }
  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->profile());
  ASSERT_EQ(kSyncEmail, sync_service->GetAccountInfo().email);
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});

  https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  https_server_.StartAcceptingConnections();
  host_resolver()->AddRule("*", "127.0.0.1");
}

void EnclaveAuthenticatorTestBase::TearDownOnMainThread() {
  identity_test_env_adaptor_.reset();
  SyncTest::TearDownOnMainThread();
}

signin::IdentityTestEnvironment&
EnclaveAuthenticatorTestBase::identity_test_env() {
  return CHECK_DEREF(identity_test_env_adaptor_->identity_test_env());
}

webauthn::PasskeyModel& EnclaveAuthenticatorTestBase::passkey_model() {
  return CHECK_DEREF(
      PasskeyModelFactory::GetInstance()->GetForProfile(browser()->profile()));
}

EnclaveManager& EnclaveAuthenticatorTestBase::enclave_manager() {
  return CHECK_DEREF(EnclaveManagerFactory::GetAsEnclaveManagerForProfile(
      browser()->profile()));
}

void EnclaveAuthenticatorTestBase::EnableUVKeySupport(
    bool fake_hardware_backing) {
  fake_uv_provider_.emplace<crypto::ScopedFakeUserVerifyingKeyProvider>(
      fake_hardware_backing);
}

bool EnclaveAuthenticatorTestBase::IsUVPAA() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kIsUVPAA);

  std::string script_result;
  CHECK(message_queue.WaitForMessage(&script_result));
  if (script_result == "\"IsUVPAA: true\"") {
    return true;
  } else if (script_result == "\"IsUVPAA: false\"") {
    return false;
  }
  NOTREACHED() << "unexpected IsUVPAA result: " << script_result;
}

void EnclaveAuthenticatorTestBase::SetBiometricsEnabled(bool enabled) {
#if BUILDFLAG(IS_MAC)
  biometrics_override_.reset();
  biometrics_override_ =
      std::make_unique<device::fido::mac::ScopedBiometricsOverride>(enabled);
#elif BUILDFLAG(IS_WIN)
  biometrics_override_.reset();
  biometrics_override_ =
      std::make_unique<device::fido::win::ScopedBiometricsOverride>(enabled);
#endif
}

void EnclaveAuthenticatorTestBase::AddTestPasskeyToModel() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  CHECK(passkey.ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
  passkey_model().AddNewPasskeyForTesting(passkey);
}

void EnclaveAuthenticatorTestBase::SimulateTrustedVaultKeyRetrieval(
    base::span<const uint8_t> trusted_vault_key,
    int trusted_vault_key_version) {
  enclave_manager().StoreKeys(kSyncGaiaId, {base::ToVector(trusted_vault_key)},
                              trusted_vault_key_version);
}

void EnclaveAuthenticatorTestBase::SimulateTrustedVaultKeyRetrieval() {
  SimulateTrustedVaultKeyRetrieval(kSecurityDomainSecret, kSecretVersion);
}

void EnclaveAuthenticatorTestBase::
    SimulateOpportunisticTrustedVaultKeyRetrieval() {
  EnclaveKeysWaiter enclave_keys_waiter(&enclave_manager());
  // Performing key retrieval without acquiring a lock via
  // `EnclaveManager::GetStoreKeysLock()`. The absence of acquired lock
  // indicates an opportunistic key retrieval logic. In this case (if either a
  // system UV or a usable GPM PIN is present) Enclave Manager stores keys and
  // adds device to account.
  SimulateTrustedVaultKeyRetrieval();
  EXPECT_EQ(enclave_keys_waiter.Wait(),
            EnclaveManager::OutOfContextRecoveryOutcome::
                kStoreKeysFromOpportunisticFlowSucceeded);
}

void EnclaveAuthenticatorTestBase::SetMockVaultConnectionOnRequestDelegate(
    AuthenticationFactorsResult result,
    content::RenderFrameHost* rfh) {
  auto connection = std::make_unique<
      testing::NiceMock<trusted_vault::MockTrustedVaultThrottlingConnection>>();
  EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                               testing::_, testing::_, testing::_))
      .WillOnce(
          [result = std::move(result)](
              const CoreAccountInfo&,
              base::OnceCallback<void(AuthenticationFactorsResult)> callback,
              base::RepeatingClosure _) mutable {
            std::move(callback).Run(std::move(result));
            return std::make_unique<
                trusted_vault::TrustedVaultConnection::Request>();
          });
  if (rfh == nullptr) {
    rfh = browser()
              ->tab_strip_model()
              ->GetActiveWebContents()
              ->GetPrimaryMainFrame();
  }
  GpmTrustedVaultConnectionProvider::SetOverrideForFrame(rfh,
                                                         std::move(connection));
}

void EnclaveAuthenticatorTestBase::SetTrustedVaultEmpty() {
  AuthenticationFactorsResult registration_state_result;
  registration_state_result.state = AuthenticationFactorsResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
}

void EnclaveAuthenticatorTestBase::SetTrustedVaultRecoverable(
    int32_t key_version,
    content::RenderFrameHost* rfh) {
  AuthenticationFactorsResult registration_state_result;
  registration_state_result.state =
      AuthenticationFactorsResult::State::kRecoverable;
  registration_state_result.key_version = key_version;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result),
                                          rfh);
  security_domain_service_->pretend_there_are_members();
}

void EnclaveAuthenticatorTestBase::SetTrustedVaultSlowAndCacheCallback() {
  auto connection_callback = [this](const CoreAccountInfo&,
                                    base::OnceCallback<void(
                                        AuthenticationFactorsResult)> callback,
                                    base::RepeatingClosure) {
    cached_connection_cb_ = std::move(callback);
    return std::make_unique<trusted_vault::TrustedVaultConnection::Request>();
  };
  auto connection = std::make_unique<
      testing::NiceMock<trusted_vault::MockTrustedVaultThrottlingConnection>>();
  EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                               testing::_, testing::_, testing::_))
      .WillOnce(connection_callback);
  GpmTrustedVaultConnectionProvider::SetOverrideForFrame(
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame(),
      std::move(connection));
}

void EnclaveAuthenticatorTestBase::SimulateSuccessfulGpmPinCreation(
    const std::string& pin_value) {
  WaitForEnclaveLoaded();

  {
    auto store_keys_lock = enclave_manager().GetStoreKeysLock();
    SimulateTrustedVaultKeyRetrieval(kSecurityDomainSecret, /*version=*/0);
  }

  base::test::TestFuture<bool> add_device_future;
  enclave_manager().AddDeviceAndPINToAccount(
      "123456",
      /*previous_pin_public_key=*/std::nullopt,
      add_device_future.GetCallback());
  ASSERT_TRUE(add_device_future.Wait()) << "AddDeviceAndPINToAccount timed out";
  ASSERT_TRUE(add_device_future.Get()) << "AddDeviceAndPINToAccount failed";

  ASSERT_TRUE(enclave_manager().is_ready());
  ASSERT_TRUE(enclave_manager().has_wrapped_pin());
}

void EnclaveAuthenticatorTestBase::WaitForEnclaveLoaded() {
  if (!enclave_manager().is_loaded()) {
    base::test::TestFuture<void> load_future;
    enclave_manager().Load(load_future.GetCallback());
    ASSERT_TRUE(load_future.Wait());
  }
}
