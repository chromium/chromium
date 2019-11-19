// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <lmerr.h>
#include <objbase.h>
#include <unknwn.h>
#include <wrl/client.h>

#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/setup/setup_lib.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpSetupTest : public ::testing::Test {
 protected:
  ~GcpSetupTest() override;

  const base::FilePath& module_path() const { return module_path_; }
  const base::string16& product_version() const { return product_version_; }

  void ExpectAllFilesToExist(bool exist, const base::string16& product_version);
  void ExpectCredentialProviderToBeRegistered(
      bool registered,
      const base::string16& product_version);

  base::FilePath installed_path_for_version(
      const base::string16& product_version) {
    return scoped_temp_prog_dir_.GetPath()
        .Append(GetInstallParentDirectoryName())
        .Append(FILE_PATH_LITERAL("Credential Provider"))
        .Append(product_version);
  }

  base::FilePath installed_path() {
    return installed_path_for_version(product_version_);
  }

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

  FakeScopedLsaPolicyFactory* fake_scoped_lsa_policy_factory() {
    return &fake_scoped_lsa_policy_factory_;
  }

  FakesForTesting* fakes_for_testing() { return &fakes_; }

 private:
  void SetUp() override;

  void GetModulePathAndProductVersion(base::FilePath* module_path,
                                      base::string16* product_version);

  registry_util::RegistryOverrideManager registry_override_;
  base::ScopedTempDir scoped_temp_prog_dir_;
  base::ScopedTempDir scoped_temp_start_menu_dir_;
  std::unique_ptr<base::ScopedPathOverride> program_files_override_;
  std::unique_ptr<base::ScopedPathOverride> start_menu_override_;
  std::unique_ptr<base::ScopedPathOverride> dll_path_override_;
  base::FilePath module_path_;
  base::string16 product_version_;
  FakeOSUserManager fake_os_user_manager_;
  FakeOSProcessManager fake_os_process_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
  FakesForTesting fakes_;
};

GcpSetupTest::~GcpSetupTest() {
  logging::ResetEventSourceForTesting();
}

void GcpSetupTest::GetModulePathAndProductVersion(
    base::FilePath* module_path,
    base::string16* product_version) {
  // Pass null module handle to get path for the executable file of the
  // current process.
  wchar_t module[MAX_PATH];
  ASSERT_TRUE(::GetModuleFileName(nullptr, module, MAX_PATH));

  // These tests assume all the binaries exist in the same directory.
  *module_path = base::FilePath(module);
  ASSERT_FALSE(module_path->empty());

  *product_version =
      FileVersionInfo::CreateFileVersionInfo(*module_path)->product_version();
  ASSERT_FALSE(product_version->empty());
}

void GcpSetupTest::ExpectAllFilesToExist(
    bool exist,
    const base::string16& product_version) {
  base::FilePath root = installed_path_for_version(product_version);
  EXPECT_EQ(exist, base::PathExists(root));

  const base::FilePath::CharType* const* filenames;
  size_t number_of_files;
  GetInstalledFileBasenames(&filenames, &number_of_files);

  for (size_t i = 0; i < number_of_files; ++i)
    EXPECT_EQ(exist, base::PathExists(root.Append(filenames[i])));
}

void GcpSetupTest::ExpectCredentialProviderToBeRegistered(
    bool registered,
    const base::string16& product_version) {
  auto guid_string = base::win::String16FromGUID(CLSID_GaiaCredentialProvider);

  // Make sure COM object is registered.
  base::string16 register_key_path =
      base::StringPrintf(L"CLSID\\%ls\\InprocServer32", guid_string.c_str());
  base::win::RegKey clsid_key(HKEY_CLASSES_ROOT, register_key_path.c_str(),
                              KEY_READ);
  EXPECT_EQ(registered, clsid_key.Valid());

  if (registered) {
    base::FilePath path = installed_path_for_version(product_version)
                              .Append(FILE_PATH_LITERAL("Gaia1_0.dll"));
    base::string16 value;
    EXPECT_EQ(ERROR_SUCCESS, clsid_key.ReadValue(L"", &value));
    EXPECT_EQ(path.value(), value);
  }

  base::string16 cp_key_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
      L"Authentication\\Credential Providers\\%ls",
      guid_string.c_str());

  // Make sure credential provider is registered.
  base::win::RegKey cp_key(HKEY_LOCAL_MACHINE, cp_key_path.c_str(), KEY_READ);
  EXPECT_EQ(registered, cp_key.Valid());

  // Make sure eventlog source is registered.
  base::win::RegKey el_key(
      HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\GCPW",
      KEY_READ);
  EXPECT_EQ(registered, el_key.Valid());

  if (registered) {
    base::FilePath path =
        installed_path_for_version(product_version)
            .Append(FILE_PATH_LITERAL("gcp_eventlog_provider.dll"));
    base::string16 value;
    EXPECT_EQ(ERROR_SUCCESS, el_key.ReadValue(L"EventMessageFile", &value));
    EXPECT_EQ(path.value(), value);
  }
}

void GcpSetupTest::SetUp() {
  ASSERT_TRUE(SUCCEEDED(
      CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE)));

  // Get the path to the setup exe (this exe during unit tests) and the
  // chrome version.
  GetModulePathAndProductVersion(&module_path_, &product_version_);

  // Override registry so that tests don't mess up the machine's state.
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_CLASSES_ROOT));
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(HKEY_USERS));

  // Override the environment used by setup so that tests don't mess
  // up the machine's state.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  ASSERT_NE(nullptr, env.get());

  ASSERT_TRUE(scoped_temp_prog_dir_.CreateUniqueTempDir());
  program_files_override_.reset(new base::ScopedPathOverride(
      base::DIR_PROGRAM_FILES, scoped_temp_prog_dir_.GetPath()));

  ASSERT_TRUE(scoped_temp_start_menu_dir_.CreateUniqueTempDir());
  start_menu_override_.reset(new base::ScopedPathOverride(
      base::DIR_COMMON_START_MENU, scoped_temp_start_menu_dir_.GetPath()));

  // In non-component builds, base::FILE_MODULE will always return the path
  // to base.dll because of the way CURRENT_MODULE works.  Therefore overriding
  // to point to gaia1_0.dll's destination path (i.e. after it is installed).
  // The actual file name is not important, just the directory.
  dll_path_override_.reset(new base::ScopedPathOverride(
      base::FILE_MODULE, installed_path().Append(FILE_PATH_LITERAL("foo.dll")),
      true /*=is_absolute*/, false /*=create*/));

  base::FilePath startup_path =
      scoped_temp_start_menu_dir_.GetPath().Append(L"StartUp");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(startup_path, nullptr));

  // Fake factories for testing the DLL registration.
  fakes_.os_user_manager_for_testing = &fake_os_user_manager_;
  fakes_.os_process_manager_for_testing = &fake_os_process_manager_;
  fakes_.scoped_lsa_policy_creator =
      fake_scoped_lsa_policy_factory_.GetCreatorCallback();
}

TEST_F(GcpSetupTest, DoInstall) {
  logging::ResetEventSourceForTesting();

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));
  ExpectAllFilesToExist(true, product_version());
  ExpectCredentialProviderToBeRegistered(true, product_version());

  EXPECT_FALSE(
      fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName).sid.empty());
  EXPECT_FALSE(fake_scoped_lsa_policy_factory()
                   ->private_data()[kLsaKeyGaiaPassword]
                   .empty());
  EXPECT_EQ(
      kDefaultGaiaAccountName,
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername]);
}

TEST_F(GcpSetupTest, DoInstallOverOldInstall) {
  logging::ResetEventSourceForTesting();

  // Install using some old version.
  const base::string16 old_version(L"1.0.0.0");
  ASSERT_EQ(S_OK, DoInstall(module_path(), old_version, fakes_for_testing()));
  ExpectAllFilesToExist(true, old_version);

  FakeOSUserManager::UserInfo old_user_info =
      fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName);
  base::string16 old_password =
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaPassword];
  EXPECT_FALSE(old_password.empty());

  base::string16 old_username =
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername];
  EXPECT_EQ(old_username, kDefaultGaiaAccountName);

  logging::ResetEventSourceForTesting();

  // Now install a newer version.
  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  // Make sure newer version exists and old version is gone.
  ExpectAllFilesToExist(true, product_version());
  ExpectAllFilesToExist(false, old_version);

  // Make sure kGaiaAccountName info and private data are unchanged.
  EXPECT_EQ(old_user_info,
            fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName));
  EXPECT_EQ(
      old_password,
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaPassword]);
  EXPECT_EQ(
      old_username,
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername]);
}

TEST_F(GcpSetupTest, DoInstallOverOldLockedInstall) {
  logging::ResetEventSourceForTesting();

  // Install using some old version.
  const base::string16 old_version(L"1.0.0.0");
  ASSERT_EQ(S_OK, DoInstall(module_path(), old_version, fakes_for_testing()));
  ExpectAllFilesToExist(true, old_version);

  // Lock the CP DLL.
  base::FilePath dll_path = installed_path_for_version(old_version)
                                .Append(FILE_PATH_LITERAL("Gaia1_0.dll"));
  base::File lock(dll_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_EXCLUSIVE_READ |
                                base::File::FLAG_EXCLUSIVE_WRITE);

  logging::ResetEventSourceForTesting();

  // Now install a newer version.
  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  // Make sure newer version exists.
  ExpectAllFilesToExist(true, product_version());

  // The locked file will still exist, the others are gone.
  const base::FilePath::CharType* const* filenames;
  size_t count;
  GetInstalledFileBasenames(&filenames, &count);
  for (size_t i = 0; i < count; ++i) {
    const base::FilePath path =
        installed_path_for_version(old_version).Append(filenames[i]);
    EXPECT_EQ(path == dll_path, base::PathExists(path));
  }
}

TEST_F(GcpSetupTest, LaunchGcpAfterInstall) {
  logging::ResetEventSourceForTesting();

  // Install using some old version.
  const base::string16 old_version(L"1.0.0.0");
  ASSERT_EQ(S_OK, DoInstall(module_path(), old_version, fakes_for_testing()));
  ExpectAllFilesToExist(true, old_version);

  // Lock the CP DLL.
  base::FilePath dll_path = installed_path_for_version(old_version)
                                .Append(FILE_PATH_LITERAL("Gaia1_0.dll"));
  base::File locked_file(dll_path, base::File::FLAG_OPEN |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_EXCLUSIVE_READ |
                                       base::File::FLAG_EXCLUSIVE_WRITE);

  logging::ResetEventSourceForTesting();

  // Now install a newer version.
  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  // Make sure newer version exists.
  ExpectAllFilesToExist(true, product_version());

  // The locked file will still exist, the others are gone.
  const base::FilePath::CharType* const* filenames;
  size_t count;
  GetInstalledFileBasenames(&filenames, &count);
  for (size_t i = 0; i < count; ++i) {
    const base::FilePath path =
        installed_path_for_version(old_version).Append(filenames[i]);
    EXPECT_EQ(path == dll_path, base::PathExists(path));
  }

  locked_file.Close();

  Microsoft::WRL::ComPtr<IGaiaCredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_PPV_ARGS(&provider)));

  // Make sure newer version exists and old version is gone.
  ExpectAllFilesToExist(true, product_version());
  ExpectAllFilesToExist(false, old_version);
}

TEST_F(GcpSetupTest, DoUninstall) {
  logging::ResetEventSourceForTesting();

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  logging::ResetEventSourceForTesting();

  ASSERT_EQ(S_OK,
            DoUninstall(module_path(), installed_path(), fakes_for_testing()));
  ExpectAllFilesToExist(false, product_version());
  ExpectCredentialProviderToBeRegistered(false, product_version());
  EXPECT_TRUE(
      fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName).sid.empty());
  EXPECT_TRUE(fake_scoped_lsa_policy_factory()
                  ->private_data()[kLsaKeyGaiaPassword]
                  .empty());
  EXPECT_TRUE(fake_scoped_lsa_policy_factory()
                  ->private_data()[kLsaKeyGaiaUsername]
                  .empty());
}

TEST_F(GcpSetupTest, ValidLsaWithNoExistingUser) {
  // Create the default user so that name is not taken when the user is created.
  CComBSTR sid;
  DWORD error;
  EXPECT_EQ(S_OK, fake_os_user_manager()->AddUser(
                      kDefaultGaiaAccountName, L"password", L"fullname",
                      L"comment", true, &sid, &error));
  // Even if the LSA information is correct, if no actual user exists, a new
  // user needs to be created.
  fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername] =
      L"gaia1";
  fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaPassword] =
      L"password";
  logging::ResetEventSourceForTesting();

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  logging::ResetEventSourceForTesting();

  EXPECT_FALSE(fake_scoped_lsa_policy_factory()
                   ->private_data()[kLsaKeyGaiaPassword]
                   .empty());
  base::string16 expected_gaia_username =
      L"gaia" + base::NumberToString16(kInitialDuplicateUsernameIndex);
  EXPECT_FALSE(fake_os_user_manager()
                   ->GetUserInfo(expected_gaia_username.c_str())
                   .sid.empty());
  EXPECT_EQ(
      expected_gaia_username,
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername]);
}

TEST_F(GcpSetupTest, EnableStats) {
  // Make sure usagestats does not exist.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE,
                       credential_provider::kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  DWORD value;
  EXPECT_NE(ERROR_SUCCESS,
            key.ReadValueDW(credential_provider::kRegUsageStatsName, &value));

  // Enable stats.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.AppendSwitch(credential_provider::switches::kEnableStats);
  EXPECT_EQ(0, EnableStatsCollection(cmdline));

  // Stats should be enabled.
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValueDW(credential_provider::kRegUsageStatsName, &value));
  EXPECT_EQ(1u, value);
}

TEST_F(GcpSetupTest, DisableStats) {
  // Make sure usagestats does not exist.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE,
                       credential_provider::kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  DWORD value;
  EXPECT_NE(ERROR_SUCCESS,
            key.ReadValueDW(credential_provider::kRegUsageStatsName, &value));

  // Disable stats.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.AppendSwitch(credential_provider::switches::kDisableStats);
  EXPECT_EQ(0, EnableStatsCollection(cmdline));

  // Stats should be disabled.
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValueDW(credential_provider::kRegUsageStatsName, &value));
  EXPECT_EQ(0u, value);
}

TEST_F(GcpSetupTest, EnableDisableStats) {
  // Make sure usagestats does not exist.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE,
                       credential_provider::kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  DWORD value;
  EXPECT_NE(ERROR_SUCCESS,
            key.ReadValueDW(credential_provider::kRegUsageStatsName, &value));

  // Enable and disable stats.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.AppendSwitch(credential_provider::switches::kEnableStats);
  cmdline.AppendSwitch(credential_provider::switches::kDisableStats);
  EXPECT_EQ(0, EnableStatsCollection(cmdline));

  // Stats should be disabled.
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValueDW(credential_provider::kRegUsageStatsName, &value));
  EXPECT_EQ(0u, value);
}

// This test checks the expect success / failure of DLL registration when
// a gaia user already exists
// Parameters:
// int: Number of gaia users to create before trying to register the DLL.
// bool: Whether the final user creation is expected to succeed. For
// kMaxAttempts = 10, 0 to 8 gaia users can be created and still have the
// test succeed. If a 9th user is create the test will fail.
class GcpGaiaUserCreationTest
    : public GcpSetupTest,
      public testing::WithParamInterface<std::tuple<int, bool>> {};

TEST_P(GcpGaiaUserCreationTest, ExistingGaiaUserTest) {
  CComBSTR sid;
  DWORD error;
  EXPECT_EQ(S_OK, fake_os_user_manager()->AddUser(
                      kDefaultGaiaAccountName, L"password", L"fullname",
                      L"comment", true, &sid, &error));

  int last_user_index = std::get<0>(GetParam());
  for (int i = 0; i < last_user_index; ++i) {
    base::string16 existing_gaia_username = kDefaultGaiaAccountName;
    existing_gaia_username +=
        base::NumberToString16(i + kInitialDuplicateUsernameIndex);
    EXPECT_EQ(S_OK, fake_os_user_manager()->AddUser(
                        existing_gaia_username.c_str(), L"password",
                        L"fullname", L"comment", true, &sid, &error));
  }
  logging::ResetEventSourceForTesting();

  bool should_succeed = std::get<1>(GetParam());

  ASSERT_EQ(should_succeed ? S_OK : E_UNEXPECTED,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  logging::ResetEventSourceForTesting();

  if (should_succeed) {
    EXPECT_FALSE(fake_scoped_lsa_policy_factory()
                     ->private_data()[kLsaKeyGaiaPassword]
                     .empty());
    base::string16 expected_gaia_username = kDefaultGaiaAccountName;
    expected_gaia_username += base::NumberToString16(
        last_user_index + kInitialDuplicateUsernameIndex);
    EXPECT_FALSE(fake_os_user_manager()
                     ->GetUserInfo(expected_gaia_username.c_str())
                     .sid.empty());
    EXPECT_EQ(
        expected_gaia_username,
        fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername]);
  } else {
    EXPECT_TRUE(fake_scoped_lsa_policy_factory()
                    ->private_data()[kLsaKeyGaiaPassword]
                    .empty());
    EXPECT_TRUE(fake_scoped_lsa_policy_factory()
                    ->private_data()[kLsaKeyGaiaUsername]
                    .empty());
  }
}

// For a max retry of 10, it is possible to create gaia users 'gaia',
// 'gaia0' ... 'gaia8' before failing. At 'gaia9' the test should fail.

INSTANTIATE_TEST_SUITE_P(
    AvailableGaiaUserName,
    GcpGaiaUserCreationTest,
    ::testing::Combine(::testing::Range(0, kMaxUsernameAttempts - 2),
                       ::testing::Values(true)));

INSTANTIATE_TEST_SUITE_P(
    UnavailableGaiaUserName,
    GcpGaiaUserCreationTest,
    ::testing::Values(std::make_tuple<int, bool>(kMaxUsernameAttempts - 1,
                                                 false)));

}  // namespace credential_provider
