// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/cleaner_sandbox_interface.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "chrome/chrome_cleaner/engines/common/registry_util.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/file_remover.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/strings/wstring_embedded_nulls.h"
#include "chrome/chrome_cleaner/test/file_remover_test_util.h"
#include "chrome/chrome_cleaner/test/reboot_deletion_helper.h"
#include "chrome/chrome_cleaner/test/scoped_process_protector.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_native_reg_util.h"
#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_task_scheduler.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_cleaner::CreateEmptyFile;
using chrome_cleaner::GetWow64RedirectedSystemPath;
using chrome_cleaner::IsFileRegisteredForPostRebootRemoval;
using chrome_cleaner::ScopedTempDirNoWow64;
using chrome_cleaner::WStringEmbeddedNulls;

namespace chrome_cleaner_sandbox {

namespace {

constexpr wchar_t kDirectNonRegistryPath[] = L"\\DosDevice\\C:";
constexpr wchar_t kTrickyNonRegistryPath[] =
    L"\\Registry\\Machine\\..\\..\\DosDevice\\C:";

WStringEmbeddedNulls FullyQualifiedKeyPathWithTrailingNull(
    const ScopedTempRegistryKey& temp_key,
    const std::vector<wchar_t>& key_name) {
  // key vectors are expected to end with NULL.
  DCHECK_EQ(key_name.back(), L'\0');

  std::wstring full_key_path(temp_key.FullyQualifiedPath());
  full_key_path += L"\\";
  // Include key_name's trailing NULL.
  full_key_path.append(key_name.begin(), key_name.end());
  return WStringEmbeddedNulls(full_key_path);
}

WStringEmbeddedNulls StringWithTrailingNull(const std::wstring& str) {
  // wstring::size() does not count the trailing null.
  return WStringEmbeddedNulls(str.c_str(), str.size() + 1);
}

WStringEmbeddedNulls VeryLongStringWithPrefix(
    const WStringEmbeddedNulls& prefix) {
  return WStringEmbeddedNulls(std::wstring(prefix.CastAsWCharArray()) +
                              std::wstring(kMaxRegistryParamLength, L'a'));
}

base::FilePath GetNativePath(const std::wstring& path) {
  // Add the native \??\ prefix described at
  // https://googleprojectzero.blogspot.com/2016/02/the-definitive-guide-on-win32-to-nt.html
  return base::FilePath(base::StrCat({L"\\??\\", path}));
}

base::FilePath GetUniversalPath(const std::wstring& path) {
  // Add the universal \\?\ prefix described at
  // https://docs.microsoft.com/en-us/windows/desktop/fileio/naming-a-file#namespaces
  return base::FilePath(base::StrCat({L"\\\\?\\", path}));
}

}  // namespace

class CleanerSandboxInterfaceDeleteFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    file_remover_ = std::make_unique<chrome_cleaner::FileRemover>(
        /*digest_verifier=*/nullptr, /*archiver=*/nullptr,
        chrome_cleaner::LayeredServiceProviderWrapper(),
        base::BindRepeating(
            &CleanerSandboxInterfaceDeleteFileTest::RebootRequired,
            base::Unretained(this)));
  }

  void RebootRequired() { reboot_required_ = true; }
  void ClearRebootRequired() { reboot_required_ = false; }

  std::unique_ptr<chrome_cleaner::FileRemoverAPI> file_remover_;
  bool reboot_required_ = false;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFile_BasicFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  chrome_cleaner::VerifyRemoveNowSuccess(file_path, file_remover_.get());
  EXPECT_FALSE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFile_UnicodePath) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path =
      temp.GetPath().Append(chrome_cleaner::kValidUtf8Name);

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  chrome_cleaner::VerifyRemoveNowSuccess(file_path, file_remover_.get());
  EXPECT_FALSE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFile_DirectoryTraversal) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  base::FilePath path_directory_traversal =
      file_path.Append(L"..").Append(file_path.BaseName());

  chrome_cleaner::VerifyRemoveNowFailure(path_directory_traversal,
                                         file_remover_.get());
  EXPECT_TRUE(base::PathExists(path_directory_traversal));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DirectoryDeletionFails) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath dir_path = temp.GetPath().Append(L"directory.exe");
  ASSERT_TRUE(base::CreateDirectory(dir_path));

  chrome_cleaner::VerifyRemoveNowFailure(dir_path, file_remover_.get());
  EXPECT_TRUE(base::DirectoryExists(dir_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, NotExecutableFileType) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.txt");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  chrome_cleaner::VerifyRemoveNowSuccess(file_path, file_remover_.get());
  EXPECT_FALSE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFile_RelativeFilePath) {
  base::FilePath file_path(L"temp_file.exe");

  chrome_cleaner::VerifyRemoveNowFailure(file_path, file_remover_.get());
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFile_NativePath) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");

  chrome_cleaner::VerifyRemoveNowFailure(GetNativePath(file_path.value()),
                                         file_remover_.get());
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFile_UniversalPath) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");

  chrome_cleaner::VerifyRemoveNowFailure(GetUniversalPath(file_path.value()),
                                         file_remover_.get());
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFile_Wow64Disabled) {
  // Skip this test on 32-bit Windows because there Wow64 redirection is not
  // supported.
  if (!chrome_cleaner::IsX64Architecture())
    return;

  static constexpr wchar_t kTestFile[] = L"temp_file.exe";

  // Create a directory and a file under true System32 directory.
  ScopedTempDirNoWow64 dir_in_system32;
  ASSERT_TRUE(
      dir_in_system32.CreateEmptyFileInUniqueSystem32TempDir(kTestFile));
  const base::FilePath dir_name = dir_in_system32.GetPath().BaseName();
  const base::FilePath file_in_system32 =
      dir_in_system32.GetPath().Append(kTestFile);

  // Mirror exactly the created folder and file under SysWOW64.
  // C:\Windows\
  //       system32\
  //           dirname\  temp_file.exe
  //       SysWOW64\
  //           dirname\  temp_file.exe
  base::FilePath syswow64_path = GetWow64RedirectedSystemPath();
  base::ScopedTempDir dir_in_syswow64;
  ASSERT_TRUE(dir_in_syswow64.Set(syswow64_path.Append(dir_name)));
  ASSERT_TRUE(
      chrome_cleaner::CreateFileInFolder(dir_in_syswow64.GetPath(), kTestFile));
  const base::FilePath file_in_syswow64 =
      dir_in_syswow64.GetPath().Append(kTestFile);

  // Delete file from C:\Windows\system32\ directory and verify it is not
  // deleted from the corresponding SysWOW64 directory.
  chrome_cleaner::VerifyRemoveNowSuccess(file_in_system32, file_remover_.get());

  {
    chrome_cleaner::ScopedDisableWow64Redirection no_wow64_redirection;
    EXPECT_FALSE(base::PathExists(file_in_system32));
    EXPECT_TRUE(base::PathExists(file_in_syswow64));
  }

  // Create a subdirectory of the temp dir in System32 that looks like an
  // active file name. The corresponding path in SysWOW64 should already exist
  // from the previous test.
  {
    chrome_cleaner::ScopedDisableWow64Redirection no_wow64_redirection;
    ASSERT_TRUE(base::CreateDirectory(file_in_system32));
    ASSERT_TRUE(base::PathExists(file_in_syswow64));
  }

  // Make sure the subdirectory can't be deleted. This tests that the "is this
  // a directory" validation is not redirected to the SysWOW64 path, which is
  // actually a file.
  chrome_cleaner::VerifyRemoveNowFailure(file_in_system32, file_remover_.get());
  {
    chrome_cleaner::ScopedDisableWow64Redirection no_wow64_redirection;
    EXPECT_TRUE(base::DirectoryExists(file_in_system32));
    EXPECT_TRUE(base::PathExists(file_in_syswow64));
  }
}

// Tests MoveFileEx functionality. Note that this test will pollute your
// registry with removal entries for non-existent files. Standard registry
// redirection using RegOverridePredefKey unfortunately doesn't work for
// MoveFileEx. This should be mostly harmless.
TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteFilePostReboot) {
  base::ScopedTempDir dir;
  EXPECT_TRUE(dir.CreateUniqueTempDir());

  const base::FilePath file_path(dir.GetPath().Append(L"a.exe"));
  EXPECT_TRUE(CreateEmptyFile(file_path));

  chrome_cleaner::VerifyRegisterPostRebootRemovalSuccess(file_path,
                                                         file_remover_.get());
  EXPECT_TRUE(reboot_required_);

  ClearRebootRequired();
  chrome_cleaner::VerifyRegisterPostRebootRemovalFailure(dir.GetPath(),
                                                         file_remover_.get());
  EXPECT_FALSE(reboot_required_);
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteSymlinkToFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  base::FilePath link_path = temp.GetPath().Append(L"link_file.exe");
  ASSERT_NE(0, ::CreateSymbolicLink(link_path.value().c_str(),
                                    file_path.value().c_str(), 0));

  chrome_cleaner::VerifyRemoveNowSuccess(link_path, file_remover_.get());
  EXPECT_FALSE(base::PathExists(link_path));
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteSymlinkToFolderFails) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath subdir_path = temp.GetPath().Append(L"dir");
  ASSERT_TRUE(base::CreateDirectory(subdir_path));
  base::FilePath link_path = temp.GetPath().Append(L"link_file.exe");

  ASSERT_NE(0, ::CreateSymbolicLink(link_path.value().c_str(),
                                    subdir_path.value().c_str(),
                                    SYMBOLIC_LINK_FLAG_DIRECTORY));

  chrome_cleaner::VerifyRemoveNowFailure(link_path, file_remover_.get());
  EXPECT_TRUE(base::PathExists(link_path));
  EXPECT_TRUE(base::DirectoryExists(subdir_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, AllowTrailingWhitespace) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  const base::FilePath path_with_space(file_path.value() + L" ");
  chrome_cleaner::VerifyRemoveNowSuccess(path_with_space, file_remover_.get());
  EXPECT_FALSE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, QuotedPath) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  const base::FilePath quoted_path(L"\"" + file_path.value() + L"\"");

  // RemoveNow should reject the file name because it starts with an invalid
  // character. This needs to match the behaviour of SandboxOpenFileReadOnly,
  // which is tested in ScannerSandboxInterface_OpenReadOnlyFile.BasicFile,
  // since the same path could be passed to both.
  chrome_cleaner::VerifyRemoveNowFailure(quoted_path, file_remover_.get());
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, QuotedFilename) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  const base::FilePath quoted_path =
      temp.GetPath().Append(L"\"temp_file.exe\"");

  // RemoveNow should return true because the file name is valid, but refers to
  // a file that already doesn't exist. The important thing is that the quotes
  // aren't interpreted, which would cause '"temp_file.exe"' and
  // 'temp_file.exe' to refer to the same thing.
  //
  // This needs to match the behaviour of SandboxOpenFileReadOnly, which is
  // tested in ScannerSandboxInterface_OpenReadOnlyFile.BasicFile, since the
  // same path could be passed to both. It would also be ok if both RemoveNow
  // and OpenFileReadOnly interpreted the quotes, as long as their behaviour
  // matches.
  chrome_cleaner::VerifyRemoveNowSuccess(quoted_path, file_remover_.get());
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteAlternativeStream) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.txt");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  constexpr char kTestContent[] = "content";
  const base::FilePath stream_path(file_path.value() + L":stream");
  chrome_cleaner::CreateFileWithContent(stream_path, kTestContent,
                                        sizeof(kTestContent));
  EXPECT_TRUE(base::PathExists(stream_path));

  chrome_cleaner::VerifyRemoveNowSuccess(stream_path, file_remover_.get());

  // The alternative stream should be gone, but the file itself should still be
  // present on the system.
  EXPECT_FALSE(base::PathExists(stream_path));
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, DeleteAlternativeStreamWithType) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  constexpr char kTestContent[] = "content";
  const base::FilePath stream_path(file_path.value() + L":stream");
  const base::FilePath stream_type_path(stream_path.value() + L":$DATA");

  // Deleting using the full path with data type should work.
  chrome_cleaner::CreateFileWithContent(stream_type_path, kTestContent,
                                        sizeof(kTestContent));
  EXPECT_TRUE(base::PathExists(stream_path));
  EXPECT_TRUE(base::PathExists(stream_type_path));

  chrome_cleaner::VerifyRemoveNowSuccess(stream_type_path, file_remover_.get());

  EXPECT_FALSE(base::PathExists(stream_path));
  EXPECT_FALSE(base::PathExists(stream_type_path));
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest,
       DeleteExecutableWithDefaultStream) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath executable_path = temp.GetPath().Append(L"temp_file.exe");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      executable_path.DirName(), executable_path.BaseName().value().c_str()));

  // ::$DATA denotes the default stream, and therefore should exist.
  const base::FilePath path_with_datatype(executable_path.value() + L"::$DATA");
  EXPECT_TRUE(base::PathExists(path_with_datatype));

  // Appending ::$DATA to file path should not prevent file extension checks.
  chrome_cleaner::VerifyRemoveNowSuccess(path_with_datatype,
                                         file_remover_.get());
  EXPECT_FALSE(base::PathExists(executable_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, TextWithDefaultStream) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.txt");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  // ::$DATA denotes the default stream, and therefore should exist.
  const base::FilePath path_with_datatype(file_path.value() + L"::$DATA");
  EXPECT_TRUE(base::PathExists(path_with_datatype));

  chrome_cleaner::VerifyRemoveNowSuccess(path_with_datatype,
                                         file_remover_.get());
  EXPECT_FALSE(base::PathExists(file_path));
}

TEST_F(CleanerSandboxInterfaceDeleteFileTest, RecognizedDigest) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.exe");
  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  auto remover_with_digest_verifier =
      std::make_unique<chrome_cleaner::FileRemover>(
          chrome_cleaner::DigestVerifier::CreateFromFile(file_path),
          /*archiver=*/nullptr, chrome_cleaner::LayeredServiceProviderWrapper(),
          base::DoNothing());

  chrome_cleaner::VerifyRemoveNowFailure(file_path,
                                         remover_with_digest_verifier.get());
  EXPECT_TRUE(base::PathExists(file_path));
}

class CleanerInterfaceRegistryTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::vector<wchar_t> key_name{L'a', L'b', L'\0', L'c', L'\0'};

    ULONG disposition = 0;
    EXPECT_EQ(STATUS_SUCCESS, NativeCreateKey(temp_key_.Get(), &key_name,
                                              &subkey_handle_, &disposition));
    ASSERT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);
    ASSERT_NE(INVALID_HANDLE_VALUE, subkey_handle_);
    full_key_path_ = FullyQualifiedKeyPathWithTrailingNull(temp_key_, key_name);

    // Create a default and a named value.
    EXPECT_EQ(STATUS_SUCCESS,
              NativeSetValueKey(subkey_handle_, WStringEmbeddedNulls(nullptr),
                                REG_SZ, value_));
    EXPECT_EQ(STATUS_SUCCESS,
              NativeSetValueKey(subkey_handle_, value_name_, REG_SZ, value_));

    default_value_should_be_normalized_ = base::BindRepeating(
        &chrome_cleaner_sandbox::DefaultShouldValueBeNormalized);
  }

  void TearDown() override {
    if (subkey_handle_ != INVALID_HANDLE_VALUE) {
      NativeDeleteKey(subkey_handle_);
      EXPECT_TRUE(::CloseHandle(subkey_handle_));
    }
  }

 protected:
  ScopedTempRegistryKey temp_key_;
  HANDLE subkey_handle_;
  WStringEmbeddedNulls full_key_path_;
  WStringEmbeddedNulls value_name_{L'f', L'o', L'o', L'\0'};
  WStringEmbeddedNulls value_{L'b', L'a', L'r', L'\0'};
  WStringEmbeddedNulls valid_changed_value_{L'b', L'a', L'\0'};
  ShouldNormalizeRegistryValue default_value_should_be_normalized_;
};

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryKey_Success) {
  EXPECT_TRUE(SandboxNtDeleteRegistryKey(full_key_path_));

  HANDLE deleted_key_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(
      STATUS_OBJECT_NAME_NOT_FOUND,
      NativeOpenKey(nullptr, full_key_path_, KEY_READ, &deleted_key_handle));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryKey_NullKey) {
  EXPECT_FALSE(SandboxNtDeleteRegistryKey(WStringEmbeddedNulls(nullptr)));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryKey_LongKey) {
  WStringEmbeddedNulls long_key = VeryLongStringWithPrefix(full_key_path_);
  EXPECT_FALSE(SandboxNtDeleteRegistryKey(long_key));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryKey_KeyMissingTerminator) {
  WStringEmbeddedNulls no_terminating_null_key(
      full_key_path_.CastAsWCharArray(), full_key_path_.size() - 1);
  EXPECT_FALSE(SandboxNtDeleteRegistryKey(no_terminating_null_key));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryKey_NonRegistryPath) {
  EXPECT_FALSE(SandboxNtDeleteRegistryKey(
      StringWithTrailingNull(kDirectNonRegistryPath)));
  EXPECT_FALSE(SandboxNtDeleteRegistryKey(
      StringWithTrailingNull(kTrickyNonRegistryPath)));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryValue_Success) {
  EXPECT_TRUE(SandboxNtDeleteRegistryValue(full_key_path_, value_name_));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryValue_NullKey) {
  EXPECT_FALSE(
      SandboxNtDeleteRegistryValue(WStringEmbeddedNulls(nullptr), value_name_));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryValue_NullValue) {
  EXPECT_FALSE(SandboxNtDeleteRegistryValue(full_key_path_,
                                            WStringEmbeddedNulls(nullptr)));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryValue_LongKey) {
  WStringEmbeddedNulls long_key = VeryLongStringWithPrefix(full_key_path_);
  EXPECT_FALSE(SandboxNtDeleteRegistryValue(long_key, value_name_));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryValue_LongValue) {
  WStringEmbeddedNulls very_long_name = VeryLongStringWithPrefix(value_name_);
  EXPECT_FALSE(SandboxNtDeleteRegistryValue(full_key_path_, very_long_name));
}

TEST_F(CleanerInterfaceRegistryTest,
       NtDeleteRegistryValue_KeyMissingTerminator) {
  WStringEmbeddedNulls no_terminating_null_key(
      full_key_path_.CastAsWCharArray(), full_key_path_.size() - 1);
  EXPECT_FALSE(
      SandboxNtDeleteRegistryValue(no_terminating_null_key, value_name_));
}

TEST_F(CleanerInterfaceRegistryTest,
       NtDeleteRegistryValue_ValueMissingTerminator) {
  WStringEmbeddedNulls no_terminating_null_name(value_name_.CastAsWCharArray(),
                                                value_name_.size() - 1);
  EXPECT_FALSE(
      SandboxNtDeleteRegistryValue(full_key_path_, no_terminating_null_name));
}

TEST_F(CleanerInterfaceRegistryTest,
       NtDeleteRegistryValue_ValueNameHasEmbeddedNull) {
  WStringEmbeddedNulls value_name{L'f', L'o', L'\0', L'o', L'\0'};

  EXPECT_EQ(STATUS_SUCCESS,
            NativeSetValueKey(subkey_handle_, value_name, REG_SZ, value_));

  EXPECT_TRUE(SandboxNtDeleteRegistryValue(full_key_path_, value_name));

  EXPECT_FALSE(
      NativeQueryValueKey(subkey_handle_, value_name, nullptr, nullptr));
}

TEST_F(CleanerInterfaceRegistryTest, NtDeleteRegistryValue_NonRegistryPath) {
  EXPECT_FALSE(SandboxNtDeleteRegistryValue(
      StringWithTrailingNull(kDirectNonRegistryPath), value_name_));
  EXPECT_FALSE(SandboxNtDeleteRegistryValue(
      StringWithTrailingNull(kTrickyNonRegistryPath), value_name_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_Success) {
  EXPECT_TRUE(SandboxNtChangeRegistryValue(
      full_key_path_, value_name_, valid_changed_value_,
      default_value_should_be_normalized_));

  DWORD type = 0;
  WStringEmbeddedNulls actual_value;
  EXPECT_TRUE(
      NativeQueryValueKey(subkey_handle_, value_name_, &type, &actual_value));
  EXPECT_EQ(REG_SZ, type);
  EXPECT_EQ(valid_changed_value_, actual_value);
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_NullKey) {
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      WStringEmbeddedNulls(nullptr), value_name_, valid_changed_value_,
      default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_NullValue) {
  EXPECT_TRUE(SandboxNtChangeRegistryValue(
      full_key_path_, value_name_, WStringEmbeddedNulls(nullptr),
      default_value_should_be_normalized_));

  WStringEmbeddedNulls actual_value;
  EXPECT_TRUE(
      NativeQueryValueKey(subkey_handle_, value_name_, nullptr, &actual_value));
  EXPECT_EQ(0U, actual_value.size());
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_LongKey) {
  WStringEmbeddedNulls long_key = VeryLongStringWithPrefix(full_key_path_);
  EXPECT_FALSE(
      SandboxNtChangeRegistryValue(long_key, value_name_, valid_changed_value_,
                                   default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_LongValueName) {
  WStringEmbeddedNulls very_long_name = VeryLongStringWithPrefix(value_name_);
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      full_key_path_, very_long_name, valid_changed_value_,
      default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_LongValue) {
  WStringEmbeddedNulls very_long_value = VeryLongStringWithPrefix(value_);
  EXPECT_FALSE(
      SandboxNtChangeRegistryValue(full_key_path_, value_name_, very_long_value,
                                   default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest,
       NtChangeRegistryValue_KeyMissingTerminator) {
  WStringEmbeddedNulls no_terminating_null_key(
      full_key_path_.CastAsWCharArray(), full_key_path_.size() - 1);
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      no_terminating_null_key, value_name_, valid_changed_value_,
      default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest,
       NtChangeRegistryValue_ValueNameMissingTerminator) {
  WStringEmbeddedNulls no_terminating_null_name(value_name_.CastAsWCharArray(),
                                                value_name_.size() - 1);
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      full_key_path_, no_terminating_null_name, valid_changed_value_,
      default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_NoNullTerminator) {
  WStringEmbeddedNulls no_terminating_null_value(
      valid_changed_value_.CastAsWCharArray(), valid_changed_value_.size());
  EXPECT_TRUE(SandboxNtChangeRegistryValue(
      full_key_path_, value_name_, no_terminating_null_value,
      default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_MissingValue) {
  WStringEmbeddedNulls absent_value_name{L'f', L'o', L'\0', L'o', L'\0'};

  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      full_key_path_, absent_value_name, valid_changed_value_,
      default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_OtherValidType) {
  WStringEmbeddedNulls reference_value_name{L'f', L'o', L'\0', L'o', L'\0'};

  EXPECT_EQ(STATUS_SUCCESS,
            NativeSetValueKey(subkey_handle_, reference_value_name,
                              REG_EXPAND_SZ, value_));

  EXPECT_TRUE(SandboxNtChangeRegistryValue(
      full_key_path_, reference_value_name, valid_changed_value_,
      default_value_should_be_normalized_));

  DWORD type = 0;
  EXPECT_TRUE(NativeQueryValueKey(subkey_handle_, reference_value_name, &type,
                                  nullptr));
  EXPECT_EQ(REG_EXPAND_SZ, type);
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_InvalidType) {
  WStringEmbeddedNulls value_name{L'f', L'o', L'\0', L'o', L'\0'};

  EXPECT_EQ(STATUS_SUCCESS,
            NativeSetValueKey(subkey_handle_, value_name, REG_BINARY, value_));

  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      full_key_path_, value_name, valid_changed_value_,
      default_value_should_be_normalized_));

  DWORD type = 0;
  EXPECT_TRUE(NativeQueryValueKey(subkey_handle_, value_name, &type, nullptr));
  EXPECT_EQ(REG_BINARY, type);
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_NullName) {
  EXPECT_EQ(STATUS_SUCCESS,
            NativeSetValueKey(subkey_handle_, WStringEmbeddedNulls(nullptr),
                              REG_SZ, value_));

  EXPECT_TRUE(SandboxNtChangeRegistryValue(
      full_key_path_, WStringEmbeddedNulls(nullptr), valid_changed_value_,
      default_value_should_be_normalized_));

  DWORD type = 0;
  WStringEmbeddedNulls actual_value;
  EXPECT_TRUE(NativeQueryValueKey(subkey_handle_, WStringEmbeddedNulls(nullptr),
                                  &type, &actual_value));
  EXPECT_EQ(REG_SZ, type);
  EXPECT_EQ(valid_changed_value_, actual_value);
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_NonRegistryPath) {
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      StringWithTrailingNull(kDirectNonRegistryPath), value_name_,
      valid_changed_value_, default_value_should_be_normalized_));
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      StringWithTrailingNull(kTrickyNonRegistryPath), value_name_,
      valid_changed_value_, default_value_should_be_normalized_));
}

TEST_F(CleanerInterfaceRegistryTest, NtChangeRegistryValue_AllowNormalization) {
  WStringEmbeddedNulls value = StringWithTrailingNull(L"f o q b,ab");
  EXPECT_EQ(STATUS_SUCCESS,
            NativeSetValueKey(subkey_handle_, value_name_, REG_SZ, value));

  WStringEmbeddedNulls normalized_value = StringWithTrailingNull(L"f,o,q,b,ab");
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      full_key_path_, value_name_, normalized_value,
      default_value_should_be_normalized_));

  WStringEmbeddedNulls normalized_shortened_value =
      StringWithTrailingNull(L"f,o,b,ab");
  EXPECT_FALSE(SandboxNtChangeRegistryValue(
      full_key_path_, value_name_, normalized_shortened_value,
      default_value_should_be_normalized_));

  // Switch to allow every value to be normalized.
  ShouldNormalizeRegistryValue normalize_all_values =
      base::BindRepeating([](const WStringEmbeddedNulls&,
                             const WStringEmbeddedNulls&) { return true; });
  EXPECT_TRUE(SandboxNtChangeRegistryValue(
      full_key_path_, value_name_, normalized_value, normalize_all_values));

  EXPECT_TRUE(SandboxNtChangeRegistryValue(full_key_path_, value_name_,
                                           normalized_shortened_value,
                                           normalize_all_values));
}

class CleanerSandboxInterfaceRunningServiceTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    // Tests calling StartService() need this.
    ASSERT_TRUE(chrome_cleaner::ResetAclForUcrtbase());
  }
};

TEST(CleanerSandboxInterface, DeleteService_NotExisting) {
  EXPECT_TRUE(SandboxDeleteService(
      chrome_cleaner::RandomUnusedServiceNameForTesting().c_str()));
}

TEST(CleanerSandboxInterface, DeleteService_Success) {
  ASSERT_TRUE(chrome_cleaner::EnsureNoTestServicesRunning());

  chrome_cleaner::TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  service_handle.Close();

  EXPECT_TRUE(SandboxDeleteService(service_handle.service_name()));

  EXPECT_FALSE(chrome_cleaner::DoesServiceExist(service_handle.service_name()));
}

// TODO(crbug.com/1061171): Test is flaky.
TEST_F(CleanerSandboxInterfaceRunningServiceTest, DISABLED_DeleteService_Running) {
  ASSERT_TRUE(chrome_cleaner::EnsureNoTestServicesRunning());

  chrome_cleaner::TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  ASSERT_TRUE(service_handle.StartService());
  service_handle.Close();

  EXPECT_TRUE(SandboxDeleteService(service_handle.service_name()));

  EXPECT_FALSE(chrome_cleaner::DoesServiceExist(service_handle.service_name()));
}

// TODO(crbug.com/1061171): Test is flaky.
TEST_F(CleanerSandboxInterfaceRunningServiceTest, DISABLED_DeleteService_HandleHeld) {
  ASSERT_TRUE(chrome_cleaner::EnsureNoTestServicesRunning());

  chrome_cleaner::TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  ASSERT_TRUE(service_handle.StartService());

  // SandboxDeleteService should succeed because even though there is still a
  // handle to the service, it has been scheduled for deletion.
  EXPECT_TRUE(SandboxDeleteService(service_handle.service_name()));

  // Make sure that after the handle is closed the service is deleted.
  // Note: Before this handle is closed, OpenService may or may not provide
  // a valid handle (pre Win10 1703 a valid handle is returned), so
  // DoesServiceExists will provide different results.
  service_handle.Close();
  EXPECT_TRUE(
      chrome_cleaner::WaitForServiceDeleted(service_handle.service_name()));
  EXPECT_FALSE(chrome_cleaner::DoesServiceExist(service_handle.service_name()));
}

class CleanerSandboxInterface_WithTaskScheduler : public ::testing::Test {
 public:
  void SetUp() override {
    chrome_cleaner::TaskScheduler::SetMockDelegateForTesting(
        &test_task_scheduler_);
  }

  void TearDown() override {
    chrome_cleaner::TaskScheduler::SetMockDelegateForTesting(nullptr);
  }

 protected:
  chrome_cleaner::TestTaskScheduler test_task_scheduler_;
};

TEST_F(CleanerSandboxInterface_WithTaskScheduler, DeleteTask_Existing) {
  chrome_cleaner::TaskScheduler::TaskInfo task_info;
  ASSERT_TRUE(RegisterTestTask(&test_task_scheduler_, &task_info));

  EXPECT_TRUE(SandboxDeleteTask(task_info.name.c_str()));
}

TEST_F(CleanerSandboxInterface_WithTaskScheduler, DeleteTask_Missing) {
  EXPECT_TRUE(SandboxDeleteTask(L"NonExistentTaskName"));
}

TEST(CleanerSandboxInterface, TerminateProcessTest) {
  // Note that this test will fail under the debugger since the debugged test
  // process will inherit the SeDebugPrivilege which allows the test to get
  // an ALL_ACCESS handle.
  if (::IsDebuggerPresent()) {
    LOG(ERROR) << "TerminateProcessTest skipped when running in debugger.";
    return;
  }

  base::Process test_process =
      chrome_cleaner::LongRunningProcess(/*command_line=*/nullptr);
  ASSERT_TRUE(test_process.IsValid());

  // Set up the process protector.
  chrome_cleaner::ScopedProcessProtector process_protector(test_process.Pid());
  EXPECT_TRUE(process_protector.Initialized());

  // We should no longer be able to kill it.
  EXPECT_EQ(SandboxTerminateProcess(test_process.Pid()),
            TerminateProcessResult::kFailed);

  // Double check the process is still around.
  DWORD exit_code = 420042;
  EXPECT_EQ(TRUE, ::GetExitCodeProcess(test_process.Handle(), &exit_code));
  EXPECT_EQ(STILL_ACTIVE, exit_code);

  // Unprotect the process and kill it.
  process_protector.Release();
  EXPECT_EQ(SandboxTerminateProcess(test_process.Pid()),
            TerminateProcessResult::kSuccess);

  // Check the process actually exits.
  int killed_process_exit_code = 0;
  test_process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                      &killed_process_exit_code);
  EXPECT_EQ(1, killed_process_exit_code);

  if (killed_process_exit_code != 1) {
    // Clean up just in case.
    test_process.Terminate(2, false);
  }
}

TEST(CleanerSandboxInterface, TerminateProcessTest_ChromeProcess) {
  base::CommandLine test_process_cmd(base::CommandLine::NO_PROGRAM);
  base::Process test_process =
      chrome_cleaner::LongRunningProcess(&test_process_cmd);
  ASSERT_TRUE(test_process.IsValid());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine original_command_line = *command_line;

  command_line->AppendSwitchPath(chrome_cleaner::kChromeExePathSwitch,
                                 test_process_cmd.GetProgram());

  EXPECT_EQ(SandboxTerminateProcess(test_process.Pid()),
            TerminateProcessResult::kDenied);

  // Make sure the process is actually still running.
  DWORD exit_code = 4711;
  EXPECT_EQ(TRUE, ::GetExitCodeProcess(test_process.Handle(), &exit_code));
  EXPECT_EQ(STILL_ACTIVE, exit_code);

  test_process.Terminate(0, false);
  *command_line = original_command_line;
}

TEST(CleanerSandboxInterface, TerminateProcessTest_RestrictedProcesses) {
  EXPECT_EQ(SandboxTerminateProcess(::GetCurrentProcessId()),
            TerminateProcessResult::kDenied);

  // 0 is System Idle Process and we shouldn't be able to terminate it.
  EXPECT_EQ(SandboxTerminateProcess(0), TerminateProcessResult::kFailed);
}

}  // namespace chrome_cleaner_sandbox
