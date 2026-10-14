// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/device_policies_manager.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <thread>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

namespace {

using ::Microsoft::WRL::ClassicCom;
using ::Microsoft::WRL::ComPtr;
using ::Microsoft::WRL::Make;
using ::Microsoft::WRL::RuntimeClass;
using ::Microsoft::WRL::RuntimeClassFlags;

class FakeUpdaterSystem
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IUpdaterSystem> {
 public:
  using FetchPoliciesHandler =
      base::RepeatingCallback<HRESULT(IUpdaterCallbackSystem*)>;

  explicit FakeUpdaterSystem(FetchPoliciesHandler handler)
      : handler_(std::move(handler)) {}
  FakeUpdaterSystem(const FakeUpdaterSystem&) = delete;
  FakeUpdaterSystem& operator=(const FakeUpdaterSystem&) = delete;

  int fetch_policies_call_count() const { return fetch_policies_call_count_; }

  // IUpdaterSystem overrides:
  IFACEMETHODIMP FetchPolicies(IUpdaterCallbackSystem* callback) override {
    ++fetch_policies_call_count_;
    return handler_ ? handler_.Run(callback) : S_OK;
  }

  IFACEMETHODIMP GetVersion(BSTR* version) override { return E_NOTIMPL; }
  IFACEMETHODIMP RegisterApp(const WCHAR* app_id,
                             const WCHAR* brand_code,
                             const WCHAR* brand_path,
                             const WCHAR* tag,
                             const WCHAR* version,
                             const WCHAR* existence_checker_path,
                             IUpdaterCallbackSystem* callback) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP RunPeriodicTasks(IUpdaterCallbackSystem* callback) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP CheckForUpdate(const WCHAR* app_id,
                                LONG priority,
                                BOOL same_version_update_allowed,
                                IUpdaterObserverSystem* observer) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP Update(const WCHAR* app_id,
                        const WCHAR* install_data_index,
                        LONG priority,
                        BOOL same_version_update_allowed,
                        IUpdaterObserverSystem* observer) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP UpdateAll(IUpdaterObserverSystem* observer) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP Install(const WCHAR* app_id,
                         const WCHAR* brand_code,
                         const WCHAR* brand_path,
                         const WCHAR* tag,
                         const WCHAR* version,
                         const WCHAR* existence_checker_path,
                         const WCHAR* client_install_data,
                         const WCHAR* install_data_index,
                         LONG priority,
                         IUpdaterObserverSystem* observer) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP CancelInstalls(const WCHAR* app_id) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP RunInstaller(const WCHAR* app_id,
                              const WCHAR* installer_path,
                              const WCHAR* install_args,
                              const WCHAR* install_data,
                              const WCHAR* install_settings,
                              IUpdaterObserverSystem* observer) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetAppStates(
      IUpdaterAppStatesCallbackSystem* callback) override {
    return E_NOTIMPL;
  }

 private:
  ~FakeUpdaterSystem() override = default;

  FetchPoliciesHandler handler_;
  int fetch_policies_call_count_ = 0;
};

}  // namespace

class GcpDevicePoliciesBaseTest : public GlsRunnerTestBase {
 protected:
  void SetUp() override;
};

void GcpDevicePoliciesBaseTest::SetUp() {
  GlsRunnerTestBase::SetUp();

  // Remove the mdm_url value which exists by default as it's added in
  // InitializeRegistryOverrideForTesting.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(kRegMdmUrl));

  FakesForTesting fakes;
  fakes.fake_win_http_url_fetcher_creator =
      fake_http_url_fetcher_factory()->GetCreatorCallback();
  fakes.os_user_manager_for_testing = fake_os_user_manager();
  UserPoliciesManager::Get()->SetFakesForTesting(&fakes);
}

TEST_F(GcpDevicePoliciesBaseTest, NewUserAssociationWithNoUserPoliciesPresent) {
  FakeUserPoliciesManager fake_user_policies_manager(true);

  // Create a few fake users associated to fake gaia ids.
  std::vector<std::wstring> sids;
  const size_t num_users_needed = 3;
  for (size_t i = 0; i < num_users_needed; ++i) {
    base::win::ScopedBstr sid_str;
    const std::wstring i_str = base::NumberToWString(i);
    std::wstring username = L"new-user-" + i_str;
    GaiaId gaia_id("gaia-id-" + base::NumberToString(i));
    std::wstring email = base::StrCat({L"user_", i_str, L"@company.com"});
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        username, L"password", L"Full Name", L"comment",
                        gaia_id, email, sid_str.Receive()));
    sids.push_back(sid_str.Get());
  }

  // Create an existing user association in registry but with an invalid sid.
  base::win::RegKey key;
  std::wstring key_name =
      std::wstring(kGcpUsersRootKeyName) + L"\\non-existent-user-sid";
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"email", L"invalid-user@company.com"));

  // Add user cloud policies only for the first two users.
  UserPolicies first_user_policy;
  first_user_policy.enable_dm_enrollment = false;
  first_user_policy.enable_gcpw_auto_update = false;
  first_user_policy.enable_multi_user_login = false;
  first_user_policy.gcpw_pinned_version = GcpwVersion("100.1.2.3");

  fake_user_policies_manager.SetUserPolicies(sids[0], first_user_policy);

  UserPolicies second_user_policy = first_user_policy;
  second_user_policy.enable_dm_enrollment = true;
  second_user_policy.gcpw_pinned_version = GcpwVersion("102.1.2.4");
  fake_user_policies_manager.SetUserPolicies(sids[1], second_user_policy);

  // Create a device policy by merging the two users with cloud policies.
  DevicePolicies merged_device_policy =
      DevicePolicies::FromUserPolicies(first_user_policy);
  merged_device_policy.MergeWith(
      DevicePolicies::FromUserPolicies(second_user_policy));

  // Get the resolved device policy.
  DevicePolicies device_policy;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policy);

  // The resolved policy should reflect only the policies of the users with
  // existing cloud policies.
  ASSERT_EQ(merged_device_policy, device_policy);
}

// Tests that existing registry values that control device policies are honored
// correctly when present.
// Parameters are:
// 1. int  0: "enable_dm_enrollment" flag is set to 0.
//         1: "enable_dm_enrollment" flag is set to 1.
//         2: "enable_dm_enrollment" flag is not set.
// 2. int  0: "mdm" flag for MDM url is set to "".
//         1: "mdm" flag for MDM url is set to some valid value.
//         2: "mdm" flag for MDM url is not set.
// 3. int  0: "enable_multi_user_login" flag is set to 0.
//         1: "enable_multi_user_login" flag is set to 1.
//         2: "enable_multi_user_login" flag is not set.
class GcpDevicePoliciesRegistryTest
    : public GcpDevicePoliciesBaseTest,
      public ::testing::WithParamInterface<std::tuple<int, int, int>> {};

TEST_P(GcpDevicePoliciesRegistryTest, DefaultValues) {
  int dm_enrollment_flag = std::get<0>(GetParam());
  int mdm_url_flag = std::get<1>(GetParam());
  int multi_user_login_flag = std::get<2>(GetParam());

  if (dm_enrollment_flag < 2) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment,
                                            dm_enrollment_flag ? 1 : 0));
  }

  if (mdm_url_flag == 1) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  } else if (mdm_url_flag == 0) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kRegMdmUrl, L""));
  }

  if (multi_user_login_flag < 2) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser,
                                            multi_user_login_flag ? 1 : 0));
  }

  DevicePolicies default_device_policies;

  // Enabled unless explicitly forbidden through setting either registry flags.
  bool enable_dm_enrollment = !(!dm_enrollment_flag || !mdm_url_flag);

  ASSERT_EQ(enable_dm_enrollment, default_device_policies.enable_dm_enrollment);
  ASSERT_EQ(multi_user_login_flag > 0,
            default_device_policies.enable_multi_user_login);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpDevicePoliciesRegistryTest,
                         ::testing::Combine(::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2)));

// Tests that the merging of two device policies does not lead to a more
// restrictive policy.
// Parameters are:
// 1. bool : Whether MDM enrollment is enabled.
// 2. bool : Whether GCPW auto update through Omaha is enabled.
// 3. bool : Whether multi user mode is enabled.
// 4. string : Version of GCPW to use.
class GcpDevicePoliciesMergeTest
    : public GcpDevicePoliciesBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, bool, const char*>> {};

TEST_P(GcpDevicePoliciesMergeTest, OtherUser) {
  UserPolicies new_user_policy;
  new_user_policy.enable_dm_enrollment = std::get<0>(GetParam());
  new_user_policy.enable_gcpw_auto_update = std::get<1>(GetParam());
  new_user_policy.enable_multi_user_login = std::get<2>(GetParam());
  new_user_policy.gcpw_pinned_version = GcpwVersion(std::get<3>(GetParam()));

  UserPolicies existing_user_policy;
  existing_user_policy.enable_dm_enrollment = true;
  existing_user_policy.enable_gcpw_auto_update = true;
  existing_user_policy.enable_multi_user_login = true;
  existing_user_policy.gcpw_pinned_version = GcpwVersion("100.1.2.3");

  // Create a device policy by merging the two users policies.
  DevicePolicies device_policy =
      DevicePolicies::FromUserPolicies(existing_user_policy);
  device_policy.MergeWith(DevicePolicies::FromUserPolicies(new_user_policy));

  // The new policy should allow everything the existing user was able to do
  // before.
  ASSERT_EQ(existing_user_policy.enable_dm_enrollment,
            device_policy.enable_dm_enrollment);
  ASSERT_EQ(existing_user_policy.enable_gcpw_auto_update,
            device_policy.enable_gcpw_auto_update);
  ASSERT_EQ(existing_user_policy.enable_multi_user_login,
            device_policy.enable_multi_user_login);

  // The GCPW version should be the latest allowed.
  GcpwVersion gcpw_version = std::max(existing_user_policy.gcpw_pinned_version,
                                      new_user_policy.gcpw_pinned_version);
  ASSERT_EQ(gcpw_version, device_policy.gcpw_pinned_version);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpDevicePoliciesMergeTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Values("99.1.2.3",
                                                              "100.1.2.3",
                                                              "100.1.2.4")));

// Base test for testing allowed domains policy scenarios.
class GcpDevicePoliciesAllowedDomainsBaseTest
    : public GcpDevicePoliciesBaseTest {
 public:
  void SetUp() override;
};

void GcpDevicePoliciesAllowedDomainsBaseTest::SetUp() {
  GcpDevicePoliciesBaseTest::SetUp();

  // Delete any existing registry entries. Setting to empty deletes them.
  SetGlobalFlagForTesting(L"ed", L"");
  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"");
}

// Test that correct allowed domains policy is obtained whether they are set in
// the registry or through the cloud policy.
// Parameters are:
// 1. bool : Whether domains set through Omaha cloud policy.
// 2. int  : 0 - Domains not set through registry.
//           1 - Domains set through deprecated "ed" registry entry.
//           2 - Domains set through "domains_allowed_to_login" registry entry.
// 3. string : List of domains from which users are allowed to login.
// 4. bool : Has existing user.
class GcpDevicePoliciesAllowedDomainsTest
    : public GcpDevicePoliciesAllowedDomainsBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, int, const wchar_t*, bool>> {};

TEST_P(GcpDevicePoliciesAllowedDomainsTest, OmahaPolicyTest) {
  bool has_omaha_domains_policy = std::get<0>(GetParam());
  bool has_registry_domains_policy = std::get<1>(GetParam()) != 0;
  bool use_old_domains_reg_key = std::get<1>(GetParam()) == 1;
  std::wstring allowed_domains_str(std::get<2>(GetParam()));
  bool has_existing_user = std::get<3>(GetParam());

  std::vector<std::wstring> allowed_domains = base::SplitString(
      allowed_domains_str, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  if (has_omaha_domains_policy) {
    ASSERT_TRUE(
        DevicePoliciesManager::Get()->SetAllowedDomainsOmahaPolicyForTesting(
            allowed_domains));
  }

  if (has_registry_domains_policy) {
    if (use_old_domains_reg_key) {
      SetGlobalFlagForTesting(L"ed", allowed_domains_str);
    } else {
      SetGlobalFlagForTesting(L"domains_allowed_to_login", allowed_domains_str);
    }
  }

  FakeUserPoliciesManager fake_user_policies_manager(true);
  UserPolicies user_policy;

  if (has_existing_user) {
    base::win::ScopedBstr sid;
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        kDefaultUsername, L"password", L"Full Name", L"comment",
                        kDefaultGaiaId, std::wstring(), sid.Receive()));
    // Add a random user policy.
    user_policy.enable_dm_enrollment = false;
    user_policy.enable_gcpw_auto_update = false;
    user_policy.enable_multi_user_login = false;
    user_policy.gcpw_pinned_version = GcpwVersion("100.1.2.3");

    fake_user_policies_manager.SetUserPolicies(sid.Get(), user_policy);
  }

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  if (has_omaha_domains_policy || has_registry_domains_policy)
    ASSERT_EQ(allowed_domains, device_policies.domains_allowed_to_login);

  if (has_existing_user) {
    ASSERT_EQ(user_policy.enable_dm_enrollment,
              device_policies.enable_dm_enrollment);
    ASSERT_EQ(user_policy.enable_gcpw_auto_update,
              device_policies.enable_gcpw_auto_update);
    ASSERT_EQ(user_policy.enable_multi_user_login,
              device_policies.enable_multi_user_login);
    ASSERT_EQ(user_policy.gcpw_pinned_version,
              device_policies.gcpw_pinned_version);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpDevicePoliciesAllowedDomainsTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(0, 1, 2),
        ::testing::Values(L"", L"acme.com", L"acme.com,acme.org"),
        ::testing::Bool()));

// Test to ensure Omaha policies override the existing registry settings for
// allowed domains policy.
// Parameters are:
// 1. bool : If true, deprecated "ed" registry entry is used. Otherwise
//           "domains_allowed_to_login" is used.
// 2. string : List of allowed domains for GCPW specified through registry.
// 3. string : List of allowed domains for GCPW specified through a Omaha cloud
//             policy.
class GcpDevicePoliciesOmahaDomainsWinTest
    : public GcpDevicePoliciesAllowedDomainsBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, const wchar_t*, const wchar_t*>> {};

TEST_P(GcpDevicePoliciesOmahaDomainsWinTest, TestConflict) {
  bool use_old_domains_reg_key = std::get<0>(GetParam());
  std::wstring domains_registry_str(std::get<1>(GetParam()));
  std::wstring domains_from_omaha_str(std::get<2>(GetParam()));

  std::vector<std::wstring> allowed_domains_registry = base::SplitString(
      domains_registry_str, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  std::vector<std::wstring> allowed_domains_omaha = base::SplitString(
      domains_from_omaha_str, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  ASSERT_TRUE(
      DevicePoliciesManager::Get()->SetAllowedDomainsOmahaPolicyForTesting(
          allowed_domains_omaha));

  if (use_old_domains_reg_key) {
    SetGlobalFlagForTesting(L"ed", domains_registry_str);
  } else {
    SetGlobalFlagForTesting(L"domains_allowed_to_login", domains_registry_str);
  }

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  if (!allowed_domains_omaha.empty()) {
    ASSERT_EQ(allowed_domains_omaha, device_policies.domains_allowed_to_login);
  } else {
    ASSERT_EQ(allowed_domains_registry,
              device_policies.domains_allowed_to_login);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpDevicePoliciesOmahaDomainsWinTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(L"", L"acme.com", L"acme.com,acme.org"),
        ::testing::Values(L"", L"company.com", L"company.com,company.org")));

class GcpDevicePoliciesDayZeroFetchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    InitializeRegistryOverrideForTesting(&registry_override_);

    base::win::RegKey key;
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
    EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(kRegMdmUrl));

    SetGlobalFlagForTesting(L"ed", L"");
    SetGlobalFlagForTesting(L"domains_allowed_to_login", L"");

    ASSERT_TRUE(scoped_temp_program_files_x86_dir_.CreateUniqueTempDir());
    program_files_x86_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_PROGRAM_FILESX86,
        scoped_temp_program_files_x86_dir_.GetPath());

    DevicePoliciesManager::SetPolicyFetchTimeoutForTesting(base::Seconds(3));
  }

  void TearDown() override {
    DevicePoliciesManager::SetUpdaterCreatorForTesting({});
    DevicePoliciesManager::SetPolicyFetchTimeoutForTesting(base::Seconds(3));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  base::ScopedTempDir scoped_temp_program_files_x86_dir_;
  std::unique_ptr<base::ScopedPathOverride> program_files_x86_override_;
  FakeOSUserManager fake_os_user_manager_;
};

TEST_F(GcpDevicePoliciesDayZeroFetchTest,
       SkipsUpdaterFetchWhenPolicyFileAlreadyExists) {
  std::vector<std::wstring> existing_domains = {L"existing.com"};
  ASSERT_TRUE(
      DevicePoliciesManager::Get()->SetAllowedDomainsOmahaPolicyForTesting(
          existing_domains));

  int creator_call_count = 0;
  DevicePoliciesManager::SetUpdaterCreatorForTesting(base::BindRepeating(
      [](int* call_count, IUpdaterSystem** updater) -> HRESULT {
        ++(*call_count);
        return S_OK;
      },
      &creator_call_count));

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  EXPECT_EQ(0, creator_call_count);
  EXPECT_EQ(existing_domains, device_policies.domains_allowed_to_login);
}

TEST_F(GcpDevicePoliciesDayZeroFetchTest,
       EnforcesDomainsFromSynchronousDayZeroFetch) {
  SetGlobalFlagForTesting(L"domains_allowed_to_login",
                          L"registry-fallback.com");

  std::vector<std::wstring> fetched_domains = {L"dayzero.com", L"corp.org"};
  auto fake_updater = Make<FakeUpdaterSystem>(base::BindRepeating(
      [](const std::vector<std::wstring>& domains,
         IUpdaterCallbackSystem* callback) -> HRESULT {
        EXPECT_TRUE(DevicePoliciesManager::Get()
                        ->SetAllowedDomainsOmahaPolicyForTesting(domains));
        callback->Run(0);
        return S_OK;
      },
      fetched_domains));

  DevicePoliciesManager::SetUpdaterCreatorForTesting(base::BindRepeating(
      [](ComPtr<FakeUpdaterSystem> fake, IUpdaterSystem** updater) -> HRESULT {
        return fake.CopyTo(updater);
      },
      fake_updater));

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  EXPECT_EQ(1, fake_updater->fetch_policies_call_count());
  EXPECT_EQ(fetched_domains, device_policies.domains_allowed_to_login);
}

TEST_F(GcpDevicePoliciesDayZeroFetchTest,
       WaitsForAsynchronousDayZeroFetchAcrossThreads) {
  SetGlobalFlagForTesting(L"domains_allowed_to_login",
                          L"registry-fallback.com");

  std::vector<std::wstring> fetched_domains = {L"async-dayzero.com"};
  std::thread background_worker;
  auto fake_updater = Make<FakeUpdaterSystem>(base::BindRepeating(
      [](std::thread* worker, const std::vector<std::wstring>& domains,
         IUpdaterCallbackSystem* callback) -> HRESULT {
        ComPtr<IUpdaterCallbackSystem> retained_callback(callback);
        *worker = std::thread([domains, retained_callback]() {
          base::PlatformThread::Sleep(base::Milliseconds(50));
          EXPECT_TRUE(DevicePoliciesManager::Get()
                          ->SetAllowedDomainsOmahaPolicyForTesting(domains));
          retained_callback->Run(0);
        });
        return S_OK;
      },
      &background_worker, fetched_domains));

  DevicePoliciesManager::SetUpdaterCreatorForTesting(base::BindRepeating(
      [](ComPtr<FakeUpdaterSystem> fake, IUpdaterSystem** updater) -> HRESULT {
        return fake.CopyTo(updater);
      },
      fake_updater));

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  if (background_worker.joinable()) {
    background_worker.join();
  }

  EXPECT_EQ(1, fake_updater->fetch_policies_call_count());
  EXPECT_EQ(fetched_domains, device_policies.domains_allowed_to_login);
}

TEST_F(GcpDevicePoliciesDayZeroFetchTest,
       FallsBackToRegistryWhenUpdaterComCreationFails) {
  SetGlobalFlagForTesting(L"domains_allowed_to_login",
                          L"registry-fallback.com");

  DevicePoliciesManager::SetUpdaterCreatorForTesting(base::BindRepeating(
      [](IUpdaterSystem** updater) -> HRESULT { return REGDB_E_CLASSNOTREG; }));

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  std::vector<std::wstring> expected = {L"registry-fallback.com"};
  EXPECT_EQ(expected, device_policies.domains_allowed_to_login);
}

TEST_F(GcpDevicePoliciesDayZeroFetchTest,
       FallsBackToRegistryWhenFetchPoliciesReturnsFailureHResult) {
  SetGlobalFlagForTesting(L"domains_allowed_to_login",
                          L"registry-fallback.com");

  auto fake_updater = Make<FakeUpdaterSystem>(base::BindRepeating(
      [](IUpdaterCallbackSystem* callback) -> HRESULT { return E_FAIL; }));

  DevicePoliciesManager::SetUpdaterCreatorForTesting(base::BindRepeating(
      [](ComPtr<FakeUpdaterSystem> fake, IUpdaterSystem** updater) -> HRESULT {
        return fake.CopyTo(updater);
      },
      fake_updater));

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  EXPECT_EQ(1, fake_updater->fetch_policies_call_count());
  std::vector<std::wstring> expected = {L"registry-fallback.com"};
  EXPECT_EQ(expected, device_policies.domains_allowed_to_login);
}

TEST_F(GcpDevicePoliciesDayZeroFetchTest,
       FallsBackToRegistryWhenFetchCallbackReportsError) {
  SetGlobalFlagForTesting(L"domains_allowed_to_login",
                          L"registry-fallback.com");

  auto fake_updater = Make<FakeUpdaterSystem>(
      base::BindRepeating([](IUpdaterCallbackSystem* callback) -> HRESULT {
        callback->Run(-1);
        return S_OK;
      }));

  DevicePoliciesManager::SetUpdaterCreatorForTesting(base::BindRepeating(
      [](ComPtr<FakeUpdaterSystem> fake, IUpdaterSystem** updater) -> HRESULT {
        return fake.CopyTo(updater);
      },
      fake_updater));

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  EXPECT_EQ(1, fake_updater->fetch_policies_call_count());
  std::vector<std::wstring> expected = {L"registry-fallback.com"};
  EXPECT_EQ(expected, device_policies.domains_allowed_to_login);
}

TEST_F(GcpDevicePoliciesDayZeroFetchTest,
       TimesOutCleanlyAndDisconnectsLateCallback) {
  SetGlobalFlagForTesting(L"domains_allowed_to_login",
                          L"registry-fallback.com");
  DevicePoliciesManager::SetPolicyFetchTimeoutForTesting(
      base::Milliseconds(50));

  ComPtr<IUpdaterCallbackSystem> saved_callback;
  auto fake_updater = Make<FakeUpdaterSystem>(base::BindRepeating(
      [](ComPtr<IUpdaterCallbackSystem>* out_callback,
         IUpdaterCallbackSystem* callback) -> HRESULT {
        *out_callback = callback;
        // Intentionally do not invoke callback->Run() so TimedWait expires.
        return S_OK;
      },
      std::addressof(saved_callback)));

  DevicePoliciesManager::SetUpdaterCreatorForTesting(base::BindRepeating(
      [](ComPtr<FakeUpdaterSystem> fake, IUpdaterSystem** updater) -> HRESULT {
        return fake.CopyTo(updater);
      },
      fake_updater));

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  EXPECT_EQ(1, fake_updater->fetch_policies_call_count());
  std::vector<std::wstring> expected = {L"registry-fallback.com"};
  EXPECT_EQ(expected, device_policies.domains_allowed_to_login);

  // Verify that invoking the COM callback after TriggerUpdaterPolicyFetch() has
  // timed out and destroyed its stack-allocated WaitableEvent is safely ignored
  // via PolicyFetchCallback::Disconnect().
  ASSERT_NE(nullptr, saved_callback.Get());
  EXPECT_EQ(S_OK, saved_callback->Run(0));
}

}  // namespace testing
}  // namespace credential_provider
