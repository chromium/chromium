// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_GLS_RUNNER_TEST_BASE_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_GLS_RUNNER_TEST_BASE_H_

#include <wrl/client.h>

#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

extern const char kDefaultEmail[];
extern const char kDefaultGaiaId[];
extern const wchar_t kDefaultUsername[];
extern const char kDefaultInvalidTokenHandleResponse[];
extern const char kDefaultValidTokenHandleResponse[];

// Helper class used to test the full call sequence of a credential provider by
// LoginUI. This includes creation of a credential provider filter and
// application of remote credentials if specified. There are default token
// handle responses (always valid token handles) and usage scenarios
// (CPUS_LOGON) that can be overridden before starting the call sequence for the
// credential provider.
class GlsRunnerTestBase : public ::testing::Test {
 public:
  // Gets a command line that runs a fake GLS that produces the desired output.
  // |default_exit_code| is the default value that will be written unless the
  // other command line arguments require a specific error code to be returned.
  static HRESULT GetFakeGlsCommandline(UiExitCodes default_exit_code,
                                       const std::string& gls_email,
                                       const std::string& gaia_id_override,
                                       const std::string& gaia_password,
                                       const std::string& full_name_override,
                                       const std::wstring& start_gls_event_name,
                                       bool ignore_expected_gaia_id,
                                       base::CommandLine* command_line);

 protected:
  GlsRunnerTestBase();
  ~GlsRunnerTestBase() override;

  void SetUp() override;
  void TearDown() override;

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }
  FakeWinHttpUrlFetcherFactory* fake_http_url_fetcher_factory() {
    return &fake_http_url_fetcher_factory_;
  }
  FakeCredentialProviderUserArray* fake_user_array() {
    return &fake_user_array_;
  }
  FakeAssociatedUserValidator* fake_associated_user_validator() {
    return &fake_associated_user_validator_;
  }
  FakePasswordRecoveryManager* fake_password_recovery_manager() {
    return &fake_password_recovery_manager_;
  }
  FakeGemDeviceDetailsManager* fake_gem_device_details_manager() {
    return &fake_gem_device_details_manager_;
  }
  FakeCredentialProviderEvents* fake_provider_events() {
    return &fake_provider_events_;
  }
  FakeCredentialProviderCredentialEvents*
  fake_credential_provider_credential_events() {
    return &fake_credential_provider_credential_events_;
  }
  FakeInternetAvailabilityChecker* fake_internet_checker() {
    return &fake_internet_checker_;
  }
  FakeChromeAvailabilityChecker* fake_chrome_checker() {
    return &fake_chrome_checker_;
  }

  const Microsoft::WRL::ComPtr<ICredentialProvider>& created_provider() const {
    return gaia_provider_;
  }

  void SetSidLockingWorkstation(const std::wstring& sid) {
    sid_locking_workstation_ = sid;
  }

  void SetDefaultTokenHandleResponse(const std::string& response) {
    default_token_handle_response_ = response;
  }

  void SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
    // Must be called before creating the provide. The usage does not normally
    // change during the execution of a provider.
    DCHECK(!gaia_provider_);
    cpus_ = cpus;
  }

  // Creates the provider and also all the credentials associated to users that
  // are already created before this call. Fills |credential_count| with the
  // number of credentials in the provider and |provider| with a pointer to the
  // created provider (this also correctly adds a reference to the provider).
  HRESULT
  InitializeProviderWithCredentials(DWORD* credential_count,
                                    ICredentialProvider** provider);

  // Creates the provider and also all the credentials associated to users that
  // are already created before this call. If |pcps_in| is non null then it will
  // pass this information as remote credentials to the credential provider
  // filter and provider. Fills |provider| with a pointer to the created
  // provider (this also correctly adds a reference to the provider).
  HRESULT
  InitializeProviderWithRemoteCredentials(
      const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
      ICredentialProvider** provider);

  // Creates the provider and also all the credentials associated to users that
  // are already created before this call. Once credentials are created, the
  // function tries to fill |credential| with the credential at index |index|.
  HRESULT InitializeProviderAndGetCredential(
      DWORD index,
      ICredentialProviderCredential** credential);

  // Used to release the provider before normal TearDown to test certain
  // cancellation scenarios. No other references should be held on the provider
  // to ensure that the provider can actually be released.
  HRESULT ReleaseProvider();

  // Initiates the logon process on the current |testing_credential_| that
  // is selected by a call to InitializeProviderAndGetCredential.
  // |succeeds| specifies whether we expect the first call to GetSerialization
  // on |testing_credential_| to succeed and start a GLS process or not.
  // If false, we will check that an appropriate error has been returned.
  HRESULT StartLogonProcess(bool succeeds);
  // The below method provides additional control of expecting a specific
  // error message whenever the logon process is expected to fail.
  // The |expected_error_message| is the id of the error message defined
  // under gaia_resources.grd file.
  HRESULT StartLogonProcess(bool succeeds, int expected_error_message);

  // Waits for the GLS process that was started in StartLogonProcess to
  // complete and returns.
  HRESULT WaitForLogonProcess();

  // Combines StartLogonProcess and WaitForLogonProcess.
  HRESULT StartLogonProcessAndWait();

  // Calls the final GetSerialization on the |testing_credential_| to complete
  // the logon process. |expected_success| specifies whether the final
  // GetSerialization is expected to succeed.
  // |expected_credentials_change_fired| specifies if a credential changed fired
  // event should have been detected by the provider. |expected_error_message|
  // is the error message that is expected. This message only applies if
  // |expected_success| is false.
  // This function combines the calls to FinishLogonProcessWithPred and
  // ReportLogonProcessResult which can be called separately to perform extra
  // operations between the last GetSerialization and the call to ReportResult.
  HRESULT FinishLogonProcess(bool expected_success,
                             bool expected_credentials_change_fired,
                             int expected_error_message);
  HRESULT FinishLogonProcess(bool expected_success,
                             bool expected_credentials_change_fired,
                             const std::wstring& expected_error_message);
  HRESULT FinishLogonProcessWithCred(
      bool expected_success,
      bool expected_credentials_change_fired,
      int expected_error_message,
      const Microsoft::WRL::ComPtr<ICredentialProviderCredential>&
          local_testing_cred);
  HRESULT FinishLogonProcessWithCred(
      bool expected_success,
      bool expected_credentials_change_fired,
      const std::wstring& expected_error_message,
      const Microsoft::WRL::ComPtr<ICredentialProviderCredential>&
          local_testing_cred);
  HRESULT ReportLogonProcessResult(
      const Microsoft::WRL::ComPtr<ICredentialProviderCredential>&
          local_testing_cred);

 private:
  HRESULT InternalInitializeProvider(
      const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
      DWORD* count);
  HRESULT ApplyProviderFilter(
      const Microsoft::WRL::ComPtr<ICredentialProvider>& provider,
      const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
      CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_out,
      HRESULT* update_remote_credentials_hr);

  registry_util::RegistryOverrideManager registry_override_;

  FakeOSProcessManager fake_os_process_manager_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
  FakeScopedUserProfileFactory fake_scoped_user_profile_factory_;
  FakeInternetAvailabilityChecker fake_internet_checker_;
  FakeAssociatedUserValidator fake_associated_user_validator_;
  FakePasswordRecoveryManager fake_password_recovery_manager_;
  FakeGemDeviceDetailsManager fake_gem_device_details_manager_;
  FakeWinHttpUrlFetcherFactory fake_http_url_fetcher_factory_;
  FakeCredentialProviderEvents fake_provider_events_;
  FakeCredentialProviderCredentialEvents
      fake_credential_provider_credential_events_;
  FakeCredentialProviderUserArray fake_user_array_;
  FakeChromeAvailabilityChecker fake_chrome_checker_;

  // SID of the user that is considered to be locking the workstation. This is
  // only relevant for CPUS_UNLOCK_WORKSTATION usage.
  std::wstring sid_locking_workstation_;

  // Reference to the provider that is created and owned by this class.
  Microsoft::WRL::ComPtr<ICredentialProvider> gaia_provider_;

  // Reference to the credential in provider that is being tested by this class.
  // This member is kept so that it can be automatically released on destruction
  // of the test if the test did not explicitly release it. This allows us to
  // write less boiler plate test code and ensures that proper destruction order
  // of the credentials is respected.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> testing_cred_;

  // Keeps track of whether a logon process has started for |testing_cred_|.
  // Testers who do not explicitly call FinishLogonProcess before the end of
  // their test will leave the completion of the logon process to the TearDown
  // of this class.
  bool logon_process_started_successfully_ = false;

  // The current usage scenario that this test is running. This should be
  // set before |gaia_provider_| is set.
  CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus_;

  // Default response returned by |fake_http_url_fetcher_factory_| when checking
  // for token handle validity.
  std::string default_token_handle_response_;

  base::ScopedTempDir scoped_temp_program_files_dir_;
  base::ScopedTempDir scoped_temp_program_files_x86_dir_;
  base::ScopedTempDir scoped_temp_progdata_dir_;
  std::unique_ptr<base::ScopedPathOverride> program_files_override_;
  std::unique_ptr<base::ScopedPathOverride> program_files_x86_override_;
  std::unique_ptr<base::ScopedPathOverride> programdata_override_;
};

}  // namespace testing

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_GLS_RUNNER_TEST_BASE_H_
