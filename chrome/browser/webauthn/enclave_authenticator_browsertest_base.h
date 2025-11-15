// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_AUTHENTICATOR_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_AUTHENTICATOR_BROWSERTEST_BASE_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <variant>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/webauthn/fake_recovery_key_store.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/browser/webauthn/webauthn_scoped_fake_unexportable_key_provider.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/render_frame_host.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/enclave/constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/util.h"
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"
#include "device/fido/mac/fake_icloud_keychain.h"
#include "device/fido/mac/util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace base {
class CommandLine;
}  // namespace base

namespace signin {
class IdentityTestEnvironment;
}  // namespace signin

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

class EnclaveManager;
class IdentityTestEnvironmentProfileAdaptor;
class SyncServiceImplHarness;
struct TempDir;

// Base class for Enclave Authenticator tests that handles common
// infrastructure setup like the sync server, fake enclave, service fakes,
// and platform fakes, but does not include UI observers.
class EnclaveAuthenticatorTestBase : public SyncTest {
 public:
  using AuthenticationFactorsResult =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;

  EnclaveAuthenticatorTestBase();
  ~EnclaveAuthenticatorTestBase() override;

  EnclaveAuthenticatorTestBase(const EnclaveAuthenticatorTestBase&) = delete;
  EnclaveAuthenticatorTestBase& operator=(const EnclaveAuthenticatorTestBase&) =
      delete;

  base::FilePath GetTempDirPath();

 protected:
  // SyncTest overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUp() override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  signin::IdentityTestEnvironment& identity_test_env();
  webauthn::PasskeyModel& passkey_model();
  EnclaveManager& enclave_manager();

  void EnableUVKeySupport(bool fake_hardware_backing = false);
  bool IsUVPAA();
  void SetBiometricsEnabled(bool enabled);
  void AddTestPasskeyToModel();
  void SimulateTrustedVaultKeyRetrieval();
  void SimulateTrustedVaultKeyRetrieval(
      base::span<const uint8_t> trusted_vault_key,
      int trusted_vault_key_version);

  // Convenience methods for setting up the mock trusted vault connection:
  void SetMockVaultConnectionOnRequestDelegate(
      AuthenticationFactorsResult result,
      content::RenderFrameHost* rfh = nullptr);
  void SetTrustedVaultEmpty();
  void SetTrustedVaultRecoverable(int32_t key_version = kSecretVersion,
                                  content::RenderFrameHost* rfh = nullptr);
  void SetTrustedVaultSlowAndCacheCallback();
  base::OnceCallback<void(AuthenticationFactorsResult)> cached_connection_cb() {
    return std::move(cached_connection_cb_);
  }

  void SimulateSuccessfulGpmPinCreation(const std::string& pin_value);

  void WaitForEnclaveLoaded();

  scoped_refptr<base::TestMockTimeTaskRunner> timer_task_runner_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<TempDir> temp_dir_;
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<SyncServiceImplHarness> sync_harness_;
  const std::pair<base::Process, uint16_t> process_and_port_;
  const device::enclave::ScopedEnclaveOverride enclave_override_;
  std::unique_ptr<FakeSecurityDomainService> security_domain_service_;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<device::FakeWinWebAuthnApi> fake_webauthn_dll_;
  std::unique_ptr<device::WinWebAuthnApi::ScopedOverride>
      webauthn_dll_override_;
  std::unique_ptr<device::fido::win::ScopedBiometricsOverride>
      biometrics_override_;
#elif BUILDFLAG(IS_MAC)
  std::unique_ptr<device::fido::mac::ScopedBiometricsOverride>
      biometrics_override_;
  std::unique_ptr<device::fido::icloud_keychain::Fake> fake_icloud_keychain_;
  std::unique_ptr<ScopedICloudDriveOverride> scoped_icloud_drive_override_;
#endif
  std::unique_ptr<FakeRecoveryKeyStore> recovery_key_store_;
  std::unique_ptr<WebAuthnScopedFakeUnexportableKeyProvider> fake_hw_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_values_for_testing_;
  std::variant<crypto::ScopedNullUserVerifyingKeyProvider,
               crypto::ScopedFakeUserVerifyingKeyProvider,
               crypto::ScopedFailingUserVerifyingKeyProvider>
      fake_uv_provider_;
  logging::ScopedVmoduleSwitches scoped_vmodule_;
  bool sync_feature_enabled_ = true;
  base::OnceCallback<void(AuthenticationFactorsResult)> cached_connection_cb_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_AUTHENTICATOR_BROWSERTEST_BASE_H_
