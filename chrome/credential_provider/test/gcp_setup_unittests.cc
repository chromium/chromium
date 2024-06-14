// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unknwn.h>

#include <datetimeapi.h>
#include <lmerr.h>
#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/extension/extension_strings.h"
#include "chrome/credential_provider/extension/extension_utils.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/setup/gcpw_files.h"
#include "chrome/credential_provider/setup/setup_lib.h"
#include "chrome/credential_provider/setup/setup_utils.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpSetupTest : public ::testing::Test {
 protected:
  ~GcpSetupTest() override;

  const base::FilePath& module_path() const { return module_path_; }
  const std::wstring& product_version() const { return product_version_; }

  void CreateSentinelFileToSimulateCrash(const std::wstring& product_version);

  void ExpectAllFilesToExist(bool exist, const std::wstring& product_version);
  void ExpectSentinelFileToNotExist(const std::wstring& product_version);
  void ExpectCredentialProviderToBeRegistered(
      bool registered,
      const std::wstring& product_version);
  void ExpectRequiredRegistryEntriesToBePresent();

  base::FilePath installed_path_for_version(
      const std::wstring& product_version) {
    return scoped_temp_prog_dir_.GetPath()
        .Append(GetInstallParentDirectoryName())
        .Append(FILE_PATH_LITERAL("Credential Provider"))
        .Append(product_version);
  }

  base::FilePath sentinel_path_for_version(
      const std::wstring& product_version) {
    return scoped_temp_progdata_dir_.GetPath()
        .Append(GetInstallParentDirectoryName())
        .Append(FILE_PATH_LITERAL("Credential Provider"))
        .Append(product_version)
        .Append(FILE_PATH_LITERAL("gcpw_startup.sentinel"));
  }

  base::FilePath installed_path() {
    return installed_path_for_version(product_version_);
  }

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

  FakeScopedLsaPolicyFactory* fake_scoped_lsa_policy_factory() {
    return &fake_scoped_lsa_policy_factory_;
  }

  FakesForTesting* fakes_for_testing() { return &fakes_; }

  std::wstring uninstall_reg_key() {
    std::wstring uninstall_reg = kRegUninstall;
    uninstall_reg.append(L"\\");
    uninstall_reg.append(kRegUninstallProduct);
    return uninstall_reg;
  }

  base::FilePath CreateFilePath(const std::string& file_name) {
    return temp_dir_.GetPath().AppendASCII(file_name.c_str());
  }

  void CreateJsonFile(const base::FilePath& path, const std::string& data) {
    ASSERT_TRUE(base::WriteFile(path, data));
  }

  void assert_addremove_reg_exists() {
    base::win::RegKey uninstall_key;
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.Open(HKEY_LOCAL_MACHINE,
                                 uninstall_reg_key().c_str(), KEY_ALL_ACCESS));
    std::wstring uninstall_args;
    std::wstring display_name;
    std::wstring install_location;
    std::wstring display_icon;
    std::wstring install_date;
    DWORD no_modify;
    DWORD no_repair;
    std::wstring publisher_name;
    std::wstring version_str;
    std::wstring display_version;
    DWORD version_major;
    DWORD version_minor;
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegUninstallString, &uninstall_args));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegUninstallDisplayName, &display_name));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegInstallLocation, &install_location));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegDisplayIcon, &display_icon));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegInstallDate, &install_date));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValueDW(kRegNoModify, &no_modify));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValueDW(kRegNoRepair, &no_repair));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegPublisherName, &publisher_name));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegVersion, &version_str));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValue(kRegDisplayVersion, &display_version));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValueDW(kRegVersionMajor, &version_major));
    ASSERT_EQ(ERROR_SUCCESS,
              uninstall_key.ReadValueDW(kRegVersionMinor, &version_minor));

    base::CommandLine uninstall_cmdline(
        installed_path().Append(kCredentialProviderSetupExe));
    uninstall_cmdline.AppendSwitch(switches::kUninstall);
    ASSERT_EQ(uninstall_args, uninstall_cmdline.GetCommandLineString().c_str());
    ASSERT_EQ(display_name, GetStringResource(IDS_PROJNAME_BASE));
    ASSERT_EQ(install_location, installed_path().value());
    ASSERT_EQ(publisher_name, kRegPublisher);

    base::FilePath setup_exe =
        installed_path().Append(kCredentialProviderSetupExe);
    ASSERT_EQ(display_icon, (setup_exe.value() + L",0").c_str());
    ASSERT_EQ(install_date, GetCurrentDateForTesting());
    ASSERT_EQ(no_modify, (DWORD)1);
    ASSERT_EQ(no_repair, (DWORD)1);

    base::Version version(CHROME_VERSION_STRING);
    ASSERT_EQ(version_str, base::ASCIIToWide(version.GetString()));
    ASSERT_EQ(display_version, base::ASCIIToWide(version.GetString()));

    const std::vector<uint32_t>& version_components = version.components();
    ASSERT_EQ(version_major, static_cast<DWORD>(version_components[2]));
    ASSERT_EQ(version_minor, static_cast<DWORD>(version_components[3]));
  }

  void SetUp() override;

 private:
  std::wstring GetCurrentDateForTesting() {
    static const wchar_t kDateFormat[] = L"yyyyMMdd";
    wchar_t date_str[std::size(kDateFormat)] = {0};
    int len = GetDateFormatW(LOCALE_INVARIANT, 0, nullptr, kDateFormat,
                             date_str, std::size(date_str));
    if (len) {
      --len;  // Subtract terminating \0.
    } else {
      return L"";
    }

    return std::wstring(date_str, len);
  }

  void GetModulePathAndProductVersion(base::FilePath* module_path,
                                      std::wstring* product_version);

  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};
  registry_util::RegistryOverrideManager registry_override_;
  base::ScopedTempDir scoped_temp_prog_dir_;
  base::ScopedTempDir scoped_temp_start_menu_dir_;
  base::ScopedTempDir scoped_temp_progdata_dir_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> program_files_override_;
  std::unique_ptr<base::ScopedPathOverride> start_menu_override_;
  std::unique_ptr<base::ScopedPathOverride> programdata_override_;
  std::unique_ptr<base::ScopedPathOverride> dll_path_override_;
  base::FilePath module_path_;
  std::wstring product_version_;
  FakeGCPWFiles fake_gcpw_files_;
  FakeOSUserManager fake_os_user_manager_;
  FakeOSProcessManager fake_os_process_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
  FakeOSServiceManager fake_os_service_manager_;
  FakesForTesting fakes_;
};

GcpSetupTest::~GcpSetupTest() {
  logging::ResetEventSourceForTesting();
}

void GcpSetupTest::GetModulePathAndProductVersion(
    base::FilePath* module_path,
    std::wstring* product_version) {
  // Pass null module handle to get path for the executable file of the
  // current process.
  wchar_t module[MAX_PATH];
  ASSERT_TRUE(::GetModuleFileName(nullptr, module, MAX_PATH));

  // These tests assume all the binaries exist in the same directory.
  *module_path = base::FilePath(module);
  ASSERT_FALSE(module_path->empty());

  *product_version = base::AsWString(
      FileVersionInfo::CreateFileVersionInfo(*module_path)->product_version());
  ASSERT_FALSE(product_version->empty());
}

void GcpSetupTest::CreateSentinelFileToSimulateCrash(
    const std::wstring& product_version) {
  base::FilePath sentinel_file = sentinel_path_for_version(product_version);

  // Create the destination folder
  ASSERT_TRUE(base::CreateDirectory(sentinel_file.DirName()));
  base::win::ScopedHandle file(
      CreateFile(sentinel_file.value().c_str(), GENERIC_WRITE, 0, nullptr,
                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  ASSERT_TRUE(file.IsValid());
}

void GcpSetupTest::ExpectSentinelFileToNotExist(
    const std::wstring& product_version) {
  base::FilePath sentinel_file = sentinel_path_for_version(product_version);
  EXPECT_EQ(false, base::PathExists(sentinel_file));
}

void GcpSetupTest::ExpectAllFilesToExist(bool exist,
                                         const std::wstring& product_version) {
  base::FilePath root = installed_path_for_version(product_version);
  EXPECT_EQ(exist, base::PathExists(root));

  bool extension_found = false;
  auto install_files = GCPWFiles::Get()->GetEffectiveInstallFiles();

  for (auto& install_file : install_files) {
    if (kCredentialProviderExtensionExe.find(install_file) !=
        base::FilePath::StringType::npos)
      extension_found = true;
    EXPECT_EQ(exist, base::PathExists(root.Append(install_file)));
  }

  EXPECT_EQ(extension::IsGCPWExtensionEnabled(), extension_found);
}

void GcpSetupTest::ExpectCredentialProviderToBeRegistered(
    bool registered,
    const std::wstring& product_version) {
  auto guid_string = base::win::WStringFromGUID(CLSID_GaiaCredentialProvider);

  // Make sure COM object is registered.
  std::wstring register_key_path =
      base::StrCat({L"CLSID\\", guid_string, L"\\InprocServer32"});
  base::win::RegKey clsid_key(HKEY_CLASSES_ROOT, register_key_path.c_str(),
                              KEY_READ);
  EXPECT_EQ(registered, clsid_key.Valid());

  if (registered) {
    base::FilePath path = installed_path_for_version(product_version)
                              .Append(FILE_PATH_LITERAL("Gaia1_0.dll"));
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS, clsid_key.ReadValue(L"", &value));
    EXPECT_EQ(path.value(), value);
  }

  std::wstring cp_key_path =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Authentication\\"
      L"Credential Providers\\" +
      guid_string;

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
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS, el_key.ReadValue(L"EventMessageFile", &value));
    EXPECT_EQ(path.value(), value);
  }
}

void GcpSetupTest::ExpectRequiredRegistryEntriesToBePresent() {
  base::win::RegKey key(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_READ);
  EXPECT_TRUE(key.Valid());
}

void GcpSetupTest::SetUp() {
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
  program_files_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_PROGRAM_FILES, scoped_temp_prog_dir_.GetPath());

  ASSERT_TRUE(scoped_temp_start_menu_dir_.CreateUniqueTempDir());
  start_menu_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_COMMON_START_MENU, scoped_temp_start_menu_dir_.GetPath());

  ASSERT_TRUE(scoped_temp_progdata_dir_.CreateUniqueTempDir());
  programdata_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_COMMON_APP_DATA, scoped_temp_progdata_dir_.GetPath());

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  // In non-component builds, base::FILE_MODULE will always return the path
  // to base.dll because of the way CURRENT_MODULE works.  Therefore overriding
  // to point to gaia1_0.dll's destination path (i.e. after it is installed).
  // The actual file name is not important, just the directory.
  dll_path_override_ = std::make_unique<base::ScopedPathOverride>(
      base::FILE_MODULE, installed_path().Append(FILE_PATH_LITERAL("foo.dll")),
      true /*=is_absolute*/, false /*=create*/);

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
  ExpectRequiredRegistryEntriesToBePresent();

  EXPECT_FALSE(
      fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName).sid.empty());
  EXPECT_FALSE(fake_scoped_lsa_policy_factory()
                   ->private_data()[kLsaKeyGaiaPassword]
                   .empty());
  EXPECT_EQ(
      kDefaultGaiaAccountName,
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername]);
}

TEST_F(GcpSetupTest, DoInstallWithExtension) {
  logging::ResetEventSourceForTesting();

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_LOCAL_MACHINE, kGcpRootKeyName,
                                      KEY_SET_VALUE | KEY_WOW64_32KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(extension::kEnableGCPWExtension, 1));

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));
  ExpectAllFilesToExist(true, product_version());
  ExpectCredentialProviderToBeRegistered(true, product_version());
  ExpectRequiredRegistryEntriesToBePresent();

  EXPECT_FALSE(
      fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName).sid.empty());
  EXPECT_FALSE(fake_scoped_lsa_policy_factory()
                   ->private_data()[kLsaKeyGaiaPassword]
                   .empty());
  EXPECT_EQ(
      kDefaultGaiaAccountName,
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername]);
}

// Tests install over old install for different types of installations.
// 0 - Indicates that initial installation is standalone.
// 1 - Indicates that the initial installation is through MSI.
class GcpInstallOverOldInstallTest : public GcpSetupTest,
                                     public ::testing::WithParamInterface<int> {
 public:
  void SetInstallerConfig(bool add_installer_data) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

    // Indicates fresh installation.
    command_line.AppendSwitch(switches::kStandaloneInstall);

    if (add_installer_data) {
      // Only set if installation source is MSI.

      std::string installer_json = "{\"distribution\": {\"msi\": true}}";
      base::FilePath installer_data_path = CreateFilePath("myfile.txt");
      CreateJsonFile(installer_data_path, installer_json);

      command_line.AppendSwitchPath(switches::kInstallerData,
                                    installer_data_path);
    }

    StandaloneInstallerConfigurator::Get()->ConfigureInstallationType(
        command_line);
  }
};

// TODO: crbug.com/347201817 - Fix ODR violation.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_DoInstallOverOldInstall DISABLED_DoInstallOverOldInstall
#else
#define MAYBE_DoInstallOverOldInstall DoInstallOverOldInstall
#endif
TEST_P(GcpInstallOverOldInstallTest, MAYBE_DoInstallOverOldInstall) {
  logging::ResetEventSourceForTesting();

  // Set installer data argument to indicate the installation source.
  SetInstallerConfig(GetParam());

  // Install using some old version.
  const std::wstring old_version(L"1.0.0.0");
  ASSERT_EQ(S_OK, DoInstall(module_path(), old_version, fakes_for_testing()));
  ExpectAllFilesToExist(true, old_version);
  CreateSentinelFileToSimulateCrash(old_version);

  FakeOSUserManager::UserInfo old_user_info =
      fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName);
  std::wstring old_password =
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaPassword];
  EXPECT_FALSE(old_password.empty());

  std::wstring old_username =
      fake_scoped_lsa_policy_factory()->private_data()[kLsaKeyGaiaUsername];
  EXPECT_EQ(old_username, kDefaultGaiaAccountName);

  logging::ResetEventSourceForTesting();

  // Don't include any flag to indicate the installation source as
  // the registry was set the first time this is called.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  StandaloneInstallerConfigurator::Get()->ConfigureInstallationType(
      command_line);

  EXPECT_TRUE(
      StandaloneInstallerConfigurator::Get()->IsStandaloneInstallation() ==
      !GetParam());

  // Now install a newer version.
  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  // Make sure newer version exists and old version is gone.
  ExpectAllFilesToExist(true, product_version());
  ExpectAllFilesToExist(false, old_version);
  ExpectSentinelFileToNotExist(old_version);

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

INSTANTIATE_TEST_SUITE_P(All,
                         GcpInstallOverOldInstallTest,
                         ::testing::Values(0, 1));

// TODO: crbug.com/347201817 - Fix ODR violation.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_DoInstallOverOldLockedInstall \
  DISABLED_DoInstallOverOldLockedInstall
#else
#define MAYBE_DoInstallOverOldLockedInstall DoInstallOverOldLockedInstall
#endif
TEST_F(GcpSetupTest, MAYBE_DoInstallOverOldLockedInstall) {
  logging::ResetEventSourceForTesting();

  // Install using some old version.
  const std::wstring old_version(L"1.0.0.0");
  ASSERT_EQ(S_OK, DoInstall(module_path(), old_version, fakes_for_testing()));
  ExpectAllFilesToExist(true, old_version);

  // Lock the CP DLL.
  base::FilePath dll_path = installed_path_for_version(old_version)
                                .Append(FILE_PATH_LITERAL("Gaia1_0.dll"));
  base::File lock(dll_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WIN_EXCLUSIVE_READ |
                                base::File::FLAG_WIN_EXCLUSIVE_WRITE);

  logging::ResetEventSourceForTesting();

  // Now install a newer version.
  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  // Make sure newer version exists.
  ExpectAllFilesToExist(true, product_version());

  // The locked file will still exist, the others are gone.
  auto install_files = GCPWFiles::Get()->GetEffectiveInstallFiles();
  for (auto& install_file : install_files) {
    const base::FilePath path =
        installed_path_for_version(old_version).Append(install_file);
    EXPECT_EQ(path == dll_path, base::PathExists(path));
  }
}

// TODO: crbug.com/347201817 - Fix ODR violation.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_LaunchGcpAfterInstall DISABLED_LaunchGcpAfterInstall
#else
#define MAYBE_LaunchGcpAfterInstall LaunchGcpAfterInstall
#endif
TEST_F(GcpSetupTest, MAYBE_LaunchGcpAfterInstall) {
  logging::ResetEventSourceForTesting();

  // Install using some old version.
  const std::wstring old_version(L"1.0.0.0");
  ASSERT_EQ(S_OK, DoInstall(module_path(), old_version, fakes_for_testing()));
  ExpectAllFilesToExist(true, old_version);

  // Lock the CP DLL.
  base::FilePath dll_path = installed_path_for_version(old_version)
                                .Append(FILE_PATH_LITERAL("Gaia1_0.dll"));
  base::File locked_file(dll_path, base::File::FLAG_OPEN |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_WIN_EXCLUSIVE_READ |
                                       base::File::FLAG_WIN_EXCLUSIVE_WRITE);

  logging::ResetEventSourceForTesting();

  // Now install a newer version.
  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  // Make sure newer version exists.
  ExpectAllFilesToExist(true, product_version());

  // The locked file will still exist, the others are gone.
  auto install_files = GCPWFiles::Get()->GetEffectiveInstallFiles();

  for (auto& install_file : install_files) {
    const base::FilePath path =
        installed_path_for_version(old_version).Append(install_file);
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

// Tests installations from exe and MSI.
// 0 - installation is not standalone.
// 1 - installation is standalone.
class GcpInstallerTest : public GcpSetupTest,
                         public ::testing::WithParamInterface<int> {};

// TODO: crbug.com/347201817 - Fix ODR violation.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_DoUninstall DISABLED_DoUninstall
#else
#define MAYBE_DoUninstall DoUninstall
#endif
TEST_P(GcpInstallerTest, MAYBE_DoUninstall) {
  int standalone_installer = GetParam();

  logging::ResetEventSourceForTesting();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kStandaloneInstall);

  if (!standalone_installer) {
    std::string installer_json = "{\"distribution\": {\"msi\": true}}";
    base::FilePath installer_data_path = CreateFilePath("myfile.txt");
    CreateJsonFile(installer_data_path, installer_json);

    command_line.AppendSwitchPath(switches::kInstallerData,
                                  installer_data_path);
  }

  StandaloneInstallerConfigurator::Get()->ConfigureInstallationType(
      command_line);

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  if (standalone_installer)
    assert_addremove_reg_exists();

  CreateSentinelFileToSimulateCrash(product_version());
  logging::ResetEventSourceForTesting();

  ASSERT_EQ(S_OK,
            DoUninstall(module_path(), installed_path(), fakes_for_testing()));
  ExpectAllFilesToExist(false, product_version());
  ExpectSentinelFileToNotExist(product_version());
  ExpectCredentialProviderToBeRegistered(false, product_version());
  EXPECT_TRUE(
      fake_os_user_manager()->GetUserInfo(kDefaultGaiaAccountName).sid.empty());
  EXPECT_TRUE(fake_scoped_lsa_policy_factory()
                  ->private_data()[kLsaKeyGaiaPassword]
                  .empty());
  EXPECT_TRUE(fake_scoped_lsa_policy_factory()
                  ->private_data()[kLsaKeyGaiaUsername]
                  .empty());

  base::win::RegKey uninstall_key;
  EXPECT_NE(ERROR_SUCCESS,
            uninstall_key.Open(HKEY_LOCAL_MACHINE, uninstall_reg_key().c_str(),
                               KEY_ALL_ACCESS));
}

INSTANTIATE_TEST_SUITE_P(All, GcpInstallerTest, ::testing::Values(0, 1));

// TODO: crbug.com/347201817 - Fix ODR violation.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_DoUninstallWithExtension DISABLED_DoUninstallWithExtension
#else
#define MAYBE_DoUninstallWithExtension DoUninstallWithExtension
#endif
TEST_F(GcpSetupTest, MAYBE_DoUninstallWithExtension) {
  logging::ResetEventSourceForTesting();

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_LOCAL_MACHINE, kGcpRootKeyName,
                                      KEY_SET_VALUE | KEY_WOW64_32KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(extension::kEnableGCPWExtension, 1));

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));
  CreateSentinelFileToSimulateCrash(product_version());
  logging::ResetEventSourceForTesting();

  ASSERT_EQ(S_OK,
            DoUninstall(module_path(), installed_path(), fakes_for_testing()));
  ExpectAllFilesToExist(false, product_version());
  ExpectSentinelFileToNotExist(product_version());
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
  logging::ResetEventSourceForTesting();

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

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));

  logging::ResetEventSourceForTesting();

  EXPECT_FALSE(fake_scoped_lsa_policy_factory()
                   ->private_data()[kLsaKeyGaiaPassword]
                   .empty());
  std::wstring expected_gaia_username =
      L"gaia" + base::NumberToWString(kInitialDuplicateUsernameIndex);
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
            key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  DWORD value;
  EXPECT_NE(ERROR_SUCCESS, key.ReadValueDW(kRegUsageStatsName, &value));

  // Enable stats.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.AppendSwitch(switches::kEnableStats);
  EXPECT_EQ(0, EnableStatsCollection(cmdline));

  // Stats should be enabled.
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(kRegUsageStatsName, &value));
  EXPECT_EQ(1u, value);
}

TEST_F(GcpSetupTest, DisableStats) {
  // Make sure usagestats does not exist.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  DWORD value;
  EXPECT_NE(ERROR_SUCCESS, key.ReadValueDW(kRegUsageStatsName, &value));

  // Disable stats.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.AppendSwitch(switches::kDisableStats);
  EXPECT_EQ(0, EnableStatsCollection(cmdline));

  // Stats should be disabled.
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(kRegUsageStatsName, &value));
  EXPECT_EQ(0u, value);
}

TEST_F(GcpSetupTest, EnableDisableStats) {
  // Make sure usagestats does not exist.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  DWORD value;
  EXPECT_NE(ERROR_SUCCESS, key.ReadValueDW(kRegUsageStatsName, &value));

  // Enable and disable stats.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.AppendSwitch(switches::kEnableStats);
  cmdline.AppendSwitch(switches::kDisableStats);
  EXPECT_EQ(0, EnableStatsCollection(cmdline));

  // Stats should be disabled.
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(kRegUsageStatsName, &value));
  EXPECT_EQ(0u, value);
}

TEST_F(GcpSetupTest, WriteUninstallStringsForMSI) {
  base::win::RegKey key;

  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));

  // Write uninstall strings.
  base::FilePath file_path =
      installed_path().Append(FILE_PATH_LITERAL("foo.exe"));
  ASSERT_EQ(S_OK, WriteUninstallRegistryValues(file_path));

  // Verify uninstall strings.
  std::wstring uninstall_string;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(kRegUninstallStringField, &uninstall_string));
  EXPECT_EQ(uninstall_string, file_path.value());

  std::wstring uninstall_arguments;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(kRegUninstallArgumentsField, &uninstall_arguments));

  base::CommandLine expected_uninstall_arguments(base::CommandLine::NO_PROGRAM);
  expected_uninstall_arguments.AppendSwitch(switches::kUninstall);

  EXPECT_EQ(uninstall_arguments,
            expected_uninstall_arguments.GetCommandLineString());
}

TEST_F(GcpSetupTest, WriteCredentialProviderRegistryValues) {
  // Verify keys don't exist.
  base::win::RegKey key;
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_ALL_ACCESS));

  base::win::RegKey uninstall_key;
  ASSERT_NE(ERROR_SUCCESS,
            uninstall_key.Open(HKEY_LOCAL_MACHINE, uninstall_reg_key().c_str(),
                               KEY_ALL_ACCESS));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // Set the standalone registry, but not installer data so that EXE
  // installation is indicated.
  command_line.AppendSwitch(switches::kStandaloneInstall);

  StandaloneInstallerConfigurator::Get()->ConfigureInstallationType(
      command_line);

  // Write GCPW registry keys.
  ASSERT_EQ(S_OK, WriteCredentialProviderRegistryValues(installed_path()));

  // Verify keys were created.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_ALL_ACCESS));

  assert_addremove_reg_exists();
}

TEST_F(GcpSetupTest, DoInstallWritesUninstallStrings) {
  logging::ResetEventSourceForTesting();

  ASSERT_EQ(S_OK,
            DoInstall(module_path(), product_version(), fakes_for_testing()));
  ExpectAllFilesToExist(true, product_version());

  base::win::RegKey key;

  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                       KEY_ALL_ACCESS | KEY_WOW64_32KEY));

  // Verify uninstall strings.
  std::wstring uninstall_string;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(kRegUninstallStringField, &uninstall_string));
  EXPECT_EQ(uninstall_string,
            installed_path().Append(kCredentialProviderSetupExe).value());

  std::wstring uninstall_arguments;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(kRegUninstallArgumentsField, &uninstall_arguments));

  base::CommandLine expected_uninstall_arguments(base::CommandLine::NO_PROGRAM);
  expected_uninstall_arguments.AppendSwitch(switches::kUninstall);
  EXPECT_EQ(uninstall_arguments,
            expected_uninstall_arguments.GetCommandLineString());
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
    std::wstring existing_gaia_username = kDefaultGaiaAccountName;
    existing_gaia_username +=
        base::NumberToWString(i + kInitialDuplicateUsernameIndex);
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
    std::wstring expected_gaia_username = kDefaultGaiaAccountName;
    expected_gaia_username +=
        base::NumberToWString(last_user_index + kInitialDuplicateUsernameIndex);
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
