// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_util_unittest.h"

#include <windows.h>

#include <shlobj.h>

#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that we are parsing Chrome version correctly.
TEST(SetupUtilTest, GetMaxVersionFromArchiveDirTest) {
  // Create a version dir
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath chrome_dir = test_dir.GetPath().AppendASCII("1.0.0.0");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  std::unique_ptr<base::Version> version(
      installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()));
  ASSERT_EQ(version->GetString(), "1.0.0.0");

  base::DeletePathRecursively(chrome_dir);
  ASSERT_FALSE(base::PathExists(chrome_dir)) << chrome_dir.value();
  ASSERT_EQ(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()),
            nullptr);

  chrome_dir = test_dir.GetPath().AppendASCII("ABC");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  ASSERT_EQ(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()),
            nullptr);

  chrome_dir = test_dir.GetPath().AppendASCII("2.3.4.5");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()));
  ASSERT_EQ(version->GetString(), "2.3.4.5");

  // Create multiple version dirs, ensure that we select the greatest.
  chrome_dir = test_dir.GetPath().AppendASCII("9.9.9.9");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  chrome_dir = test_dir.GetPath().AppendASCII("1.1.1.1");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));

  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()));
  ASSERT_EQ(version->GetString(), "9.9.9.9");
}

TEST(SetupUtilTest, DeleteFileFromTempProcess) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file;
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  base::WriteFile(test_file, "foo");
  EXPECT_TRUE(installer::DeleteFileFromTempProcess(test_file, 0));
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 3);
  EXPECT_FALSE(base::PathExists(test_file)) << test_file.value();
}

TEST(SetupUtilTest, RegisterEventLogProvider) {
  registry_util::RegistryOverrideManager registry_override_manager;
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE));

  const base::Version version("1.2.3.4");
  const base::FilePath install_directory(
      FILE_PATH_LITERAL("c:\\some_path\\test"));
  installer::RegisterEventLogProvider(install_directory, version);

  std::wstring reg_path(
      L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
  reg_path.append(install_static::InstallDetails::Get().install_full_name());
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ));
  EXPECT_TRUE(key.HasValue(L"CategoryCount"));
  EXPECT_TRUE(key.HasValue(L"TypesSupported"));
  EXPECT_TRUE(key.HasValue(L"CategoryMessageFile"));
  EXPECT_TRUE(key.HasValue(L"EventMessageFile"));
  EXPECT_TRUE(key.HasValue(L"ParameterMessageFile"));
  std::wstring value;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"CategoryMessageFile", &value));
  const base::FilePath expected_directory(
      install_directory.AppendASCII(version.GetString()));
  const base::FilePath provider_path(value);
  EXPECT_EQ(expected_directory, provider_path.DirName());
  key.Close();

  installer::DeRegisterEventLogProvider();

  EXPECT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ));
}

const char kAdjustThreadPriority[] = "adjust-thread-priority";

PriorityClassChangeResult DoThreadPriorityAdjustment() {
  return installer::AdjustThreadPriority() ? PCCR_CHANGED : PCCR_UNCHANGED;
}

namespace {

// A scoper that sets/resets the current process's priority class.
class ScopedPriorityClass {
 public:
  // Applies |priority_class|, returning an instance if a change was made.
  // Otherwise, returns an empty scoped_ptr.
  static std::unique_ptr<ScopedPriorityClass> Create(DWORD priority_class);

  ScopedPriorityClass(const ScopedPriorityClass&) = delete;
  ScopedPriorityClass& operator=(const ScopedPriorityClass&) = delete;

  ~ScopedPriorityClass();

 private:
  explicit ScopedPriorityClass(DWORD original_priority_class);
  DWORD original_priority_class_;
};

std::unique_ptr<ScopedPriorityClass> ScopedPriorityClass::Create(
    DWORD priority_class) {
  HANDLE this_process = ::GetCurrentProcess();
  DWORD original_priority_class = ::GetPriorityClass(this_process);
  EXPECT_NE(0U, original_priority_class);
  if (original_priority_class && original_priority_class != priority_class) {
    BOOL result = ::SetPriorityClass(this_process, priority_class);
    EXPECT_NE(FALSE, result);
    if (result) {
      return std::unique_ptr<ScopedPriorityClass>(
          new ScopedPriorityClass(original_priority_class));
    }
  }
  return nullptr;
}

ScopedPriorityClass::ScopedPriorityClass(DWORD original_priority_class)
    : original_priority_class_(original_priority_class) {}

ScopedPriorityClass::~ScopedPriorityClass() {
  BOOL result =
      ::SetPriorityClass(::GetCurrentProcess(), original_priority_class_);
  EXPECT_NE(FALSE, result);
}

PriorityClassChangeResult RelaunchAndDoThreadPriorityAdjustment() {
  base::CommandLine cmd_line(*base::CommandLine::ForCurrentProcess());
  cmd_line.AppendSwitch(kAdjustThreadPriority);
  base::Process process = base::LaunchProcess(cmd_line, base::LaunchOptions());
  int exit_code = 0;
  if (!process.IsValid()) {
    ADD_FAILURE() << " to launch subprocess.";
  } else if (!process.WaitForExit(&exit_code)) {
    ADD_FAILURE() << " to wait for subprocess to exit.";
  } else {
    return static_cast<PriorityClassChangeResult>(exit_code);
  }
  return PCCR_UNKNOWN;
}

}  // namespace

// Launching a subprocess at normal priority class is a noop.
TEST(SetupUtilTest, AdjustFromNormalPriority) {
  const DWORD priority_class = ::GetPriorityClass(::GetCurrentProcess());
  if (priority_class != NORMAL_PRIORITY_CLASS) {
    LOG(WARNING) << "Skipping SetupUtilTest.AdjustFromNormalPriority since "
                    "the test harness is running at priority 0x"
                 << std::hex << priority_class;
    return;
  }
  EXPECT_EQ(PCCR_UNCHANGED, RelaunchAndDoThreadPriorityAdjustment());
}

// Launching a subprocess below normal priority class drops it to bg mode for
// sufficiently recent operating systems.
TEST(SetupUtilTest, AdjustFromBelowNormalPriority) {
  std::unique_ptr<ScopedPriorityClass> below_normal;
  if (::GetPriorityClass(::GetCurrentProcess()) !=
      BELOW_NORMAL_PRIORITY_CLASS) {
    below_normal = ScopedPriorityClass::Create(BELOW_NORMAL_PRIORITY_CLASS);
    ASSERT_TRUE(below_normal);
  }
  EXPECT_EQ(PCCR_CHANGED, RelaunchAndDoThreadPriorityAdjustment());
}

TEST(SetupUtilTest, GetInstallAge) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  installer::InstallerState installer_state;
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  // Wait a beat to let time advance.
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_GE(0, installer::GetInstallAge(installer_state));

  // Crank back the directory's creation time.
  constexpr int kAgeDays = 28;
  base::Time now = base::Time::Now();
  base::win::ScopedHandle dir(::CreateFile(
      temp_dir.GetPath().value().c_str(),
      FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  ASSERT_TRUE(dir.IsValid());

  FILE_BASIC_INFO info = {};
  ASSERT_NE(0, ::GetFileInformationByHandleEx(dir.Get(), FileBasicInfo, &info,
                                              sizeof(info)));
  FILETIME creation_time = (now - base::Days(kAgeDays)).ToFileTime();
  info.CreationTime.u.LowPart = creation_time.dwLowDateTime;
  info.CreationTime.u.HighPart = creation_time.dwHighDateTime;
  ASSERT_NE(0, ::SetFileInformationByHandle(dir.Get(), FileBasicInfo, &info,
                                            sizeof(info)));

  EXPECT_EQ(kAgeDays, installer::GetInstallAge(installer_state));
}

TEST(SetupUtilTest, RecordUnPackMetricsTest) {
  base::HistogramTester histogram_tester;
  std::string unpack_status_metrics_name =
      std::string(installer::kUnPackStatusMetricsName) + "_SetupExePatch";
  histogram_tester.ExpectTotalCount(unpack_status_metrics_name, 0);

  RecordUnPackMetrics(UnPackStatus::UNPACK_NO_ERROR,
                      installer::UnPackConsumer::SETUP_EXE_PATCH);
  histogram_tester.ExpectTotalCount(unpack_status_metrics_name, 1);
  histogram_tester.ExpectBucketCount(unpack_status_metrics_name, 0, 1);

  RecordUnPackMetrics(UnPackStatus::UNPACK_EXTRACT_ERROR,
                      installer::UnPackConsumer::SETUP_EXE_PATCH);
  histogram_tester.ExpectTotalCount(unpack_status_metrics_name, 2);
  histogram_tester.ExpectBucketCount(unpack_status_metrics_name, 4, 1);
}

TEST(SetupUtilTest, AddDowngradeVersion) {
  install_static::ScopedInstallDetails system_install(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE));
  const HKEY kRoot = HKEY_LOCAL_MACHINE;
  base::win::RegKey(kRoot, install_static::GetClientStateKeyPath().c_str(),
                    KEY_SET_VALUE | KEY_WOW64_32KEY);
  std::unique_ptr<WorkItemList> list;

  base::Version current_version("1.1.1.1");
  base::Version higer_new_version("1.1.1.2");
  base::Version lower_new_version_1("1.1.1.0");
  base::Version lower_new_version_2("1.1.0.0");

  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());

  // Upgrade should not create the value.
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, current_version,
                                           higer_new_version, list.get());
  ASSERT_TRUE(list->Do());
  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());

  // Downgrade should create the value.
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, current_version,
                                           lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Multiple downgrades should not change the value.
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, lower_new_version_1,
                                           lower_new_version_2, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());
}

TEST(SetupUtilTest, DeleteDowngradeVersion) {
  install_static::ScopedInstallDetails system_install(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE));
  const HKEY kRoot = HKEY_LOCAL_MACHINE;
  base::win::RegKey(kRoot, install_static::GetClientStateKeyPath().c_str(),
                    KEY_SET_VALUE | KEY_WOW64_32KEY);
  std::unique_ptr<WorkItemList> list;

  base::Version current_version("1.1.1.1");
  base::Version higer_new_version("1.1.1.2");
  base::Version lower_new_version_1("1.1.1.0");
  base::Version lower_new_version_2("1.1.0.0");

  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, current_version,
                                           lower_new_version_2, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Upgrade should not delete the value if it still lower than the version that
  // downgrade from.
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, lower_new_version_2,
                                           lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Repair should not delete the value.
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, lower_new_version_1,
                                           lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());

  // Fully upgrade should delete the value.
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, lower_new_version_1,
                                           higer_new_version, list.get());
  ASSERT_TRUE(list->Do());
  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());

  // Fresh install should delete the value if it exists.
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, current_version,
                                           lower_new_version_2, list.get());
  ASSERT_TRUE(list->Do());
  EXPECT_EQ(current_version, InstallUtil::GetDowngradeVersion());
  list.reset(WorkItem::CreateWorkItemList());
  installer::AddUpdateDowngradeVersionItem(kRoot, base::Version(),
                                           lower_new_version_1, list.get());
  ASSERT_TRUE(list->Do());
  ASSERT_FALSE(InstallUtil::GetDowngradeVersion());
}

namespace {

// A test fixture that configures an InstallationState and an InstallerState
// with a product being updated.
class FindArchiveToPatchTest : public testing::Test {
 public:
  FindArchiveToPatchTest(const FindArchiveToPatchTest&) = delete;
  FindArchiveToPatchTest& operator=(const FindArchiveToPatchTest&) = delete;

 protected:
  class FakeInstallationState : public installer::InstallationState {};

  class FakeProductState : public installer::ProductState {
   public:
    static FakeProductState* FromProductState(const ProductState* product) {
      return static_cast<FakeProductState*>(const_cast<ProductState*>(product));
    }

    void set_version(const base::Version& version) {
      if (version.IsValid())
        version_ = std::make_unique<base::Version>(version);
      else
        version_.reset();
    }

    void set_uninstall_command(const base::CommandLine& uninstall_command) {
      uninstall_command_ = uninstall_command;
    }
  };

  FindArchiveToPatchTest() {}

  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    product_version_ = base::Version("30.0.1559.0");
    max_version_ = base::Version("47.0.1559.0");

    // Install the product according to the version.
    original_state_ = std::make_unique<FakeInstallationState>();
    InstallProduct();

    // Prepare to update the product in the temp dir.
    installer_state_ = std::make_unique<installer::InstallerState>(
        kSystemInstall_ ? installer::InstallerState::SYSTEM_LEVEL
                        : installer::InstallerState::USER_LEVEL);
    installer_state_->set_target_path_for_testing(test_dir_.GetPath());

    // Create archives in the two version dirs.
    ASSERT_TRUE(
        base::CreateDirectory(GetProductVersionArchivePath().DirName()));
    ASSERT_TRUE(base::WriteFile(GetProductVersionArchivePath(), "a"));
    ASSERT_TRUE(base::CreateDirectory(GetMaxVersionArchivePath().DirName()));
    ASSERT_TRUE(base::WriteFile(GetMaxVersionArchivePath(), "b"));
  }

  void TearDown() override { original_state_.reset(); }

  base::FilePath GetArchivePath(const base::Version& version) const {
    return test_dir_.GetPath()
        .AppendASCII(version.GetString())
        .Append(installer::kInstallerDir)
        .Append(installer::kChromeArchive);
  }

  base::FilePath GetMaxVersionArchivePath() const {
    return GetArchivePath(max_version_);
  }

  base::FilePath GetProductVersionArchivePath() const {
    return GetArchivePath(product_version_);
  }

  void InstallProduct() {
    FakeProductState* product = FakeProductState::FromProductState(
        original_state_->GetNonVersionedProductState(kSystemInstall_));

    product->set_version(product_version_);
    base::CommandLine uninstall_command(
        test_dir_.GetPath()
            .AppendASCII(product_version_.GetString())
            .Append(installer::kInstallerDir)
            .Append(installer::kSetupExe));
    uninstall_command.AppendSwitch(installer::switches::kUninstall);
    product->set_uninstall_command(uninstall_command);
  }

  void UninstallProduct() {
    FakeProductState::FromProductState(
        original_state_->GetNonVersionedProductState(kSystemInstall_))
        ->set_version(base::Version());
  }

  static const bool kSystemInstall_;
  base::ScopedTempDir test_dir_;
  base::Version product_version_;
  base::Version max_version_;
  std::unique_ptr<FakeInstallationState> original_state_;
  std::unique_ptr<installer::InstallerState> installer_state_;

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

const bool FindArchiveToPatchTest::kSystemInstall_ = false;

}  // namespace

// Test that the path to the advertised product version is found.
TEST_F(FindArchiveToPatchTest, ProductVersionFound) {
  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version()));
  EXPECT_EQ(GetProductVersionArchivePath().value(), patch_source.value());
}

// Test that the path to the max version is found if the advertised version is
// missing.
TEST_F(FindArchiveToPatchTest, MaxVersionFound) {
  // The patch file is absent.
  ASSERT_TRUE(base::DeleteFile(GetProductVersionArchivePath()));
  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version()));
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source.value());

  // The product doesn't appear to be installed, so the max version is found.
  UninstallProduct();
  patch_source = installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version());
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source.value());
}

// Test that an empty path is returned if no version is found.
TEST_F(FindArchiveToPatchTest, NoVersionFound) {
  // The product doesn't appear to be installed and no archives are present.
  UninstallProduct();
  ASSERT_TRUE(base::DeleteFile(GetProductVersionArchivePath()));
  ASSERT_TRUE(base::DeleteFile(GetMaxVersionArchivePath()));

  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version()));
  EXPECT_EQ(base::FilePath::StringType(), patch_source.value());
}

TEST_F(FindArchiveToPatchTest, DesiredVersionFound) {
  base::FilePath patch_source1(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, product_version_));
  EXPECT_EQ(GetProductVersionArchivePath().value(), patch_source1.value());
  base::FilePath patch_source2(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, max_version_));
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source2.value());
}

TEST_F(FindArchiveToPatchTest, DesiredVersionNotFound) {
  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version("1.2.3.4")));
  EXPECT_EQ(base::FilePath().value(), patch_source.value());
}

TEST(SetupUtilTest, ContainsUnsupportedSwitch) {
  EXPECT_FALSE(installer::ContainsUnsupportedSwitch(
      base::CommandLine::FromString(L"foo.exe")));
  EXPECT_TRUE(installer::ContainsUnsupportedSwitch(
      base::CommandLine::FromString(L"foo.exe --chrome-frame")));
}

TEST(SetupUtilTest, GetConsoleSessionStartTime) {
  base::Time start_time = installer::GetConsoleSessionStartTime();
  EXPECT_FALSE(start_time.is_null());
}

TEST(SetupUtilTest, DecodeDMTokenSwitchValue) {
  // Expect false with empty or badly formed base64-encoded string.
  EXPECT_FALSE(installer::DecodeDMTokenSwitchValue(L""));
  EXPECT_FALSE(installer::DecodeDMTokenSwitchValue(L"not-ascii\xff"));
  EXPECT_FALSE(installer::DecodeDMTokenSwitchValue(L"not-base64-string"));

  std::string token("this is a token");
  std::string encoded = base::Base64Encode(token);
  EXPECT_EQ(token,
            *installer::DecodeDMTokenSwitchValue(base::UTF8ToWide(encoded)));
}

TEST(SetupUtilTest, StoreDMTokenToRegistrySuccess) {
  install_static::ScopedInstallDetails scoped_install_details(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Use the 2 argument std::string constructor so that the length of the string
  // is not calculated by assuming the input char array is null terminated.
  static constexpr char kTokenData[] = "tokens are \0 binary data";
  static constexpr DWORD kExpectedSize = sizeof(kTokenData) - 1;
  std::string token(&kTokenData[0], kExpectedSize);
  ASSERT_EQ(kExpectedSize, token.length());
  EXPECT_TRUE(installer::StoreDMToken(token));

  auto [key, name] = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  ASSERT_TRUE(key.Valid());

  DWORD size = kExpectedSize;
  std::vector<char> raw_value(size);
  DWORD dtype;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));
  EXPECT_EQ(REG_BINARY, dtype);
  ASSERT_EQ(kExpectedSize, size);
  EXPECT_EQ(0, memcmp(token.data(), raw_value.data(), kExpectedSize));

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(true));
  ASSERT_TRUE(key.Valid());

  size = kExpectedSize;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));
  EXPECT_EQ(REG_BINARY, dtype);
  ASSERT_EQ(kExpectedSize, size);
  EXPECT_EQ(0, memcmp(token.data(), raw_value.data(), kExpectedSize));
}

TEST(SetupUtilTest, StoreDMTokenToRegistryShouldFailWhenDMTokenTooLarge) {
  install_static::ScopedInstallDetails scoped_install_details(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE));

  std::string token_too_large(installer::kMaxDMTokenLength + 1, 'x');
  ASSERT_GT(token_too_large.size(), installer::kMaxDMTokenLength);

  EXPECT_FALSE(installer::StoreDMToken(token_too_large));
}

TEST(SetupUtilTest, DeleteDMTokenFromRegistrySuccess) {
  install_static::ScopedInstallDetails scoped_install_details(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Store the DMToken and confirm that it can be found in the registry.
  static constexpr char kTokenData[] = "tokens are \0 binary data";
  static constexpr DWORD kExpectedSize = sizeof(kTokenData) - 1;
  std::string token(&kTokenData[0], kExpectedSize);
  ASSERT_TRUE(installer::StoreDMToken(token));

  auto [key, name] = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  ASSERT_TRUE(key.Valid());
  ASSERT_TRUE(key.HasValue(name.c_str()));
  DWORD size = kExpectedSize;
  std::vector<char> raw_value(size);
  DWORD dtype;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(true));
  ASSERT_TRUE(key.Valid());
  ASSERT_TRUE(key.HasValue(name.c_str()));
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));

  // Delete the DMToken from registry and confirm that the corresponding value
  // can no longer be found. Since no other values were stored in the key, the
  // key is also deleted.
  ASSERT_TRUE(installer::DeleteDMToken());

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  ASSERT_FALSE(key.Valid());

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(true));
  ASSERT_FALSE(key.Valid());
}

TEST(SetupUtilTest, DeleteDMTokenFromRegistryWhenValueNotFound) {
  install_static::ScopedInstallDetails scoped_install_details(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Store an unrelated value in the registry.
  auto [key, name] = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(false), InstallUtil::BrowserLocation(false));
  ASSERT_TRUE(key.Valid());
  auto result = key.WriteValue(L"unrelated_value", L"unrelated_data");
  ASSERT_EQ(ERROR_SUCCESS, result);

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(false), InstallUtil::BrowserLocation(true));
  ASSERT_TRUE(key.Valid());
  result = key.WriteValue(L"unrelated_value", L"unrelated_data");
  ASSERT_EQ(ERROR_SUCCESS, result);

  // Validate that the DMToken value is not found in the registry.
  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  ASSERT_TRUE(key.Valid());
  ASSERT_FALSE(key.HasValue(name.c_str()));
  DWORD size = 0;
  std::vector<char> raw_value(size);
  DWORD dtype;
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(true));
  ASSERT_TRUE(key.Valid());
  ASSERT_FALSE(key.HasValue(name.c_str()));
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));

  // DMToken deletion is treated as successful if the value is not found.
  ASSERT_TRUE(installer::DeleteDMToken());

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  ASSERT_TRUE(key.Valid());
  ASSERT_FALSE(key.HasValue(name.c_str()));
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(true));
  ASSERT_TRUE(key.Valid());
  ASSERT_FALSE(key.HasValue(name.c_str()));
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));
}

TEST(SetupUtilTest, DeleteDMTokenFromRegistryWhenKeyNotFound) {
  install_static::ScopedInstallDetails scoped_install_details(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Validate that the key is not found in the registry.
  auto [key, name] = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  ASSERT_FALSE(key.Valid());

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(true));
  ASSERT_FALSE(key.Valid());

  // DMToken deletion is treated as successful if the key is not found.
  ASSERT_TRUE(installer::DeleteDMToken());

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  ASSERT_FALSE(key.Valid());

  std::tie(key, name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(true));
  ASSERT_FALSE(key.Valid());
}

TEST(SetupUtilTest, WerHelperRegPath) {
  // Must return a valid regpath, never an empty string.
  ASSERT_FALSE(installer::GetWerHelperRegistryPath().empty());
}

namespace installer {

class DeleteRegistryKeyPartialTest : public ::testing::Test {
 protected:
  using RegKey = base::win::RegKey;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(_registry_override_manager.OverrideRegistry(root_));
    to_preserve_.push_back(L"preSERve1");
    to_preserve_.push_back(L"1evRESerp");
  }

  void CreateSubKeys(bool with_preserves) {
    ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
    // These subkeys are added such that 1) keys to preserve are intermixed with
    // other keys, and 2) the case of the keys to preserve doesn't match the
    // values in |to_preserve_|.
    ASSERT_EQ(
        ERROR_SUCCESS,
        RegKey(root_, path_.c_str(), KEY_WRITE).CreateKey(L"0sub", KEY_WRITE));
    if (with_preserves) {
      ASSERT_EQ(ERROR_SUCCESS, RegKey(root_, path_.c_str(), KEY_WRITE)
                                   .CreateKey(L"1evreserp", KEY_WRITE));
    }
    ASSERT_EQ(
        ERROR_SUCCESS,
        RegKey(root_, path_.c_str(), KEY_WRITE).CreateKey(L"asub", KEY_WRITE));
    if (with_preserves) {
      ASSERT_EQ(ERROR_SUCCESS, RegKey(root_, path_.c_str(), KEY_WRITE)
                                   .CreateKey(L"preserve1", KEY_WRITE));
    }
    ASSERT_EQ(
        ERROR_SUCCESS,
        RegKey(root_, path_.c_str(), KEY_WRITE).CreateKey(L"sub1", KEY_WRITE));
  }

  const HKEY root_ = HKEY_CURRENT_USER;
  std::wstring path_ = L"key_path";
  std::vector<std::wstring> to_preserve_;

 private:
  registry_util::RegistryOverrideManager _registry_override_manager;
};

TEST_F(DeleteRegistryKeyPartialTest, NoKey) {
  DeleteRegistryKeyPartial(root_, L"does_not_exist",
                           std::vector<std::wstring>());
  DeleteRegistryKeyPartial(root_, L"does_not_exist", to_preserve_);
}

TEST_F(DeleteRegistryKeyPartialTest, EmptyKey) {
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), std::vector<std::wstring>());
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());

  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), to_preserve_);
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
}

TEST_F(DeleteRegistryKeyPartialTest, NonEmptyKey) {
  CreateSubKeys(false); /* !with_preserves */

  // Put some values into the main key.
  {
    RegKey key(root_, path_.c_str(), KEY_SET_VALUE);
    ASSERT_TRUE(key.Valid());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(nullptr, 5U));
    ASSERT_EQ(
        1u,
        base::win::RegistryValueIterator(root_, path_.c_str()).ValueCount());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"foo", L"bar"));
    ASSERT_EQ(
        2u,
        base::win::RegistryValueIterator(root_, path_.c_str()).ValueCount());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"baz", L"huh"));
    ASSERT_EQ(
        3u,
        base::win::RegistryValueIterator(root_, path_.c_str()).ValueCount());
  }

  DeleteRegistryKeyPartial(root_, path_.c_str(), std::vector<std::wstring>());
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());

  CreateSubKeys(false); /* !with_preserves */
  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), to_preserve_);
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
}

TEST_F(DeleteRegistryKeyPartialTest, NonEmptyKeyWithPreserve) {
  CreateSubKeys(true); /* with_preserves */

  // Put some values into the main key.
  {
    RegKey key(root_, path_.c_str(), KEY_SET_VALUE);
    ASSERT_TRUE(key.Valid());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(nullptr, 5U));
    ASSERT_EQ(
        1u,
        base::win::RegistryValueIterator(root_, path_.c_str()).ValueCount());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"foo", L"bar"));
    ASSERT_EQ(
        2u,
        base::win::RegistryValueIterator(root_, path_.c_str()).ValueCount());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"baz", L"huh"));
    ASSERT_EQ(
        3u,
        base::win::RegistryValueIterator(root_, path_.c_str()).ValueCount());
  }

  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), to_preserve_);
  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_READ).Valid());

  // Ensure that the preserved subkeys are still present.
  {
    base::win::RegistryKeyIterator it(root_, path_.c_str());
    ASSERT_EQ(to_preserve_.size(), it.SubkeyCount());
    std::wstring (*to_lower)(std::wstring_view) = &base::ToLowerASCII;
    for (; it.Valid(); ++it) {
      ASSERT_TRUE(
          base::Contains(to_preserve_, base::ToLowerASCII(it.Name()), to_lower))
          << it.Name();
    }
  }

  // Ensure that all values are absent.
  {
    base::win::RegistryValueIterator it(root_, path_.c_str());
    ASSERT_EQ(0u, it.ValueCount());
  }
}

class LegacyCleanupsTest : public ::testing::Test {
 public:
  LegacyCleanupsTest(const LegacyCleanupsTest&) = delete;
  LegacyCleanupsTest& operator=(const LegacyCleanupsTest&) = delete;

 protected:
  LegacyCleanupsTest() = default;
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    installer_state_ =
        std::make_unique<FakeInstallerState>(temp_dir_.GetPath());
    // Create the state to be cleared.
#if !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
    ASSERT_TRUE(base::win::RegKey(HKEY_CURRENT_USER, kBinariesClientsKeyPath,
                                  KEY_WRITE | KEY_WOW64_32KEY)
                    .Valid());
    ASSERT_TRUE(base::win::RegKey(HKEY_CURRENT_USER, kCommandExecuteImplClsid,
                                  KEY_WRITE)
                    .Valid());
#endif
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    ASSERT_TRUE(base::win::RegKey(HKEY_CURRENT_USER, kAppLauncherClientsKeyPath,
                                  KEY_WRITE | KEY_WOW64_32KEY)
                    .Valid());
    ASSERT_TRUE(
        base::win::RegKey(HKEY_CURRENT_USER,
                          GetChromeAppCommandPath(L"install-extension").c_str(),
                          KEY_WRITE | KEY_WOW64_32KEY)
            .Valid());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  }

  const InstallerState& installer_state() const { return *installer_state_; }

#if !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  bool HasBinariesVersionKey() const {
    return base::win::RegKey(HKEY_CURRENT_USER, kBinariesClientsKeyPath,
                             KEY_QUERY_VALUE | KEY_WOW64_32KEY)
        .Valid();
  }

  bool HasCommandExecuteImplClassKey() const {
    return base::win::RegKey(HKEY_CURRENT_USER, kCommandExecuteImplClsid,
                             KEY_QUERY_VALUE)
        .Valid();
  }
#endif  // !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  bool HasAppLauncherVersionKey() const {
    return base::win::RegKey(HKEY_CURRENT_USER, kAppLauncherClientsKeyPath,
                             KEY_QUERY_VALUE | KEY_WOW64_32KEY)
        .Valid();
  }

  bool HasInstallExtensionCommand() const {
    return base::win::RegKey(
               HKEY_CURRENT_USER,
               GetChromeAppCommandPath(L"install-extension").c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY)
        .Valid();
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

 private:
  // An InstallerState for a per-user install of Chrome in a given directory.
  class FakeInstallerState : public InstallerState {
   public:
    explicit FakeInstallerState(const base::FilePath& target_path) {
      operation_ = InstallerState::SINGLE_INSTALL_OR_UPDATE;
      target_path_ = target_path;
      state_key_ = install_static::GetClientStateKeyPath();
      level_ = InstallerState::USER_LEVEL;
      root_key_ = HKEY_CURRENT_USER;
    }
  };

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::wstring GetChromeAppCommandPath(const wchar_t* command) const {
    return std::wstring(
               L"SOFTWARE\\Google\\Update\\Clients\\"
               L"{8A69D345-D564-463c-AFF1-A69D9E530F96}\\Commands\\") +
           command;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  static const wchar_t kBinariesClientsKeyPath[];
  static const wchar_t kCommandExecuteImplClsid[];
#endif
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static const wchar_t kAppLauncherClientsKeyPath[];
#endif

  base::ScopedTempDir temp_dir_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  std::unique_ptr<FakeInstallerState> installer_state_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t LegacyCleanupsTest::kBinariesClientsKeyPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\"
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";
const wchar_t LegacyCleanupsTest::kCommandExecuteImplClsid[] =
    L"Software\\Classes\\CLSID\\{5C65F4B0-3651-4514-B207-D10CB699B14B}";
const wchar_t LegacyCleanupsTest::kAppLauncherClientsKeyPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\"
    L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";
#elif BUILDFLAG(CHROMIUM_BRANDING) && \
    !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
const wchar_t LegacyCleanupsTest::kBinariesClientsKeyPath[] =
    L"SOFTWARE\\Chromium Binaries";
const wchar_t LegacyCleanupsTest::kCommandExecuteImplClsid[] =
    L"Software\\Classes\\CLSID\\{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}";
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(LegacyCleanupsTest, NoOpOnFailedUpdate) {
  DoLegacyCleanups(installer_state(), INSTALL_FAILED);
#if !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  EXPECT_TRUE(HasBinariesVersionKey());
  EXPECT_TRUE(HasCommandExecuteImplClassKey());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(HasAppLauncherVersionKey());
  EXPECT_TRUE(HasInstallExtensionCommand());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
}

TEST_F(LegacyCleanupsTest, Do) {
  DoLegacyCleanups(installer_state(), NEW_VERSION_UPDATED);
#if !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  EXPECT_FALSE(HasBinariesVersionKey());
  EXPECT_FALSE(HasCommandExecuteImplClassKey());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_FALSE(HasAppLauncherVersionKey());
  EXPECT_FALSE(HasInstallExtensionCommand());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // !BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
}

}  // namespace installer
