// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gls_runner_test_base.h"

#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_filter.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/multiprocess_func_list.h"

namespace credential_provider {

namespace switches {

constexpr char kDefaultExitCode[] = "default-exit-code";
constexpr char kIgnoreExpectedGaiaId[] = "ignore-expected-gaia-id";
constexpr char kGlsUserEmail[] = "gls-user-email";
constexpr char kStartGlsEventName[] = "start-gls-event-name";
constexpr char kOverrideGaiaId[] = "override-gaia-id";
constexpr char kOverrideGaiaPassword[] = "override-gaia-password";
constexpr char kOverrideFullName[] = "override-full-name";

}  // namespace switches

namespace testing {

// Corresponding default email and username for tests that don't override them.
const char kDefaultEmail[] = "foo@gmail.com";
const char kDefaultGaiaId[] = "test-gaia-id";
const wchar_t kDefaultUsername[] = L"foo";
const char kDefaultInvalidTokenHandleResponse[] = "{}";
const char kDefaultValidTokenHandleResponse[] = "{\"expires_in\":1}";

namespace {

// Generates a common signin result given an email pass through the command
// line and writes this result to stdout.  This is used as a fake GLS process
// for testing.
MULTIPROCESS_TEST_MAIN(gls_main) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // If a start event name is specified, the process waits for an event from the
  // tester telling it that it can start running.
  if (command_line->HasSwitch(switches::kStartGlsEventName)) {
    std::wstring start_event_name =
        command_line->GetSwitchValueNative(switches::kStartGlsEventName);
    if (!start_event_name.empty()) {
      base::win::ScopedHandle start_event_handle(
          ::CreateEvent(nullptr, false, false, start_event_name.c_str()));
      if (start_event_handle.IsValid()) {
        base::WaitableEvent start_event(std::move(start_event_handle));
        start_event.Wait();
      }
    }
  }

  int default_exit_code = kUiecSuccess;
  EXPECT_TRUE(base::StringToInt(
      command_line->GetSwitchValueASCII(switches::kDefaultExitCode),
      &default_exit_code));
  std::string gls_email =
      command_line->GetSwitchValueASCII(switches::kGlsUserEmail);
  std::string gaia_id_override =
      command_line->GetSwitchValueASCII(switches::kOverrideGaiaId);
  std::string gaia_password =
      command_line->GetSwitchValueASCII(switches::kOverrideGaiaPassword);
  std::string full_name =
      command_line->GetSwitchValueASCII(switches::kOverrideFullName);
  std::string expected_gaia_id =
      command_line->GetSwitchValueASCII(kGaiaIdSwitch);
  std::string expected_email =
      command_line->GetSwitchValueASCII(kPrefillEmailSwitch);
  if (expected_email.empty()) {
    expected_email = gls_email;
  } else {
    EXPECT_EQ(gls_email, std::string());
  }
  if (expected_gaia_id.empty())
    expected_gaia_id = kDefaultGaiaId;

  if (gaia_password.empty())
    gaia_password = "password";

  if (full_name.empty())
    full_name = "Full Name";

  if (command_line->HasSwitch(switches::kIgnoreExpectedGaiaId)) {
    DCHECK(!gaia_id_override.empty());
    expected_gaia_id = gaia_id_override;
  }

  base::Value::Dict dict;
  if (!gaia_id_override.empty() && gaia_id_override != expected_gaia_id) {
    dict.Set(kKeyExitCode, kUiecEMailMissmatch);
  } else {
    dict.Set(kKeyExitCode, static_cast<UiExitCodes>(default_exit_code));
    dict.Set(kKeyEmail, expected_email);
    dict.Set(kKeyFullname, full_name);
    dict.Set(kKeyId, expected_gaia_id);
    dict.Set(kKeyAccessToken, "at-123456");
    dict.Set(kKeyMdmIdToken, "idt-123456");
    dict.Set(kKeyPassword, gaia_password);
    dict.Set(kKeyRefreshToken, "rt-123456");
    dict.Set(kKeyTokenHandle, "th-123456");
  }

  std::string json;
  if (!base::JSONWriter::Write(dict, &json))
    return -1;

  HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD written;
  if (::WriteFile(hstdout, json.c_str(), json.length(), &written, nullptr)) {
    return 0;
  }

  return -1;
}

}  // namespace

GlsRunnerTestBase::GlsRunnerTestBase()
    : cpus_(CPUS_LOGON),
      default_token_handle_response_(kDefaultValidTokenHandleResponse) {}

GlsRunnerTestBase::~GlsRunnerTestBase() = default;

void GlsRunnerTestBase::SetUp() {
  // Create the special gaia account used to run GLS and save its password.
  BSTR sid;
  DWORD error;
  EXPECT_EQ(S_OK, fake_os_user_manager()->AddUser(
                      kDefaultGaiaAccountName, L"password", L"fullname",
                      L"comment", true, &sid, &error));

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  EXPECT_EQ(S_OK, policy->StorePrivateData(kLsaKeyGaiaUsername,
                                           kDefaultGaiaAccountName));
  EXPECT_EQ(S_OK, policy->StorePrivateData(kLsaKeyGaiaPassword, L"password"));

  // Make sure not to read random GCPW settings from the machine that is running
  // the tests.
  InitializeRegistryOverrideForTesting(&registry_override_);

  // Override location of "Program Files" system folder and its x86 version so
  // we don't modify local machine settings.
  ASSERT_TRUE(scoped_temp_program_files_dir_.CreateUniqueTempDir());
  program_files_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_PROGRAM_FILES, scoped_temp_program_files_dir_.GetPath());
  ASSERT_TRUE(scoped_temp_program_files_x86_dir_.CreateUniqueTempDir());
  program_files_x86_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_PROGRAM_FILESX86, scoped_temp_program_files_x86_dir_.GetPath());

  // Also override location of "ProgramData" system folder as we store user
  // policies there.
  ASSERT_TRUE(scoped_temp_progdata_dir_.CreateUniqueTempDir());
  programdata_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_COMMON_APP_DATA, scoped_temp_progdata_dir_.GetPath());
}

void GlsRunnerTestBase::TearDown() {
  // If credential has not been explicitly completed and the logon process
  // was started, then complete here under the assumption that it will
  // complete successfully.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  ASSERT_EQ(S_OK, ReleaseProvider());
}

HRESULT GlsRunnerTestBase::ReleaseProvider() {
  if (!gaia_provider_)
    return S_OK;

  // If any logon are still pending, they are about to be killed now.
  logon_process_started_successfully_ = false;

  HRESULT hr = S_OK;
  // Complete the release of the provider.
  // Unadvise all the credentials.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  HRESULT get_count_hr =
      gaia_provider_->GetCredentialCount(&count, &default_index, &autologon);
  if (SUCCEEDED(get_count_hr)) {
    for (DWORD i = 0; i < count; ++i) {
      Microsoft::WRL::ComPtr<ICredentialProviderCredential> credential;
      HRESULT get_hr = gaia_provider_->GetCredentialAt(i, &credential);
      EXPECT_EQ(get_hr, S_OK);
      if (SUCCEEDED(get_hr)) {
        get_hr = credential->UnAdvise();
        if (FAILED(get_hr))
          hr = get_hr;
      } else {
        hr = get_hr;
      }
    }
  } else {
    hr = get_count_hr;
  }

  // Unadvise the provider.
  HRESULT unadvise_hr = gaia_provider_->UnAdvise();
  if (FAILED(unadvise_hr))
    hr = unadvise_hr;
  gaia_provider_.Reset();

  return hr;
}

HRESULT
GlsRunnerTestBase::InitializeProviderWithCredentials(
    DWORD* credential_count,
    ICredentialProvider** provider) {
  HRESULT hr = InternalInitializeProvider(nullptr, credential_count);
  if (FAILED(hr))
    return hr;

  return gaia_provider_.CopyTo(IID_PPV_ARGS(provider));
}

HRESULT GlsRunnerTestBase::InitializeProviderWithRemoteCredentials(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
    ICredentialProvider** provider) {
  HRESULT hr = InternalInitializeProvider(pcpcs_in, nullptr);
  if (FAILED(hr))
    return hr;

  return gaia_provider_.CopyTo(IID_PPV_ARGS(provider));
}

HRESULT GlsRunnerTestBase::InitializeProviderAndGetCredential(
    DWORD index,
    ICredentialProviderCredential** credential) {
  DCHECK(credential);

  *credential = nullptr;
  DWORD count = 0;
  HRESULT hr = InternalInitializeProvider(nullptr, &count);
  if (FAILED(hr))
    return hr;

  if (index >= count)
    return E_FAIL;

  // Reference specific credential that was requested.
  hr = gaia_provider_->GetCredentialAt(index, &testing_cred_);
  if (FAILED(hr))
    return hr;

  EXPECT_EQ(S_OK, testing_cred_.CopyTo(IID_PPV_ARGS(credential)));
  return S_OK;
}

HRESULT GlsRunnerTestBase::InternalInitializeProvider(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
    DWORD* count) {
  if (count)
    *count = 0;

  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  HRESULT hr =
      CComCreator<CComObject<CTestGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_PPV_ARGS(&provider));
  if (FAILED(hr))
    return hr;

  // Apply the filter and get remote credentials as needed.
  {
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION remote_credentials;
    HRESULT update_remote_credentials_hr;
    hr = ApplyProviderFilter(provider, pcpcs_in, &remote_credentials,
                             &update_remote_credentials_hr);
    if (FAILED(hr))
      return hr;

    // Start process for logon screen.
    hr = provider->SetUsageScenario(cpus_, 0);
    if (FAILED(hr))
      return hr;

    if (SUCCEEDED(update_remote_credentials_hr) &&
        remote_credentials.rgbSerialization) {
      // Apply remote credentials if any.
      if (remote_credentials.clsidCredentialProvider ==
          CLSID_GaiaCredentialProvider) {
        provider->SetSerialization(&remote_credentials);
      }
      ::CoTaskMemFree(remote_credentials.rgbSerialization);
    }
  }

  // Give list of users visible on welcome screen.
  Microsoft::WRL::ComPtr<ICredentialProviderSetUserArray> provider_user_array;
  hr = provider.As(&provider_user_array);
  if (FAILED(hr))
    return hr;

  // All users are shown if the usage is not for unlocking the workstation.
  bool all_users_shown = cpus_ != CPUS_UNLOCK_WORKSTATION;

  CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS cpao;
  ICredentialProviderUserArray* user_array = fake_user_array();
  hr = user_array->GetAccountOptions(&cpao);
  if (FAILED(hr))
    return hr;

  bool other_user_tile_available = cpao == CPAO_EMPTY_LOCAL;

  for (auto& sid_and_username : fake_os_user_manager_.GetUsers()) {
    // If not all the users are shown, the user that locked the system is
    // the only one that is in the user array (if the other user tile is
    // not available).
    if (!all_users_shown &&
        ((!sid_locking_workstation_.empty() &&
          sid_locking_workstation_ != sid_and_username.first) ||
         other_user_tile_available)) {
      continue;
    }

    // Don't add the gaia special account into the fake user array.
    if (sid_and_username.second == kDefaultGaiaAccountName)
      continue;

    fake_user_array_.AddUser(sid_and_username.first.c_str(),
                             sid_and_username.second.c_str());
  }

  hr = provider_user_array->SetUserArray(&fake_user_array_);
  if (FAILED(hr))
    return hr;

  // Activate the CP.
  hr = provider->Advise(fake_provider_events(), 0);
  if (FAILED(hr))
    return hr;

  // This class can now take ownership of the provider so that it can
  // be correctly uninitialized later.
  gaia_provider_ = provider;

  // GetCredentialCount must be called to initialize the credentials (if
  // desired).
  if (count) {
    DWORD default_index;
    BOOL autologon;
    hr = gaia_provider_->GetCredentialCount(count, &default_index, &autologon);
    if (FAILED(hr))
      return hr;

    EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
    EXPECT_FALSE(autologon);

    // Advise all the credentials
    for (DWORD i = 0; i < *count; ++i) {
      Microsoft::WRL::ComPtr<ICredentialProviderCredential> current_credential;
      hr = gaia_provider_->GetCredentialAt(i, &current_credential);
      if (FAILED(hr))
        return hr;

      hr = current_credential->Advise(
          fake_credential_provider_credential_events());
      if (FAILED(hr))
        return hr;
    }
  }

  // Verify fields.
  DWORD field_count;
  hr = gaia_provider_->GetFieldDescriptorCount(&field_count);
  if (FAILED(hr))
    return hr;

  for (DWORD i = 0; i < field_count; ++i) {
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* ppcpfd;
    hr = gaia_provider_->GetFieldDescriptorAt(i, &ppcpfd);
    if (FAILED(hr))
      return hr;
  }

  // Initialize the default field states by calling GetFieldState of
  // ICredentialProviderCredential.
  for (DWORD i = 0; count && i < *count; ++i) {
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> current_credential;
    hr = gaia_provider_->GetCredentialAt(i, &current_credential);
    if (FAILED(hr))
      return hr;

    for (DWORD fieldID = 0; fieldID < field_count; fieldID++) {
      CREDENTIAL_PROVIDER_FIELD_STATE cpfs;
      CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
      hr = current_credential->GetFieldState(fieldID, &cpfs, &cpfis);
      if (FAILED(hr))
        return hr;

      hr = fake_credential_provider_credential_events()->SetFieldState(
          current_credential.Get(), fieldID, cpfs);
      if (FAILED(hr))
        return hr;
    }
  }

  return S_OK;
}

HRESULT GlsRunnerTestBase::ApplyProviderFilter(
    const Microsoft::WRL::ComPtr<ICredentialProvider>& provider,
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_out,
    HRESULT* update_remote_credentials_hr) {
  if (update_remote_credentials_hr)
    *update_remote_credentials_hr = E_NOTIMPL;

  // Filter only lives long enough to apply filter and get serialization
  // credentials.
  Microsoft::WRL::ComPtr<ICredentialProviderFilter> filter;
  HRESULT hr =
      CComCreator<CComObject<CGaiaCredentialProviderFilter>>::CreateInstance(
          nullptr, IID_ICredentialProviderFilter, (void**)&filter);
  if (FAILED(hr))
    return hr;

  // Set token fetch result before starting the filter.
  fake_http_url_fetcher_factory_.SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), default_token_handle_response_);

  // Start initial refresh of token handles. The filter will apply user access
  // restrictions as needed.
  fake_associated_user_validator_.StartRefreshingTokenHandleValidity();

  // Perform initial filter code.
  GUID CLSID_SystemCredProvider1 = {
      0x11111111,
      0x2222,
      0x3333,
      {0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}};
  GUID CLSID_SystemCredProvider2 = {
      0x11111211,
      0x2122,
      0x3333,
      {0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}};
  GUID provider_guids[] = {CLSID_GaiaCredentialProvider,
                           CLSID_SystemCredProvider1,
                           CLSID_SystemCredProvider2};
  BOOL provider_allow[] = {TRUE, TRUE, TRUE};
  DWORD provider_count = 3;
  hr = filter->Filter(cpus_, 0, provider_guids, provider_allow, provider_count);

  // None of the system CLSID should be filtered out.
  EXPECT_EQ(TRUE, provider_allow[1]);
  EXPECT_EQ(TRUE, provider_allow[2]);

  BOOL all_providers_allowed =
      provider_allow[0] && provider_allow[1] && provider_allow[2];

  if (FAILED(hr))
    return hr;
  else if (!all_providers_allowed)
    return E_FAIL;

  // Apply remote credentials if any.
  if (pcpcs_in && pcpcs_out && update_remote_credentials_hr)
    *update_remote_credentials_hr =
        filter->UpdateRemoteCredential(pcpcs_in, pcpcs_out);

  return S_OK;
}

HRESULT GlsRunnerTestBase::StartLogonProcess(bool succeeds) {
  return StartLogonProcess(succeeds, 0);
}

HRESULT GlsRunnerTestBase::StartLogonProcess(bool succeeds,
                                             int expected_error_message) {
  DCHECK(testing_cred_);
  DCHECK(!logon_process_started_successfully_);
  BOOL auto_login;
  EXPECT_EQ(S_OK, testing_cred_->SetSelected(&auto_login));

  // Logging on is an async process, so the call to GetSerialization() starts
  // the process, but when it returns it has not completed.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  wchar_t* status_text;
  EXPECT_EQ(S_OK, testing_cred_->GetSerialization(&cpgsr, &cpcs, &status_text,
                                                  &status_icon));
  EXPECT_EQ(CPSI_NONE, status_icon);
  if (succeeds) {
    EXPECT_EQ(nullptr, status_text);
    EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);
    logon_process_started_successfully_ = true;
  } else {
    EXPECT_NE(nullptr, status_text);
    EXPECT_EQ(CPGSR_NO_CREDENTIAL_FINISHED, cpgsr);
  }

  if (expected_error_message != 0) {
    EXPECT_STREQ(status_text,
                 GetStringResource(expected_error_message).c_str());
  }

  return S_OK;
}

HRESULT GlsRunnerTestBase::WaitForLogonProcess() {
  Microsoft::WRL::ComPtr<testing::ITestCredential> test;
  HRESULT hr = testing_cred_.As(&test);
  if (FAILED(hr))
    return hr;
  return test->WaitForGls();
}

HRESULT GlsRunnerTestBase::StartLogonProcessAndWait() {
  HRESULT hr = StartLogonProcess(/*succeeds=*/true);
  if (FAILED(hr))
    return hr;
  return WaitForLogonProcess();
}

// static
HRESULT GlsRunnerTestBase::GetFakeGlsCommandline(
    UiExitCodes default_exit_code,
    const std::string& gls_email,
    const std::string& gaia_id_override,
    const std::string& gaia_password,
    const std::string& full_name_override,
    const std::wstring& start_gls_event_name,
    bool ignore_expected_gaia_id,
    base::CommandLine* command_line) {
  *command_line = base::GetMultiProcessTestChildBaseCommandLine();
  command_line->AppendSwitchASCII(::switches::kTestChildProcess, "gls_main");
  command_line->AppendSwitchASCII(switches::kGlsUserEmail, gls_email);
  command_line->AppendSwitchNative(switches::kDefaultExitCode,
                                   base::NumberToWString(default_exit_code));

  if (ignore_expected_gaia_id)
    command_line->AppendSwitch(switches::kIgnoreExpectedGaiaId);

  if (!gaia_id_override.empty()) {
    command_line->AppendSwitchASCII(switches::kOverrideGaiaId,
                                    gaia_id_override);
  }

  if (!gaia_password.empty()) {
    command_line->AppendSwitchASCII(switches::kOverrideGaiaPassword,
                                    gaia_password);
  }

  if (!start_gls_event_name.empty()) {
    command_line->AppendSwitchNative(switches::kStartGlsEventName,
                                     start_gls_event_name);
  }

  if (!full_name_override.empty()) {
    command_line->AppendSwitchASCII(switches::kOverrideFullName,
                                    full_name_override);
  }

  return S_OK;
}

HRESULT GlsRunnerTestBase::FinishLogonProcess(
    bool expected_success,
    bool expected_credentials_change_fired,
    int expected_error_message) {
  return FinishLogonProcess(
      expected_success, expected_credentials_change_fired,
      expected_error_message ? GetStringResource(expected_error_message) : L"");
}

HRESULT GlsRunnerTestBase::FinishLogonProcess(
    bool expected_success,
    bool expected_credentials_change_fired,
    const std::wstring& expected_error_message) {
  // If no logon process was started, there is nothing to finish.
  if (!logon_process_started_successfully_)
    return S_OK;

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> local_testing_cred =
      testing_cred_;

  // Release ownership on the testing_cred_ which should be finishing.
  testing_cred_.Reset();

  HRESULT hr = FinishLogonProcessWithCred(
      expected_success, expected_credentials_change_fired,
      expected_error_message, local_testing_cred);

  if (!fake_os_user_manager()->DoesOperationFail(
          FAILEDOPERATIONS::CHANGE_PASSWORD)) {
    EXPECT_EQ(hr, S_OK);
  }

  if (FAILED(hr))
    return hr;

  if (expected_credentials_change_fired) {
    hr = ReportLogonProcessResult(local_testing_cred);
    EXPECT_EQ(hr, S_OK);
    return hr;
  }

  return S_OK;
}

HRESULT GlsRunnerTestBase::FinishLogonProcessWithCred(
    bool expected_success,
    bool expected_credentials_change_fired,
    int expected_error_message,
    const Microsoft::WRL::ComPtr<ICredentialProviderCredential>&
        local_testing_cred) {
  return FinishLogonProcessWithCred(
      expected_success, expected_credentials_change_fired,
      expected_error_message ? GetStringResource(expected_error_message) : L"",
      local_testing_cred);
}

HRESULT GlsRunnerTestBase::FinishLogonProcessWithCred(
    bool expected_success,
    bool expected_credentials_change_fired,
    const std::wstring& expected_error_message,
    const Microsoft::WRL::ComPtr<ICredentialProviderCredential>&
        local_testing_cred) {
  // If no logon process was started, there is nothing to finish.
  if (!logon_process_started_successfully_)
    return S_OK;

  logon_process_started_successfully_ = false;
  DCHECK(gaia_provider_);

  Microsoft::WRL::ComPtr<ITestCredential> test_cred;
  HRESULT hr = local_testing_cred.As(&test_cred);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<ITestCredentialProvider> test_provider;
  hr = gaia_provider_.As(&test_provider);
  if (FAILED(hr))
    return hr;

  EXPECT_EQ(test_provider->credentials_changed_fired(),
            expected_success && expected_credentials_change_fired);

  if (!expected_success) {
    // Check that values were not propagated to the provider.
    EXPECT_EQ(0u, test_provider->username().Length());
    EXPECT_EQ(0u, test_provider->password().Length());
    EXPECT_EQ(0u, test_provider->sid().Length());

    if (!expected_error_message.empty()) {
      EXPECT_STREQ(test_cred->GetErrorText(), expected_error_message.c_str());
    } else {
      EXPECT_EQ(test_cred->GetErrorText(), nullptr);
    }
    return S_OK;
  } else {
    // Also extract other registration related fields and verify if those are
    // non-empty.
    EXPECT_TRUE(test_cred->ContainsIsAdJoinedUser());
  }

  // Call final GetSerialization and expect it to be finished.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  hr = local_testing_cred->GetSerialization(&cpgsr, &cpcs, &status_text,
                                            &status_icon);

  if (FAILED(hr))
    return hr;

  // Credentials not valid, login doesn't go through.
  if (test_cred->AreCredentialsValid()) {
    EXPECT_EQ(nullptr, status_text);
    EXPECT_EQ(CPSI_SUCCESS, status_icon);
    EXPECT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
    EXPECT_LT(0u, cpcs.cbSerialization);
    EXPECT_NE(nullptr, cpcs.rgbSerialization);
  } else {
    EXPECT_EQ(CPSI_ERROR, status_icon);
    EXPECT_EQ(CPGSR_RETURN_NO_CREDENTIAL_FINISHED, cpgsr);
    // The credential provider has not serialized a credential,
    // but has completed its work. This will force the logon UI to
    // return, which will call UnAdvise for all the credential providers.
    return E_FAIL;
  }

  // Check that values were propagated to the provider.
  if (expected_credentials_change_fired) {
    EXPECT_NE(0u, test_provider->username().Length());
    EXPECT_NE(0u, test_provider->password().Length());
    EXPECT_NE(0u, test_provider->sid().Length());
  }

  EXPECT_EQ(test_cred->GetErrorText(), nullptr);

  return S_OK;
}

HRESULT GlsRunnerTestBase::ReportLogonProcessResult(
    const Microsoft::WRL::ComPtr<ICredentialProviderCredential>&
        local_testing_cred) {
  Microsoft::WRL::ComPtr<ITestCredential> test_cred;
  HRESULT hr = local_testing_cred.As(&test_cred);
  if (FAILED(hr))
    return hr;

  // State was not reset.
  EXPECT_TRUE(test_cred->AreCredentialsValid());
  wchar_t* report_status_text = nullptr;
  CREDENTIAL_PROVIDER_STATUS_ICON report_icon;
  hr =
      local_testing_cred->ReportResult(0, 0, &report_status_text, &report_icon);
  if (FAILED(hr))
    return hr;

  // State was reset.
  EXPECT_FALSE(test_cred->AreCredentialsValid());

  return S_OK;
}

}  // namespace testing

}  // namespace credential_provider
