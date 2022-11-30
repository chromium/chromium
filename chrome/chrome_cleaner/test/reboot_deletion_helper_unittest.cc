// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/reboot_deletion_helper.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

// The moves-pending-reboot is a MULTISZ registry value in the HKLM part of the
// registry.
const wchar_t kSessionManagerKey[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager";
const wchar_t kPendingFileRenameOps[] = L"PendingFileRenameOperations";

const uint32_t kMaxRegistryValueLength = 1024;

const wchar_t kRemoveFile1[] = L"remove_one";
const wchar_t kRemoveFile2[] = L"remove_two";
const wchar_t kRemoveFile3[] = L"remove_three";
const wchar_t kRemoveFile4[] = L"remove_four";
const wchar_t kRemoveFile5[] = L"remove_five";
const wchar_t kDeleteFile[] = L"";
const BYTE kNoNullInvalidString[] = {12, 13};
const BYTE kMissingNullEntryString[] = {65, 0, 0, 0};
const BYTE kOddSizeEntryString[] = {0, 65, 0, 66, 0xFF, 0, 0, 0, 0};
const wchar_t kRemoveRawMultipleFiles[] =
    L"remove_one\0\0remove_two\0remove_three\0\0\0";
const wchar_t kRemoveRawEmpty0[] = L"";
const wchar_t kRemoveRawEmpty1[] = L"\0";
const wchar_t kRemoveRawEmpty2[] = L"\0\0";
const wchar_t kRemoveRawCorrupt[] = L"file_with_no_ending_null";

bool TestPendingFileRenameOperations(const wchar_t* content,
                                     size_t content_size,
                                     PendingMoveVector* pending_moves) {
  DCHECK(content);
  DCHECK(pending_moves);
  pending_moves->clear();

  base::win::RegKey session_manager_key(HKEY_LOCAL_MACHINE, kSessionManagerKey,
                                        KEY_ALL_ACCESS);
  if (!session_manager_key.Handle()) {
    PLOG(ERROR) << "Can't open session manager.";
    return false;
  }

  // Write raw content of the registry value.
  if (session_manager_key.WriteValue(kPendingFileRenameOps, content,
                                     content_size,
                                     REG_MULTI_SZ) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Can't write to registry value '" << kPendingFileRenameOps
                << "' value: '" << content << "'.";
    return false;
  }

  // Read back the pending moves.
  if (!GetPendingMoves(pending_moves)) {
    PLOG(ERROR) << "Can't read back or parse the registry value '"
                << kPendingFileRenameOps << "'.";
    return false;
  }

  return true;
}

}  // namespace

TEST(RebootDeletionHelper, GetPendingMovesRawValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  PendingMoveVector pending_moves;
  EXPECT_TRUE(
      TestPendingFileRenameOperations(kRemoveRawEmpty0, 0, &pending_moves));
  EXPECT_TRUE(pending_moves.empty());

  EXPECT_TRUE(TestPendingFileRenameOperations(
      kRemoveRawEmpty0, sizeof(kRemoveRawEmpty0), &pending_moves));
  EXPECT_TRUE(pending_moves.empty());

  EXPECT_TRUE(TestPendingFileRenameOperations(
      kRemoveRawEmpty1, sizeof(kRemoveRawEmpty1), &pending_moves));
  EXPECT_TRUE(pending_moves.empty());

  EXPECT_TRUE(TestPendingFileRenameOperations(
      kRemoveRawEmpty2, sizeof(kRemoveRawEmpty2), &pending_moves));
  EXPECT_TRUE(pending_moves.empty());

  EXPECT_TRUE(TestPendingFileRenameOperations(kRemoveRawMultipleFiles,
                                              sizeof(kRemoveRawMultipleFiles),
                                              &pending_moves));
  EXPECT_THAT(pending_moves,
              testing::ElementsAre(std::make_pair(kRemoveFile1, kDeleteFile),
                                   std::make_pair(kRemoveFile2, kRemoveFile3)));

  const wchar_t kRemoveRawSingleFile[] = L"remove_one\0\0";
  EXPECT_TRUE(TestPendingFileRenameOperations(
      kRemoveRawSingleFile, sizeof(kRemoveRawSingleFile), &pending_moves));
  EXPECT_THAT(pending_moves,
              testing::ElementsAre(std::make_pair(kRemoveFile1, kDeleteFile)));

  const wchar_t kRemoveRawRename0[] = L"remove_one\0remove_two";
  EXPECT_TRUE(TestPendingFileRenameOperations(
      kRemoveRawRename0, sizeof(kRemoveRawRename0), &pending_moves));
  EXPECT_THAT(pending_moves,
              testing::ElementsAre(std::make_pair(kRemoveFile1, kRemoveFile2)));

  const wchar_t kRemoveRawRename1[] = L"remove_one\0remove_two\0";
  EXPECT_TRUE(TestPendingFileRenameOperations(
      kRemoveRawRename1, sizeof(kRemoveRawRename1), &pending_moves));
  EXPECT_THAT(pending_moves,
              testing::ElementsAre(std::make_pair(kRemoveFile1, kRemoveFile2)));

  const wchar_t kRemoveRawRename2[] = L"remove_one\0remove_two\0\0";
  EXPECT_TRUE(TestPendingFileRenameOperations(
      kRemoveRawRename2, sizeof(kRemoveRawRename2), &pending_moves));
  EXPECT_THAT(pending_moves,
              testing::ElementsAre(std::make_pair(kRemoveFile1, kRemoveFile2)));

  EXPECT_FALSE(TestPendingFileRenameOperations(
      kRemoveRawCorrupt, sizeof(kRemoveRawCorrupt), &pending_moves));
}

TEST(RebootDeletionHelper, SetPendingMovesRawValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey session_manager_key(HKEY_LOCAL_MACHINE, kSessionManagerKey,
                                        KEY_ALL_ACCESS);
  EXPECT_TRUE(session_manager_key.Handle());

  // Write some pending moves.
  PendingMoveVector pending_moves;
  pending_moves.push_back(std::make_pair(kRemoveFile1, kDeleteFile));
  pending_moves.push_back(std::make_pair(kRemoveFile2, kRemoveFile3));

  EXPECT_TRUE(SetPendingMoves(pending_moves));

  wchar_t buffer[kMaxRegistryValueLength];
  DWORD buffer_size = kMaxRegistryValueLength;
  DWORD buffer_type = REG_NONE;

  // Validate the raw content of the registry value.
  EXPECT_EQ(ERROR_SUCCESS,
            session_manager_key.ReadValue(kPendingFileRenameOps, &buffer[0],
                                          &buffer_size, &buffer_type));

  EXPECT_EQ(buffer_size, sizeof(kRemoveRawMultipleFiles));
  EXPECT_EQ(0, ::memcmp(kRemoveRawMultipleFiles, buffer, buffer_size));
  EXPECT_EQ(REG_MULTI_SZ, buffer_type);
}

TEST(RebootDeletionHelper, SetPendingMovesValueWithEmptyInput) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey session_manager_key(HKEY_LOCAL_MACHINE, kSessionManagerKey,
                                        KEY_ALL_ACCESS);
  EXPECT_TRUE(session_manager_key.Handle());

  // Write raw content of the registry value.
  EXPECT_EQ(ERROR_SUCCESS, session_manager_key.WriteValue(
                               kPendingFileRenameOps, kRemoveRawMultipleFiles,
                               sizeof(kRemoveRawMultipleFiles), REG_MULTI_SZ));

  // Write an empty pending moves vector.
  PendingMoveVector pending_moves;
  SetPendingMoves(pending_moves);

  // The registry value must be removed.
  EXPECT_FALSE(session_manager_key.HasValue(kPendingFileRenameOps));
}

TEST(RebootDeletionHelper, GetAndSetPendingMoves) {
  // Declare the scoped temp dir before the registry override manager because
  // the recursive deletion of folders fail when running elevated while a
  // registry override is active.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Expect the registry key to be empty.
  PendingMoveVector pending_moves;
  EXPECT_TRUE(GetPendingMoves(&pending_moves));
  ASSERT_TRUE(pending_moves.empty());

  // Write back some moves into the registry key.
  base::FilePath file_path1 = temp.GetPath().Append(kRemoveFile1);
  base::FilePath file_path2 = temp.GetPath().Append(kRemoveFile2);
  base::FilePath file_path3 = temp.GetPath().Append(kRemoveFile3);

  pending_moves.push_back(
      std::make_pair(file_path1.value(), file_path2.value()));
  pending_moves.push_back(std::make_pair(file_path3.value(), kDeleteFile));

  EXPECT_TRUE(SetPendingMoves(pending_moves));

  // Read back the written moves.
  PendingMoveVector written_moves;
  EXPECT_TRUE(GetPendingMoves(&written_moves));

  // Validate that moves before and after serialisation are the same.
  EXPECT_THAT(written_moves, testing::ElementsAreArray(pending_moves));

  // Write back an empty set of moves.
  pending_moves.clear();
  EXPECT_TRUE(SetPendingMoves(pending_moves));
  EXPECT_TRUE(GetPendingMoves(&written_moves));
  EXPECT_TRUE(written_moves.empty());
}

TEST(RebootDeletionHelper, GetInvalidPendingMoves) {
  // Open the pending registry key to write into invalid data.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey session_manager_key(HKEY_LOCAL_MACHINE, kSessionManagerKey,
                                        KEY_CREATE_SUB_KEY | KEY_SET_VALUE);
  EXPECT_TRUE(session_manager_key.Handle());

  // Write an invalid pending moves registry key with a wrong type.
  EXPECT_EQ(ERROR_SUCCESS,
            session_manager_key.WriteValue(kPendingFileRenameOps, 0xCCCCCCCC));
  PendingMoveVector pending_moves;
  EXPECT_FALSE(GetPendingMoves(&pending_moves));

  // Write an invalid pending moves registry key with an invalid string format.
  EXPECT_EQ(ERROR_SUCCESS, session_manager_key.WriteValue(
                               kPendingFileRenameOps, kNoNullInvalidString,
                               sizeof(kNoNullInvalidString), REG_MULTI_SZ));
  EXPECT_FALSE(GetPendingMoves(&pending_moves));

  // Write an invalid string without the ending empty entry.
  EXPECT_EQ(ERROR_SUCCESS, session_manager_key.WriteValue(
                               kPendingFileRenameOps, kMissingNullEntryString,
                               sizeof(kMissingNullEntryString), REG_MULTI_SZ));
  EXPECT_FALSE(GetPendingMoves(&pending_moves));

  // Write an invalid string with an odd number of bytes.
  EXPECT_EQ(ERROR_SUCCESS, session_manager_key.WriteValue(
                               kPendingFileRenameOps, kOddSizeEntryString,
                               sizeof(kOddSizeEntryString), REG_MULTI_SZ));
  EXPECT_TRUE(GetPendingMoves(&pending_moves));
}

TEST(RebootDeletionHelper, UnregisterPostRebootRemovals) {
  // Declare the scoped temp dir before the registry override manager because
  // the recursive deletion of folders fail when running elevated while a
  // registry override is active.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Write some moves and renamings into the registry key.
  base::FilePath file_path1 = temp.GetPath().Append(kRemoveFile1);
  base::FilePath file_path2 = temp.GetPath().Append(kRemoveFile2);
  base::FilePath file_path3 = temp.GetPath().Append(kRemoveFile3);
  base::FilePath file_path4 = temp.GetPath().Append(kRemoveFile4);
  base::FilePath file_path5 = temp.GetPath().Append(kRemoveFile5);

  PendingMoveVector pending_moves;
  pending_moves.push_back(std::make_pair(file_path1.value(), kDeleteFile));
  pending_moves.push_back(std::make_pair(file_path2.value(), kDeleteFile));
  pending_moves.push_back(std::make_pair(file_path3.value(), kDeleteFile));
  pending_moves.push_back(
      std::make_pair(file_path4.value(), file_path5.value()));

  EXPECT_TRUE(SetPendingMoves(pending_moves));

  // Unregister paths from the pending moves.
  FilePathSet paths;
  paths.Insert(file_path2);
  paths.Insert(file_path3);
  // A file renaming action must not be removed, even if a unregister is
  // requested.
  paths.Insert(file_path4);
  EXPECT_TRUE(UnregisterPostRebootRemovals(paths));

  // Read back the pending moves.
  PendingMoveVector written_moves;
  EXPECT_TRUE(GetPendingMoves(&written_moves));
  EXPECT_THAT(written_moves,
              testing::ElementsAre(
                  std::make_pair(file_path1.value(), kDeleteFile),
                  std::make_pair(file_path4.value(), file_path5.value())));
}

TEST(RebootDeletionHelper, UnregisterPostRebootRemovalsOfQualifiedPath) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a pending deletion with a qualified path.
  PendingMoveVector pending_moves;
  pending_moves.push_back(
      std::make_pair(L"\\??\\C:\\this_file_doesnt_exist", kDeleteFile));
  EXPECT_TRUE(SetPendingMoves(pending_moves));

  // Try to remove it with an unqualified path.
  FilePathSet paths;
  paths.Insert(base::FilePath(FILE_PATH_LITERAL("C:\\this_file_doesnt_exist")));
  EXPECT_TRUE(UnregisterPostRebootRemovals(paths));

  // Read back the pending moves.
  PendingMoveVector written_moves;
  EXPECT_TRUE(GetPendingMoves(&written_moves));
  EXPECT_TRUE(written_moves.empty());
}

}  // namespace chrome_cleaner
