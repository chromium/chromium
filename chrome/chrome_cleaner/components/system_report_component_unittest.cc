// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/system_report_component.h"

#include <shlobj.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/logging/cleaner_logging_service.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/proto/chrome_cleaner_report.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/parsers/json_parser/test_json_parser.h"
#include "chrome/chrome_cleaner/parsers/parser_utils/command_line_arguments_sanitizer.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/fake_shortcut_parser.h"
#include "chrome/chrome_cleaner/test/test_extensions.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {
namespace {

using google::protobuf::RepeatedPtrField;
using InstalledExtension =
    ChromeCleanerReport::SystemReport::InstalledExtension;
using base::WaitableEvent;

const int kExtensionIdLength = 32;

const wchar_t kRunKeyPath[] =
    L"software\\microsoft\\windows\\currentversion\\run";
const wchar_t kRunTestName[] = L"test_chrome_cleanup_tool";
const wchar_t kRunTestValue[] = L"chrome_cleanup_tool.exe";

const wchar_t kNameServerKeyPath[] =
    L"system\\currentcontrolset\\services\\tcpip\\parameters\\interfaces\\"
    L"fishy";
const wchar_t kNameServerName[] = L"nameserver";
const wchar_t kNameServerValue[] = L"8.8.4.4";
const wchar_t kFakeProgramFolder[] = L"FakeProgram";
const char kFakeProgram[] = "FakeProgram";
const wchar_t kFakeAppDataProgramFolder[] = L"FakeAppDataProgram";
const char kFakeAppDataProgram[] = "FakeAppDataProgram";

const char kExtensionPoliciesNonExistent[] =
    "software\\policies\\google\\chrome\\";

struct ReportTestData {
  HKEY hkey;
  const wchar_t* key_path;
  const wchar_t* name;
  const wchar_t* value;
};

const ReportTestData kExtensionPolicyEmpty{
    HKEY_LOCAL_MACHINE, kChromePoliciesAllowlistKeyPath, L"test1", L""};

constexpr wchar_t kTestingExtensionId1[] = L"ababababcdcdcdcdefefefefghghghgh";
constexpr wchar_t kTestingExtensionId1WithUpdateUrl[] =
    L"ababababcdcdcdcdefefefefghghghgh;https://clients2.google.com/service/"
    L"update2/crx";
constexpr wchar_t kTestingExtensionId2[] = L"aaaabbbbccccddddeeeeffffgggghhhh";
constexpr wchar_t kTestingExtensionId2WithUpdateUrl[] =
    L"aaaabbbbccccddddeeeeffffgggghhhh;https://clients2.google.com/service/"
    L"update2/crx";

const ReportTestData extension_policies[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesWhitelistKeyPathDeprecated, L"test1",
     kTestingExtensionId1},
    {HKEY_CURRENT_USER, kChromePoliciesWhitelistKeyPathDeprecated, L"test2",
     kTestingExtensionId1},
    {HKEY_LOCAL_MACHINE, kChromePoliciesForcelistKeyPath, L"test3",
     kTestingExtensionId2WithUpdateUrl},
    {HKEY_CURRENT_USER, kChromePoliciesForcelistKeyPath, L"test4",
     kTestingExtensionId2WithUpdateUrl},
    {HKEY_LOCAL_MACHINE, kChromiumPoliciesWhitelistKeyPathDeprecated, L"test5",
     kTestingExtensionId1},
    {HKEY_CURRENT_USER, kChromiumPoliciesWhitelistKeyPathDeprecated, L"test6",
     kTestingExtensionId1},
    {HKEY_LOCAL_MACHINE, kChromiumPoliciesForcelistKeyPath, L"test7",
     kTestingExtensionId2WithUpdateUrl},
    {HKEY_CURRENT_USER, kChromiumPoliciesForcelistKeyPath, L"test8",
     kTestingExtensionId2WithUpdateUrl},
    {HKEY_LOCAL_MACHINE, kChromePoliciesAllowlistKeyPath, L"test9",
     kTestingExtensionId1},
    {HKEY_CURRENT_USER, kChromePoliciesAllowlistKeyPath, L"test10",
     kTestingExtensionId1},
    {HKEY_LOCAL_MACHINE, kChromiumPoliciesAllowlistKeyPath, L"test11",
     kTestingExtensionId1},
    {HKEY_CURRENT_USER, kChromiumPoliciesAllowlistKeyPath, L"test12",
     kTestingExtensionId1},
};

const ReportTestData extension_forcelist_policies[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesForcelistKeyPath, L"test1",
     kTestingExtensionId1WithUpdateUrl},
    {HKEY_CURRENT_USER, kChromePoliciesForcelistKeyPath, L"test2",
     kTestingExtensionId2WithUpdateUrl},
};

const UwSId kFakePupId = 42;

const wchar_t kFakeChromeFolderForTests[] =
    L"google\\chrome\\application\\42.12.34.56";

const char kDefaultExtensionsJsonForTests[] = R"(
    {
      "ababababcdcdcdcdefefefefghghghgh" : {
        "external_update_url": "https://clients2.google.com/service/update2/crx"
      },
      "aaaabbbbccccddddeeeeffffgggghhhh" : {
        "external_update_url": "https://clients2.google.com/service/update2/crx"
      }
    })";
// Don't include the null-terminator in the length.
const int kDefaultExtensionsJsonForTestsSize =
    sizeof(kDefaultExtensionsJsonForTests) - 1;

// ExtensionSettings that has two force_installed extensions and two not.
const wchar_t kExtensionSettingsJson[] =
    LR"(
    {
      "ababababcdcdcdcdefefefefghghghgh": {
        "installation_mode": "force_installed",
        "update_url":"https://clients2.google.com/service/update2/crx"
      },
      "aaaabbbbccccddddeeeeffffgggghhhh": {
        "installation_mode": "force_installed",
        "update_url":"https://clients2.google.com/service/update2/crx"
      },
      "extensionwithinstallmodeblockeda": {
        "installation_mode": "blocked",
        "update_url":"https://clients2.google.com/service/update2/crx"
      },
      "extensionwithnosettingsabcdefghi": {}
    })";

const ReportTestData extension_settings_entry = {
    HKEY_LOCAL_MACHINE, kChromePoliciesKeyPath, L"ExtensionSettings",
    kExtensionSettingsJson};

const wchar_t kChromeExePathForTests[] = L"google\\chrome\\application";
const wchar_t kMasterPreferencesFileNameForTests[] = L"master_preferences";
const char kMasterPreferencesJsonForTests[] = R"(
    {
      "homepage": "http://dev.chromium.org/",
      "extensions": {
        "settings": {
          "ababababcdcdcdcdefefefefghghghgh": {
            "location": 1,
            "manifest": {
              "name": "Test extension"
            }
          },
          "aaaabbbbccccddddeeeeffffgggghhhh": {
            "location": 1,
            "manifest": {
              "name": "Another one"
            }
          }
        }
      }
    })";

const char kSanitizedLnkPath[] = "CSIDL_PROFILE\\appdata\\roaming";
typedef std::map<std::wstring, std::vector<std::wstring>>
    ExtensionIdToFileNamesMap;

class SystemReportComponentTest : public testing::Test {
 public:
  SystemReportComponentTest() : component_(&json_parser_, &shortcut_parser_) {}

  void SetUp() override {
    cleaner_logging_service_ = CleanerLoggingService::GetInstance();
    cleaner_logging_service_->Initialize(nullptr);
    cleaner_logging_service_->EnableUploads(true, /*registry_logger=*/nullptr);
    LoggingServiceAPI::SetInstanceForTesting(cleaner_logging_service_);
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);
    registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE);

    base::PathService::Get(CsidlToPathServiceKey(CSIDL_APPDATA),
                           &appdata_file_path_);

    ASSERT_TRUE(fake_user_data_.CreateUniqueTempDir());

    extension_id_to_filenames_map_[kTestingExtensionId1] = {L"file1.test",
                                                            L"file2.test"};
    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_.GetPath().Append(L"Profile 1"), kTestingExtensionId1,
        extension_id_to_filenames_map_[kTestingExtensionId1]));

    extension_id_to_filenames_map_[kTestingExtensionId2] = {L"file1.test"};
    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_.GetPath().Append(L"Profile 2"), kTestingExtensionId2,
        extension_id_to_filenames_map_[kTestingExtensionId2]));

    component_.SetUserDataPathForTesting(fake_user_data_.GetPath());
  }

  void TearDown() override {
    LoggingServiceAPI::SetInstanceForTesting(nullptr);
    cleaner_logging_service_->Terminate();
  }

  ChromeCleanerReport GetChromeCleanerReport() {
    ChromeCleanerReport report;
    CHECK(report.ParseFromString(cleaner_logging_service_->RawReportContent()));
    return report;
  }

  TestJsonParser json_parser_;
  FakeShortcutParser shortcut_parser_;
  SystemReportComponent component_;
  CleanerLoggingService* cleaner_logging_service_ = nullptr;
  registry_util::RegistryOverrideManager registry_override_;
  base::FilePath appdata_file_path_;
  base::ScopedTempDir fake_user_data_;
  ExtensionIdToFileNamesMap extension_id_to_filenames_map_;
};

template <typename Component>
bool SomeComponentContainsPath(const RepeatedPtrField<Component>& components,
                               const base::FilePath& path) {
  const std::string name = path.BaseName().MaybeAsASCII();
  const std::string sanitized_path = base::WideToUTF8(SanitizePath(path));
  for (const auto component : components) {
    if (component.name() == name &&
        component.file_information().path() == sanitized_path) {
      return true;
    }
  }
  return false;
}

bool InstalledProgramCollected(
    const RepeatedPtrField<ChromeCleanerReport::SystemReport::InstalledProgram>&
        programs,
    const std::string& folder_name) {
  const std::string lower_case_folder_name = base::ToLowerASCII(folder_name);
  for (const auto& program : programs) {
    if (program.folder_information().path().find(lower_case_folder_name) !=
        std::string::npos) {
      return true;
    }
  }
  return false;
}

bool RegistryEntryCollected(
    const RepeatedPtrField<RegistryValue>& registry_values,
    const std::string& key_path,
    const std::string& value_name,
    const std::string& data) {
  for (const auto& registry_value : registry_values) {
    if (StringContainsCaseInsensitive(registry_value.key_path(), key_path) &&
        registry_value.value_name() == value_name &&
        registry_value.data() == data) {
      return true;
    }
  }
  return false;
}

bool RegistryKeyCollected(
    const RepeatedPtrField<RegistryValue>& registry_values,
    const std::string& key_path) {
  for (const auto& registry_value : registry_values) {
    if (StringContainsCaseInsensitive(registry_value.key_path(), key_path))
      return true;
  }
  return false;
}

// Returns whether |extension_files| contains the expected file information
// of |extension_id|.
::testing::AssertionResult ExtensionsReportedWithExpectedFiles(
    const RepeatedPtrField<InstalledExtension>& reported_extensions,
    const ExtensionIdToFileNamesMap& extension_id_to_filenames_map) {
  for (const auto& installed_extension : reported_extensions) {
    const RepeatedPtrField<FileInformation> extension_files =
        installed_extension.extension_files();

    std::unordered_set<std::wstring> expected_files(
        extension_id_to_filenames_map
            .at(base::UTF8ToWide(installed_extension.extension_id()))
            .begin(),
        extension_id_to_filenames_map
            .at(base::UTF8ToWide(installed_extension.extension_id()))
            .end());

    if (static_cast<size_t>(extension_files.size()) != expected_files.size())
      return ::testing::AssertionFailure()
             << "Different number of files than expected for extension "
             << installed_extension.extension_id();

    for (const auto& file : extension_files) {
      std::wstring file_name =
          base::FilePath(base::UTF8ToWide(file.path())).BaseName().value();

      if (expected_files.find(file_name) == expected_files.end())
        return ::testing::AssertionFailure()
               << "file : " << file_name << " not reported for extension "
               << installed_extension.extension_id();
    }
  }
  return ::testing::AssertionSuccess();
}

// Returns whether |reported_extensions| contains an entry for an extension with
// |extension_id| and |install_method|.
bool InstalledExtensionReported(
    const RepeatedPtrField<InstalledExtension>& reported_extensions,
    const std::wstring& extension_id,
    ExtensionInstallMethod install_method) {
  for (const auto& installed_extension : reported_extensions) {
    if (installed_extension.extension_id() == base::WideToUTF8(extension_id) &&
        installed_extension.install_method() == install_method) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_F(SystemReportComponentTest, PreScan_NoDetailedReport) {
  component_.PreScan();
  EXPECT_FALSE(component_.created_report());
}

TEST_F(SystemReportComponentTest, PostScanNoUwS_DetailedReport) {
  std::vector<UwSId> found_pups;
  component_.PostScan(found_pups);
  EXPECT_TRUE(component_.created_report());
}

TEST_F(SystemReportComponentTest, PostScanNonRemovalableUwS_DetailedReport) {
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(kFakePupId, PUPData::FLAGS_NONE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  std::vector<UwSId> found_pups;
  found_pups.push_back(kFakePupId);
  component_.PostScan(found_pups);
  EXPECT_TRUE(component_.created_report());
}

TEST_F(SystemReportComponentTest, PostScanRemovalableUwS_NoDetailedReport) {
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(kFakePupId, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  std::vector<UwSId> found_pups;
  found_pups.push_back(kFakePupId);
  component_.PostScan(found_pups);
  EXPECT_FALSE(component_.created_report());
}

TEST_F(SystemReportComponentTest, PreCleanup_NoDetailedReport) {
  component_.PreCleanup();
  EXPECT_FALSE(component_.created_report());
}

TEST_F(SystemReportComponentTest, PostCleanup_DetailedReport) {
  component_.PostCleanup(RESULT_CODE_SUCCESS, nullptr);
  EXPECT_TRUE(component_.created_report());
}

TEST_F(SystemReportComponentTest, PostCleanup_canceled_NoDetailedReport) {
  component_.PostCleanup(RESULT_CODE_CANCELED, nullptr);
  EXPECT_FALSE(component_.created_report());
}

TEST_F(SystemReportComponentTest, PostValidation_NoDetailedReport) {
  component_.PostValidation(RESULT_CODE_SUCCESS);
  EXPECT_FALSE(component_.created_report());
}

TEST_F(SystemReportComponentTest, OnClose_NoDetailedReport) {
  component_.OnClose(RESULT_CODE_SUCCESS);
  EXPECT_FALSE(component_.created_report());
}

TEST_F(SystemReportComponentTest, DisableUploads) {
  cleaner_logging_service_->EnableUploads(false, nullptr);

  component_.CreateFullSystemReport();
  EXPECT_FALSE(component_.created_report());
}

TEST_F(SystemReportComponentTest, NoRepeatedCollections) {
  component_.CreateFullSystemReport();
  EXPECT_TRUE(component_.created_report());

  // Clear the report and ensure it isn't populated again, since
  // |created_report| is still true.
  EXPECT_LT(0, GetChromeCleanerReport().system_report().processes_size());
  cleaner_logging_service_->Terminate();
  cleaner_logging_service_->Initialize(/*registry_logger=*/nullptr);
  EXPECT_EQ(0, GetChromeCleanerReport().system_report().processes_size());
  component_.CreateFullSystemReport();
  EXPECT_EQ(0, GetChromeCleanerReport().system_report().processes_size());
}

TEST_F(SystemReportComponentTest, DetectFakePrograms) {
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::ScopedPathOverride appdata_override(base::DIR_LOCAL_APP_DATA);
  base::FilePath appdata_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &appdata_dir));

  // Setup a fake program installation folder.
  base::FilePath fake_program_dir(program_files_dir.Append(kFakeProgramFolder));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_program_dir, nullptr));
  ASSERT_TRUE(CreateFileInFolder(fake_program_dir, L"fake.exe"));

  base::FilePath fake_appdata_dir(
      appdata_dir.Append(kFakeAppDataProgramFolder));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_appdata_dir, nullptr));
  ASSERT_TRUE(CreateFileInFolder(fake_appdata_dir, L"fake.exe"));

  // Setup a fake runonce keys.
  base::win::RegKey run_once(HKEY_CURRENT_USER, kRunKeyPath, KEY_WRITE);
  ASSERT_TRUE(run_once.Valid());
  ASSERT_EQ(run_once.WriteValue(kRunTestName, kRunTestValue), ERROR_SUCCESS);

  component_.CreateFullSystemReport();

  ChromeCleanerReport report = GetChromeCleanerReport();
  const auto& system_report = report.system_report();

  const base::FilePath module_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();
  EXPECT_TRUE(
      SomeComponentContainsPath(system_report.loaded_modules(), module_path));

  EXPECT_TRUE(
      SomeComponentContainsPath(system_report.processes(), module_path));

  EXPECT_TRUE(RegistryEntryCollected(
      system_report.registry_values(), base::WideToASCII(kRunKeyPath),
      base::WideToASCII(kRunTestName), base::WideToASCII(kRunTestValue)));

  EXPECT_TRUE(InstalledProgramCollected(system_report.installed_programs(),
                                        kFakeProgram));
  EXPECT_TRUE(InstalledProgramCollected(system_report.installed_programs(),
                                        kFakeAppDataProgram));
}

TEST_F(SystemReportComponentTest, ReportNameServer) {
  // Setup a fake key that triggers nameserver check.
  base::win::RegKey nameserver_key;
  ASSERT_EQ(ERROR_SUCCESS,
            nameserver_key.Create(HKEY_LOCAL_MACHINE, kNameServerKeyPath,
                                  KEY_ALL_ACCESS));
  ASSERT_TRUE(nameserver_key.Valid());
  ASSERT_EQ(nameserver_key.WriteValue(kNameServerName, kNameServerValue),
            ERROR_SUCCESS);

  component_.CreateFullSystemReport();

  EXPECT_TRUE(RegistryEntryCollected(
      GetChromeCleanerReport().system_report().registry_values(),
      base::WideToASCII(kNameServerKeyPath), base::WideToASCII(kNameServerName),
      base::WideToASCII(kNameServerValue)));
}

TEST_F(SystemReportComponentTest, ReportNameServerBadValue) {
  // Setup a fake key that triggers nameserver check.
  base::win::RegKey nameserver_key;
  ASSERT_EQ(ERROR_SUCCESS,
            nameserver_key.Create(HKEY_LOCAL_MACHINE, kNameServerKeyPath,
                                  KEY_ALL_ACCESS));
  ASSERT_TRUE(nameserver_key.Valid());
  ASSERT_EQ(nameserver_key.WriteValue(kNameServerName, L""), ERROR_SUCCESS);

  component_.CreateFullSystemReport();

  EXPECT_FALSE(RegistryKeyCollected(
      GetChromeCleanerReport().system_report().registry_values(),
      base::WideToASCII(kNameServerKeyPath)));
}

TEST_F(SystemReportComponentTest, ReportNameServerNonExistent) {
  component_.CreateFullSystemReport();

  EXPECT_FALSE(RegistryKeyCollected(
      GetChromeCleanerReport().system_report().registry_values(),
      base::WideToASCII(kNameServerKeyPath)));
}

TEST_F(SystemReportComponentTest, ReportExtensionPolicies) {
  for (const auto& extension_policy : extension_policies) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.Create(extension_policy.hkey,
                                extension_policy.key_path, KEY_ALL_ACCESS));
    ASSERT_TRUE(policy_key.Valid());
    ASSERT_EQ(
        policy_key.WriteValue(extension_policy.name, extension_policy.value),
        ERROR_SUCCESS);
  }

  component_.CreateFullSystemReport();

  ChromeCleanerReport report = GetChromeCleanerReport();
  const auto& system_report = report.system_report();
  for (const auto& extension_policy : extension_policies) {
    EXPECT_TRUE(
        RegistryEntryCollected(system_report.registry_values(),
                               base::WideToASCII(extension_policy.key_path),
                               base::WideToASCII(extension_policy.name),
                               base::WideToASCII(extension_policy.value)));
  }
}

TEST_F(SystemReportComponentTest, ReportExtensionPoliciesBadValue) {
  base::win::RegKey policy_key;
  ASSERT_EQ(ERROR_SUCCESS,
            policy_key.Create(kExtensionPolicyEmpty.hkey,
                              kExtensionPolicyEmpty.key_path, KEY_ALL_ACCESS));
  ASSERT_TRUE(policy_key.Valid());
  ASSERT_EQ(policy_key.WriteValue(kExtensionPolicyEmpty.name,
                                  kExtensionPolicyEmpty.value),
            ERROR_SUCCESS);
  component_.CreateFullSystemReport();
  EXPECT_FALSE(RegistryKeyCollected(
      GetChromeCleanerReport().system_report().registry_values(),
      base::WideToASCII(kExtensionPolicyEmpty.key_path)));
}

TEST_F(SystemReportComponentTest, ReportExtensionPoliciesNonExistent) {
  component_.CreateFullSystemReport();

  EXPECT_FALSE(RegistryKeyCollected(
      GetChromeCleanerReport().system_report().registry_values(),
      kExtensionPoliciesNonExistent));
}

TEST_F(SystemReportComponentTest, ReportForcelistExtensions) {
  for (const ReportTestData& policy : extension_forcelist_policies) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.Create(policy.hkey, policy.key_path, KEY_ALL_ACCESS));
    ASSERT_TRUE(policy_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS, policy_key.WriteValue(policy.name, policy.value));
  }

  base::test::ScopedCommandLine scoped_command_line;
  component_.CreateFullSystemReport();
  ChromeCleanerReport report = GetChromeCleanerReport();

  for (const ReportTestData& expected : extension_forcelist_policies) {
    std::wstring policy_value(expected.value);
    EXPECT_TRUE(InstalledExtensionReported(
        report.system_report().installed_extensions(),
        policy_value.substr(0, kExtensionIdLength),
        ExtensionInstallMethod::POLICY_EXTENSION_FORCELIST));
  }
}

TEST_F(SystemReportComponentTest, ReportDefaultExtensions) {
  // Set up a fake default extensions JSON file.
  base::ScopedPathOverride program_files_x86_override(
      base::DIR_PROGRAM_FILESX86);
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath fake_apps_dir(
      program_files_dir.Append(kFakeChromeFolderForTests)
          .Append(L"default_apps"));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));

  base::FilePath default_extensions_file =
      fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kDefaultExtensionsJsonForTests,
                        kDefaultExtensionsJsonForTestsSize);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  base::test::ScopedCommandLine scoped_command_line;
  component_.CreateFullSystemReport();
  ChromeCleanerReport report = GetChromeCleanerReport();

  EXPECT_EQ(2, report.system_report().installed_extensions().size());
  EXPECT_TRUE(InstalledExtensionReported(
      report.system_report().installed_extensions(), kTestingExtensionId1,
      ExtensionInstallMethod::DEFAULT_APPS_EXTENSION));
  EXPECT_TRUE(InstalledExtensionReported(
      report.system_report().installed_extensions(), kTestingExtensionId2,
      ExtensionInstallMethod::DEFAULT_APPS_EXTENSION));

  EXPECT_TRUE(ExtensionsReportedWithExpectedFiles(
      report.system_report().installed_extensions(),
      extension_id_to_filenames_map_));
}

TEST_F(SystemReportComponentTest, ReportExtensionSettings) {
  // Set up a fake ExtensionSettings registry key.
  base::win::RegKey settings_key;
  ASSERT_EQ(
      ERROR_SUCCESS,
      settings_key.Create(extension_settings_entry.hkey,
                          extension_settings_entry.key_path, KEY_ALL_ACCESS));
  DCHECK(settings_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.WriteValue(extension_settings_entry.name,
                                    extension_settings_entry.value));

  base::test::ScopedCommandLine scoped_command_line;
  component_.CreateFullSystemReport();
  ChromeCleanerReport report = GetChromeCleanerReport();

  EXPECT_TRUE(InstalledExtensionReported(
      report.system_report().installed_extensions(), kTestingExtensionId1,
      ExtensionInstallMethod::POLICY_EXTENSION_SETTINGS));
  EXPECT_TRUE(InstalledExtensionReported(
      report.system_report().installed_extensions(), kTestingExtensionId2,
      ExtensionInstallMethod::POLICY_EXTENSION_SETTINGS));

  EXPECT_TRUE(ExtensionsReportedWithExpectedFiles(
      report.system_report().installed_extensions(),
      extension_id_to_filenames_map_));
}

TEST_F(SystemReportComponentTest, ReportMasterPreferencesExtensions) {
  // Set up a fake master preferences file
  base::ScopedPathOverride program_files_x86_override(
      base::DIR_PROGRAM_FILESX86);
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePathForTests));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));

  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileNameForTests);
  CreateFileWithContent(master_preferences, kMasterPreferencesJsonForTests,
                        sizeof(kMasterPreferencesJsonForTests) - 1);
  ASSERT_TRUE(base::PathExists(master_preferences));

  base::test::ScopedCommandLine scoped_command_line;
  component_.CreateFullSystemReport();
  ChromeCleanerReport report = GetChromeCleanerReport();

  EXPECT_EQ(2, report.system_report().installed_extensions().size());
  EXPECT_TRUE(InstalledExtensionReported(
      report.system_report().installed_extensions(), kTestingExtensionId1,
      ExtensionInstallMethod::POLICY_MASTER_PREFERENCES));
  EXPECT_TRUE(InstalledExtensionReported(
      report.system_report().installed_extensions(), kTestingExtensionId2,
      ExtensionInstallMethod::POLICY_MASTER_PREFERENCES));

  EXPECT_TRUE(ExtensionsReportedWithExpectedFiles(
      report.system_report().installed_extensions(),
      extension_id_to_filenames_map_));
}

TEST_F(SystemReportComponentTest,
       ReportModifiedShortcutWithCommandLineArguments) {
  const std::wstring kShortcutArguments =
      L"--some-flag --some-other-scary-flag --flag-with-personal-data=" +
      appdata_file_path_.value();
  const int kArgumentSize = 3;
  ShortcutInformation shortcut_with_command_line_arguments;
  shortcut_with_command_line_arguments.lnk_path = appdata_file_path_;
  shortcut_with_command_line_arguments.target_path =
      (appdata_file_path_.Append(L"chrome.exe")).value();
  shortcut_with_command_line_arguments.command_line_arguments =
      kShortcutArguments;

  std::vector<ShortcutInformation> fake_parsed_shortcuts;
  fake_parsed_shortcuts.push_back(shortcut_with_command_line_arguments);

  shortcut_parser_.SetShortcutsToReturn(fake_parsed_shortcuts);

  component_.CreateFullSystemReport();
  ChromeCleanerReport report = GetChromeCleanerReport();
  ASSERT_EQ(report.system_report().shortcut_data().size(), 1);
  ChromeCleanerReport_SystemReport_ShortcutData shortcut_data =
      report.system_report().shortcut_data(0);

  EXPECT_EQ(shortcut_data.lnk_path(), kSanitizedLnkPath);

  const std::string kSanitizedFakeChromePath = "CSIDL_APPDATA\\chrome.exe";
  EXPECT_EQ(shortcut_data.executable_path(), kSanitizedFakeChromePath);

  ASSERT_EQ(shortcut_data.command_line_arguments().size(), kArgumentSize);
  EXPECT_EQ(
      shortcut_data.command_line_arguments(0),
      base::JoinString({"--flag-with-personal-data=", kSanitizedLnkPath}, ""));
  EXPECT_EQ(shortcut_data.command_line_arguments(1), "--some-flag");
  EXPECT_EQ(shortcut_data.command_line_arguments(2), "--some-other-scary-flag");
}

TEST_F(SystemReportComponentTest, ReportShortcutWithPersonalSite) {
  const std::wstring kPersonalSite = L"http://www.somesite.com/user/happy_user";
  const std::string kSanitizedPersonalSite = "http://www.somesite.com";
  ShortcutInformation shortcut_with_personal_site;
  shortcut_with_personal_site.lnk_path = appdata_file_path_;
  shortcut_with_personal_site.target_path =
      (appdata_file_path_.Append(L"chrome.exe")).value();
  shortcut_with_personal_site.command_line_arguments = kPersonalSite;

  std::vector<ShortcutInformation> fake_parsed_shortcuts;
  fake_parsed_shortcuts.push_back(shortcut_with_personal_site);
  shortcut_parser_.SetShortcutsToReturn(fake_parsed_shortcuts);

  component_.CreateFullSystemReport();
  ChromeCleanerReport report = GetChromeCleanerReport();
  ASSERT_EQ(report.system_report().shortcut_data().size(), 1);
  ChromeCleanerReport_SystemReport_ShortcutData shortcut_data =
      report.system_report().shortcut_data(0);

  ASSERT_EQ(shortcut_data.command_line_arguments().size(), 1);
  EXPECT_EQ(shortcut_data.lnk_path(), kSanitizedLnkPath);

  const std::string kSanitizedFakeChromePath = "CSIDL_APPDATA\\chrome.exe";
  EXPECT_EQ(shortcut_data.executable_path(), kSanitizedFakeChromePath);

  ASSERT_EQ(shortcut_data.command_line_arguments().size(), 1);
  EXPECT_EQ(shortcut_data.command_line_arguments(0), kSanitizedPersonalSite);
}

TEST_F(SystemReportComponentTest, ReportShortcutWithDifferentTarget) {
  ShortcutInformation shortcut_with_different_target;
  shortcut_with_different_target.lnk_path = appdata_file_path_;
  shortcut_with_different_target.target_path =
      (appdata_file_path_.Append(L"totallynotuws.exe")).value();
  shortcut_with_different_target.command_line_arguments = L"";

  std::vector<ShortcutInformation> fake_parsed_shortcuts;
  fake_parsed_shortcuts.push_back(shortcut_with_different_target);
  shortcut_parser_.SetShortcutsToReturn(fake_parsed_shortcuts);

  component_.CreateFullSystemReport();

  ChromeCleanerReport report = GetChromeCleanerReport();
  ASSERT_EQ(report.system_report().shortcut_data().size(), 1);
  ChromeCleanerReport_SystemReport_ShortcutData shortcut_data =
      report.system_report().shortcut_data(0);

  EXPECT_EQ(shortcut_data.lnk_path(), kSanitizedLnkPath);

  const std::string kSanitizedFakeUwSFile = "CSIDL_APPDATA\\totallynotuws.exe";
  EXPECT_EQ(shortcut_data.executable_path(), kSanitizedFakeUwSFile);

  EXPECT_EQ(shortcut_data.command_line_arguments().size(), 0);
}

TEST_F(SystemReportComponentTest, DoNotReportShortcutWithoutModifications) {
  ShortcutInformation not_modified_shortcut;
  not_modified_shortcut.lnk_path = appdata_file_path_;
  not_modified_shortcut.target_path =
      (appdata_file_path_.Append(L"chrome.exe")).value();
  not_modified_shortcut.command_line_arguments = L"";

  std::vector<ShortcutInformation> fake_parsed_shortcuts;
  fake_parsed_shortcuts.push_back(not_modified_shortcut);
  shortcut_parser_.SetShortcutsToReturn(fake_parsed_shortcuts);

  component_.CreateFullSystemReport();
  ChromeCleanerReport report = GetChromeCleanerReport();
  EXPECT_EQ(report.system_report().shortcut_data().size(), 0);
}
}  // namespace chrome_cleaner
