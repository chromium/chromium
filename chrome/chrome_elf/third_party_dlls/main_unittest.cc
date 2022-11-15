// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/main.h"

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/chrome_elf/sha1/sha1.h"
#include "chrome/chrome_elf/third_party_dlls/hook.h"
#include "chrome/chrome_elf/third_party_dlls/main_unittest_exe.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_file.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace third_party_dlls {
namespace {

constexpr wchar_t kTestExeFilename[] = L"third_party_dlls_test_exe.exe";
constexpr wchar_t kTestBlFileName[] = L"blfile";
constexpr wchar_t kTestDllName1[] = L"main_unittest_dll_1.dll";
constexpr wchar_t kTestDllName1MixedCase[] = L"MaiN_uniTtest_dLL_1.Dll";
constexpr wchar_t kTestDllName2[] = L"main_unittest_dll_2.dll";
constexpr wchar_t kChineseUnicode[] = {0x68D5, 0x8272, 0x72D0, 0x72F8, 0x002E,
                                       0x0064, 0x006C, 0x006C, 0x0000};
constexpr wchar_t kOldBlocklistDllName[] = L"libapi2hook.dll";

struct TestModuleData {
  std::string image_name;
  std::string section_path;
  std::string section_basename;
  DWORD timedatestamp;
  DWORD imagesize;
};

// NOTE: TestTimeouts::action_max_timeout() is not long enough here.
base::TimeDelta g_timeout =
    ::IsDebuggerPresent() ? base::TimeDelta::Max() : base::Milliseconds(5000);

// Centralize child test process control.
void LaunchChildAndWait(const base::CommandLine& command_line, int* exit_code) {
  base::Process proc =
      base::LaunchProcess(command_line, base::LaunchOptionsForTest());
  ASSERT_TRUE(proc.IsValid());

  *exit_code = 0;
  if (!proc.WaitForExitWithTimeout(g_timeout, exit_code)) {
    // Timeout while waiting.  Try to cleanup.
    proc.Terminate(1, false);
    ADD_FAILURE();
  }

  return;
}

// Given the name and path of a test DLL, mine the data of interest out of it
// and return it via |test_module|.
// - Note: the DLL must be loaded into memory by NTLoader to mine all of the
//         desired data (not just read from disk).
bool GetTestModuleData(const std::wstring& file_name,
                       const std::wstring& file_path,
                       TestModuleData* test_module) {
  base::FilePath path(file_path);
  path = path.Append(file_name);

  // Map the target DLL into memory just long enough to mine data out of it.
  base::ScopedNativeLibrary test_dll(path);
  if (!test_dll.is_valid())
    return false;

  return GetDataFromImageForTesting(
      test_dll.get(), &test_module->timedatestamp, &test_module->imagesize,
      &test_module->image_name, &test_module->section_path,
      &test_module->section_basename);
}

// Turn given data into a PackedListModule structure.
// - |image_name| should be utf-8 at this point.
PackedListModule GeneratePackedListModule(const std::string& image_name,
                                          DWORD timedatestamp,
                                          DWORD imagesize) {
  // Internally, an empty string should not be passed in here.
  assert(!image_name.empty());

  // SHA1 hash the two strings into the new struct.
  PackedListModule packed_module;
  packed_module.code_id_hash =
      elf_sha1::SHA1HashString(GetFingerprintString(timedatestamp, imagesize));
  packed_module.basename_hash = elf_sha1::SHA1HashString(image_name);

  return packed_module;
}

inline std::wstring MakePath(const std::wstring& path,
                             const std::wstring& name) {
  std::wstring full_path(path);
  full_path.push_back(L'\\');
  full_path.append(name);
  return full_path;
}

inline bool MakeFileCopy(const std::wstring& old_path,
                         const std::wstring& old_name,
                         const std::wstring& new_path,
                         const std::wstring& new_name) {
  base::FilePath source(MakePath(old_path, old_name));
  base::FilePath destination(MakePath(new_path, new_name));
  return base::CopyFileW(source, destination);
}

// Utility function for protecting local registry.
void RegRedirect(nt::ROOT_KEY key,
                 registry_util::RegistryOverrideManager* rom) {
  ASSERT_NE(key, nt::AUTO);
  HKEY root = (key == nt::HKCU ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE);
  std::wstring temp;

  ASSERT_NO_FATAL_FAILURE(rom->OverrideRegistry(root, &temp));
  ASSERT_TRUE(nt::SetTestingOverride(key, temp));
}

// Utility function to disable local registry protection.
void CancelRegRedirect(nt::ROOT_KEY key) {
  ASSERT_NE(key, nt::AUTO);
  ASSERT_TRUE(nt::SetTestingOverride(key, std::wstring()));
}

// Use NtRegistry to query status codes (it's more handy than base).
// - Returns true if the key value exists and is type REG_BINARY.
bool QueryStatusCodes(std::vector<ThirdPartyStatus>* status_array) {
  HANDLE handle = nullptr;
  if (!nt::OpenRegKey(nt::HKCU,
                      install_static::GetRegistryPath()
                          .append(kThirdPartyRegKeyName)
                          .c_str(),
                      KEY_QUERY_VALUE, &handle, nullptr)) {
    return false;
  }

  ULONG type = REG_NONE;
  std::vector<uint8_t> temp_buffer;
  bool success =
      nt::QueryRegKeyValue(handle, kStatusCodesRegValue, &type, &temp_buffer);
  nt::CloseRegKey(handle);

  if (!success || type != REG_BINARY)
    return false;

  ConvertBufferToStatusCodes(temp_buffer, status_array);

  return true;
}

//------------------------------------------------------------------------------
// ThirdPartyTest class
//------------------------------------------------------------------------------

class ThirdPartyTest : public testing::Test {
 public:
  ThirdPartyTest(const ThirdPartyTest&) = delete;
  ThirdPartyTest& operator=(const ThirdPartyTest&) = delete;

 protected:
  ThirdPartyTest() = default;

  void SetUp() override {
    // Setup temp test dir.
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    // Store full path to test file.
    base::FilePath path = scoped_temp_dir_.GetPath();
    path = path.Append(kTestBlFileName);
    bl_test_file_path_ = std::move(path.value());

    // Also store a copy of current exe directory for efficiency.
    base::FilePath exe;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe));
    exe_dir_ = std::move(exe.value());

    // Create the blocklist file empty.
    base::File file(base::FilePath(bl_test_file_path_),
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                        base::File::FLAG_WIN_SHARE_DELETE |
                        base::File::FLAG_DELETE_ON_CLOSE);
    ASSERT_TRUE(file.IsValid());

    // Leave file handle open for DELETE_ON_CLOSE.
    bl_file_ = std::move(file);
  }

  void TearDown() override {}

  // Overwrite the content of the blocklist file.
  bool WriteModulesToBlocklist(const std::vector<PackedListModule>& list) {
    bl_file_.SetLength(0);

    // Write content {metadata}{array_of_modules}.
    PackedListMetadata meta = {kInitialVersion,
                               static_cast<uint32_t>(list.size())};

    if (bl_file_.Write(0, reinterpret_cast<const char*>(&meta), sizeof(meta)) !=
        static_cast<int>(sizeof(meta))) {
      return false;
    }
    int size = static_cast<int>(list.size() * sizeof(PackedListModule));
    if (bl_file_.Write(sizeof(PackedListMetadata),
                       reinterpret_cast<const char*>(list.data()),
                       size) != size) {
      return false;
    }

    return true;
  }

  const std::wstring& GetBlTestFilePath() { return bl_test_file_path_; }
  const std::wstring& GetExeDir() { return exe_dir_; }
  const std::wstring& GetScopedTempDirValue() {
    return scoped_temp_dir_.GetPath().value();
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  base::File bl_file_;
  std::wstring bl_test_file_path_;
  std::wstring exe_dir_;
};

//------------------------------------------------------------------------------
// Third-party tests
//
// These tests spawn a child test process to keep the hooking contained to a
// separate process.  This prevents test clashes in certain testing
// configurations.
//------------------------------------------------------------------------------

#if BUILDFLAG(IS_WIN)
#define MAYBE_Base DISABLED_Base
#else
#define MAYBE_Base Base
#endif
// Note: The test module used in this unittest has no export table.
TEST_F(ThirdPartyTest, MAYBE_Base) {
  // 1. Spawn the test process with NO blocklist.  Expect successful
  // initialization.
  base::CommandLine cmd_line1 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line1.AppendArgNative(GetBlTestFilePath());
  cmd_line1.AppendArgNative(base::NumberToWString(kTestOnlyInitialization));

  int exit_code = 0;
  LaunchChildAndWait(cmd_line1, &exit_code);
  ASSERT_EQ(kDllLoadSuccess, exit_code);

  //----------------------------------------------------------------------------
  // 2. Spawn the test process with NO blocklist.  Expect successful DLL load.
  base::CommandLine cmd_line2 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line2.AppendArgNative(GetBlTestFilePath());
  cmd_line2.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line2.AppendArgNative(MakePath(GetExeDir(), kTestDllName1));

  LaunchChildAndWait(cmd_line2, &exit_code);
  ASSERT_EQ(kDllLoadSuccess, exit_code);

  //----------------------------------------------------------------------------
  // 3. Spawn the test process with blocklist.  Expect failed DLL load.
  TestModuleData module_data = {};
  ASSERT_TRUE(GetTestModuleData(kTestDllName1, GetExeDir(), &module_data));

  // Note: image_name will be empty, as there is no export table in this test
  //       module.
  EXPECT_TRUE(module_data.image_name.empty());

  std::vector<PackedListModule> vector(1);
  vector.emplace_back(GeneratePackedListModule(module_data.section_basename,
                                               module_data.timedatestamp,
                                               module_data.imagesize));
  ASSERT_TRUE(WriteModulesToBlocklist(vector));

  base::CommandLine cmd_line3 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line3.AppendArgNative(GetBlTestFilePath());
  cmd_line3.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line3.AppendArgNative(MakePath(GetExeDir(), kTestDllName1));

  LaunchChildAndWait(cmd_line3, &exit_code);
  ASSERT_EQ(kDllLoadFailed, exit_code);

  //----------------------------------------------------------------------------
  // 4. Spawn the test process with blocklist.  Expect failed DLL load.
  //    ** Rename the module with some upper-case characters to test that
  //       the hook matching handles case properly.
  ASSERT_TRUE(MakeFileCopy(GetExeDir(), kTestDllName1, GetScopedTempDirValue(),
                           kTestDllName1MixedCase));

  // Note: the blocklist is already set from the previous test.
  // Note: using the module with no export table for this test, to ensure that
  //       the section name (the rename) is used in the comparison.

  base::CommandLine cmd_line4 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line4.AppendArgNative(GetBlTestFilePath());
  cmd_line4.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line4.AppendArgNative(
      MakePath(GetScopedTempDirValue(), kTestDllName1MixedCase));

  LaunchChildAndWait(cmd_line4, &exit_code);
  ASSERT_EQ(kDllLoadFailed, exit_code);
}

// Note: The test module used in this unittest has no export table.
TEST_F(ThirdPartyTest, WideCharEncoding) {
  // Rename module to chinese unicode (kChineseUnicode).  Be sure to handle
  // any conversions to UTF8 appropriately here.  No ASCII.
  ASSERT_TRUE(MakeFileCopy(GetExeDir(), kTestDllName1, GetScopedTempDirValue(),
                           kChineseUnicode));

  // 1) Test a successful DLL load with no blocklist.
  base::CommandLine cmd_line1 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line1.AppendArgNative(GetBlTestFilePath());
  cmd_line1.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line1.AppendArgNative(MakePath(GetScopedTempDirValue(), kChineseUnicode));

  int exit_code = 0;
  LaunchChildAndWait(cmd_line1, &exit_code);
  ASSERT_EQ(kDllLoadSuccess, exit_code);

  //----------------------------------------------------------------------------
  // 2) Test a failed DLL load with blocklist.
  TestModuleData module_data = {};
  ASSERT_TRUE(GetTestModuleData(kChineseUnicode, GetScopedTempDirValue(),
                                &module_data));

  // Note: image_name will be empty, as there is no export table in this test
  //       module.
  EXPECT_TRUE(module_data.image_name.empty());

  std::vector<PackedListModule> vector;
  vector.emplace_back(GeneratePackedListModule(module_data.section_basename,
                                               module_data.timedatestamp,
                                               module_data.imagesize));
  ASSERT_TRUE(WriteModulesToBlocklist(vector));

  base::CommandLine cmd_line2 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line2.AppendArgNative(GetBlTestFilePath());
  cmd_line2.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line2.AppendArgNative(MakePath(GetScopedTempDirValue(), kChineseUnicode));

  LaunchChildAndWait(cmd_line2, &exit_code);
  ASSERT_EQ(kDllLoadFailed, exit_code);
}

// Note: The test module used in this unittest has an export table.
TEST_F(ThirdPartyTest, WideCharEncodingWithExportDir) {
  // Rename module to chinese unicode (kChineseUnicode).  Be sure to handle
  // any conversions to UTF8 appropriately here.  No ASCII.
  ASSERT_TRUE(MakeFileCopy(GetExeDir(), kTestDllName2, GetScopedTempDirValue(),
                           kChineseUnicode));

  // 1) Test a successful DLL load with no blocklist.
  base::CommandLine cmd_line1 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line1.AppendArgNative(GetBlTestFilePath());
  cmd_line1.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line1.AppendArgNative(MakePath(GetScopedTempDirValue(), kChineseUnicode));

  int exit_code = 0;
  LaunchChildAndWait(cmd_line1, &exit_code);
  ASSERT_EQ(kDllLoadSuccess, exit_code);

  //----------------------------------------------------------------------------
  // 2) Test a failed DLL load with blocklist.
  TestModuleData module_data = {};
  ASSERT_TRUE(GetTestModuleData(kChineseUnicode, GetScopedTempDirValue(),
                                &module_data));
  // Ensure the export section was found as expected.
  EXPECT_FALSE(module_data.image_name.empty());

  // NOTE: a file rename does not affect the module name mined from the export
  //       table in the PE.  So image_name and section_basename will be
  //       different. Ensure blocklisting both section name and image name
  //       works!

  // 2a) Only blocklist the original DLL name, which should be mined out of the
  //     export table by the hook, and the load should be blocked.
  std::vector<PackedListModule> vector;
  vector.emplace_back(GeneratePackedListModule(base::WideToASCII(kTestDllName2),
                                               module_data.timedatestamp,
                                               module_data.imagesize));
  ASSERT_TRUE(WriteModulesToBlocklist(vector));

  base::CommandLine cmd_line2 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line2.AppendArgNative(GetBlTestFilePath());
  cmd_line2.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line2.AppendArgNative(MakePath(GetScopedTempDirValue(), kChineseUnicode));

  LaunchChildAndWait(cmd_line2, &exit_code);
  ASSERT_EQ(kDllLoadFailed, exit_code);

  // 2b) Only blocklist the new DLL file name, which should be mined out of the
  //     section by the hook, and the load should be blocked.
  vector.clear();
  vector.emplace_back(GeneratePackedListModule(
      base::WideToUTF8(kChineseUnicode), module_data.timedatestamp,
      module_data.imagesize));
  ASSERT_TRUE(WriteModulesToBlocklist(vector));

  base::CommandLine cmd_line3 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line3.AppendArgNative(GetBlTestFilePath());
  cmd_line3.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line3.AppendArgNative(MakePath(GetScopedTempDirValue(), kChineseUnicode));

  LaunchChildAndWait(cmd_line3, &exit_code);
  ASSERT_EQ(kDllLoadFailed, exit_code);
}

// Note: The test module used in this unittest has no export table.
TEST_F(ThirdPartyTest, DeprecatedBlocklistSanityCheck) {
  // Rename module to something on the old, deprecated, hard-coded blocklist.
  ASSERT_TRUE(MakeFileCopy(GetExeDir(), kTestDllName1, GetScopedTempDirValue(),
                           kOldBlocklistDllName));

  // 1) Test a failed DLL load with no blocklist (the old, hard-coded blocklist
  //    should trigger a block).
  base::CommandLine cmd_line1 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line1.AppendArgNative(GetBlTestFilePath());
  cmd_line1.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line1.AppendArgNative(
      MakePath(GetScopedTempDirValue(), kOldBlocklistDllName));

  int exit_code = 0;
  LaunchChildAndWait(cmd_line1, &exit_code);
  ASSERT_EQ(kDllLoadFailed, exit_code);
}

// Note: This test only sanity checks the two SHA1 libraries used on either side
//       of the Third-Party block.
//       This Side: chrome/chrome_elf\third_party_dlls\*,
//                  elf_sha1::SHA1HashString().
//       The Other Side: chrome\browser\conflicts\module_list_filter_win.cc,
//                       base::SHA1HashString().
TEST_F(ThirdPartyTest, SHA1SanityCheck) {
  // Rename module to chinese unicode (kChineseUnicode).  Be sure to handle
  // any conversions to UTF8 appropriately here.  No ASCII.
  ASSERT_TRUE(MakeFileCopy(GetExeDir(), kTestDllName1, GetScopedTempDirValue(),
                           kChineseUnicode));

  TestModuleData module_data = {};
  ASSERT_TRUE(GetTestModuleData(kChineseUnicode, GetScopedTempDirValue(),
                                &module_data));

  // Get hashes from elf_sha1.
  PackedListModule elf_sha1_generated = GeneratePackedListModule(
      base::WideToUTF8(kChineseUnicode), module_data.timedatestamp,
      module_data.imagesize);

  // Get hashes from base_sha1.
  const std::string module_basename_hash =
      base::SHA1HashString(base::WideToUTF8(kChineseUnicode));
  const std::string module_code_id_hash = base::SHA1HashString(
      GetFingerprintString(module_data.timedatestamp, module_data.imagesize));

  // Compare the hashes.
  EXPECT_EQ(::memcmp(&elf_sha1_generated.basename_hash[0],
                     module_basename_hash.data(), elf_sha1::kSHA1Length),
            0);
  EXPECT_EQ(::memcmp(&elf_sha1_generated.code_id_hash[0],
                     module_code_id_hash.data(), elf_sha1::kSHA1Length),
            0);
}

// Flaky: crbug.com/868233
#if BUILDFLAG(IS_WIN)
#define MAYBE_PathCaseSensitive DISABLED_PathCaseSensitive
#else
#define MAYBE_PathCaseSensitive PathCaseSensitive
#endif

// Test that full section path is left alone, in terms of case.
TEST_F(ThirdPartyTest, MAYBE_PathCaseSensitive) {
  // Rename module to have mixed case.
  ASSERT_TRUE(MakeFileCopy(GetExeDir(), kTestDllName2, GetScopedTempDirValue(),
                           kTestDllName1MixedCase));

  // 1) Sanity check that the hook GetDataFromImage() mining leaves the
  // section path alone.
  TestModuleData module_data = {};
  ASSERT_TRUE(GetTestModuleData(kTestDllName1MixedCase, GetScopedTempDirValue(),
                                &module_data));
  // Reminder: this string is actually UTF-8, but this test ensures it is ascii.
  // Also, |section_path| will be a device path, so convert to drive letter
  // before comparing.
  base::FilePath drive;
  ASSERT_TRUE(base::DevicePathToDriveLetterPath(
      base::FilePath(base::ASCIIToWide(module_data.section_path)), &drive));

  EXPECT_EQ(drive.value().compare(
                MakePath(GetScopedTempDirValue(), kTestDllName1MixedCase)),
            0);

  // 2) Now check an actual log.  Successful DLL load with no blocklist is fine
  //    for this test.
  base::CommandLine cmd_line1 = base::CommandLine::FromString(kTestExeFilename);
  cmd_line1.AppendArgNative(GetBlTestFilePath());
  cmd_line1.AppendArgNative(base::NumberToWString(kTestSingleDllLoad));
  cmd_line1.AppendArgNative(
      MakePath(GetScopedTempDirValue(), kTestDllName1MixedCase));

  int exit_code = 0;
  LaunchChildAndWait(cmd_line1, &exit_code);
  ASSERT_EQ(kDllLoadSuccess, exit_code);
}

// Test the status-code passing in registry.
TEST_F(ThirdPartyTest, StatusCodes) {
  // 1. Enable reg override for test net.
  registry_util::RegistryOverrideManager override_manager;
  ASSERT_NO_FATAL_FAILURE(RegRedirect(nt::HKCU, &override_manager));

  // 2. Reset the registry key and value to empty.
  ASSERT_TRUE(ResetStatusCodesForTesting());

  // 3. Confirm key and empty value.
  std::vector<ThirdPartyStatus> code_array;
  EXPECT_TRUE(QueryStatusCodes(&code_array));
  EXPECT_EQ(0u, code_array.size());

  // 4. Add status codes, then verify.
  ASSERT_NO_FATAL_FAILURE(
      AddStatusCodeForTesting(ThirdPartyStatus::kFileEmpty));
  ASSERT_NO_FATAL_FAILURE(
      AddStatusCodeForTesting(ThirdPartyStatus::kLogsCreateMutexFailure));
  ASSERT_NO_FATAL_FAILURE(
      AddStatusCodeForTesting(ThirdPartyStatus::kHookVirtualProtectFailure));
  EXPECT_TRUE(QueryStatusCodes(&code_array));
  ASSERT_EQ(3u, code_array.size());
  EXPECT_EQ(ThirdPartyStatus::kFileEmpty, code_array[0]);
  EXPECT_EQ(ThirdPartyStatus::kLogsCreateMutexFailure, code_array[1]);
  EXPECT_EQ(ThirdPartyStatus::kHookVirtualProtectFailure, code_array[2]);

  // 5. Reset the registry value to empty.
  EXPECT_TRUE(ResetStatusCodesForTesting());
  EXPECT_TRUE(QueryStatusCodes(&code_array));
  EXPECT_EQ(0u, code_array.size());

  // 6. Disable reg override.
  ASSERT_NO_FATAL_FAILURE(CancelRegRedirect(nt::HKCU));
}

}  // namespace
}  // namespace third_party_dlls
