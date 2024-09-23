// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/base_file.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace download {
namespace {

const char kTestData1[] = "Let's write some data to the file!\n";
const char kTestData2[] = "Writing more data.\n";
const char kTestData3[] = "Final line.";
const char kTestData4[] = "supercalifragilisticexpialidocious";
const int kTestDataLength1 = std::size(kTestData1) - 1;
const int kTestDataLength2 = std::size(kTestData2) - 1;
const int kTestDataLength3 = std::size(kTestData3) - 1;
const int kTestDataLength4 = std::size(kTestData4) - 1;
int64_t kTestDataBytesWasted = 0;

// SHA-256 hash of kTestData1 (excluding terminating NUL).
const uint8_t kHashOfTestData1[] = {
    0x0b, 0x2d, 0x3f, 0x3f, 0x79, 0x43, 0xad, 0x64, 0xb8, 0x60, 0xdf,
    0x94, 0xd0, 0x5c, 0xb5, 0x6a, 0x8a, 0x97, 0xc6, 0xec, 0x57, 0x68,
    0xb5, 0xb7, 0x0b, 0x93, 0x0c, 0x5a, 0xa7, 0xfa, 0x9a, 0xde};

// SHA-256 hash of kTestData1 ++ kTestData2 ++ kTestData3 (excluding terminating
// NUL).
const uint8_t kHashOfTestData1To3[] = {
    0xcb, 0xf6, 0x8b, 0xf1, 0x0f, 0x80, 0x03, 0xdb, 0x86, 0xb3, 0x13,
    0x43, 0xaf, 0xac, 0x8c, 0x71, 0x75, 0xbd, 0x03, 0xfb, 0x5f, 0xc9,
    0x05, 0x65, 0x0f, 0x8c, 0x80, 0xaf, 0x08, 0x74, 0x43, 0xa8};

}  // namespace

class BaseFileTest : public testing::Test {
 public:
  static const unsigned char kEmptySha256Hash[crypto::kSHA256Length];

  BaseFileTest()
      : expect_file_survives_(false),
        expect_in_progress_(true),
        expected_error_(DOWNLOAD_INTERRUPT_REASON_NONE) {}

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    ASSERT_TRUE(com_initializer_.Succeeded());
#endif
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base_file_ = std::make_unique<BaseFile>(DownloadItem::kInvalidId);
  }

  void TearDown() override {
    EXPECT_FALSE(base_file_->in_progress());
    if (!expected_error_) {
      EXPECT_EQ(static_cast<int64_t>(expected_data_.size()),
                base_file_->bytes_so_far());
    }

    base::FilePath full_path = base_file_->full_path();

    if (!expected_data_.empty() && !expected_error_) {
      // Make sure the data has been properly written to disk.
      std::string disk_data;
      EXPECT_TRUE(base::ReadFileToString(full_path, &disk_data));
      EXPECT_EQ(expected_data_, disk_data);
    }

    base_file_.reset();

    EXPECT_EQ(expect_file_survives_, base::PathExists(full_path));
  }

  bool InitializeFile() {
    DownloadInterruptReason result = base_file_->Initialize(
        base::FilePath(), temp_dir_.GetPath(), base::File(), 0, std::string(),
        std::unique_ptr<crypto::SecureHash>(), false, &kTestDataBytesWasted);
    EXPECT_EQ(expected_error_, result);
    return result == DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  bool AppendDataToFile(const std::string& data) {
    EXPECT_EQ(expect_in_progress_, base_file_->in_progress());
    DownloadInterruptReason result =
        base_file_->AppendDataToFile(data.data(), data.size());
    if (result == DOWNLOAD_INTERRUPT_REASON_NONE)
      EXPECT_TRUE(expect_in_progress_) << " result = " << result;

    EXPECT_EQ(expected_error_, result);
    if (base_file_->in_progress()) {
      expected_data_ += data;
      if (expected_error_ == DOWNLOAD_INTERRUPT_REASON_NONE) {
        EXPECT_EQ(static_cast<int64_t>(expected_data_.size()),
                  base_file_->bytes_so_far());
      }
    }
    return result == DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  void set_expected_data(const std::string& data) { expected_data_ = data; }

  // Helper functions.
  // Create a file.  Returns the complete file path.
  base::FilePath CreateTestFile() {
    base::FilePath file_name;
    BaseFile file(DownloadItem::kInvalidId);

    EXPECT_EQ(
        DOWNLOAD_INTERRUPT_REASON_NONE,
        file.Initialize(base::FilePath(), temp_dir_.GetPath(), base::File(), 0,
                        std::string(), std::unique_ptr<crypto::SecureHash>(),
                        false, &kTestDataBytesWasted));
    file_name = file.full_path();
    EXPECT_NE(base::FilePath::StringType(), file_name.value());

    EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
              file.AppendDataToFile(kTestData4, kTestDataLength4));

    // Keep the file from getting deleted when existing_file_name is deleted.
    file.Detach();

    return file_name;
  }

  // Create a file with the specified file name.
  void CreateFileWithName(const base::FilePath& file_name) {
    EXPECT_NE(base::FilePath::StringType(), file_name.value());
    BaseFile duplicate_file(download::DownloadItem::kInvalidId);
    DownloadInterruptReason reason = duplicate_file.Initialize(
        file_name, temp_dir_.GetPath(), base::File(), 0, std::string(),
        std::unique_ptr<crypto::SecureHash>(), false, &kTestDataBytesWasted);
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(reason, DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
#else
    EXPECT_EQ(reason, DOWNLOAD_INTERRUPT_REASON_NONE);
    // Write something into it.
    duplicate_file.AppendDataToFile(kTestData4, kTestDataLength4);
#endif  // BUILDFLAG(IS_WIN)

    // Detach the file so it isn't deleted on destruction of |duplicate_file|.
    duplicate_file.Detach();
  }

  int64_t CurrentSpeedAtTime(base::TimeTicks current_time) {
    EXPECT_TRUE(base_file_.get());
    return base_file_->CurrentSpeedAtTime(current_time);
  }

  base::TimeTicks StartTick() {
    EXPECT_TRUE(base_file_.get());
    return base_file_->start_tick_;
  }

  void set_expected_error(DownloadInterruptReason err) {
    expected_error_ = err;
  }

  void ExpectPermissionError(DownloadInterruptReason err) {
    EXPECT_TRUE(err == DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR ||
                err == DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED)
        << "Interrupt reason = " << err;
  }

  template <size_t SZ>
  static void ExpectHashValue(const uint8_t (&expected_hash)[SZ],
                              std::unique_ptr<crypto::SecureHash> hash_state) {
    std::vector<uint8_t> hash_value(hash_state->GetHashLength());
    hash_state->Finish(&hash_value.front(), hash_value.size());
    ASSERT_EQ(SZ, hash_value.size());
    EXPECT_EQ(0, memcmp(expected_hash, &hash_value.front(), hash_value.size()));
  }

 private:
#if BUILDFLAG(IS_WIN)
  // This must occur early in the member list to ensure COM is initialized first
  // and uninitialized last.
  base::win::ScopedCOMInitializer com_initializer_;
#endif

 protected:
  // BaseClass instance we are testing.
  std::unique_ptr<BaseFile> base_file_;

  // Temporary directory for renamed downloads.
  base::ScopedTempDir temp_dir_;

  // Expect the file to survive deletion of the BaseFile instance.
  bool expect_file_survives_;

  // Expect the file to be in progress.
  bool expect_in_progress_;

 private:
  // Keep track of what data should be saved to the disk file.
  std::string expected_data_;
  DownloadInterruptReason expected_error_;
};

// This will initialize the entire array to zero.
const unsigned char BaseFileTest::kEmptySha256Hash[] = {0};

// Test the most basic scenario: just create the object and do a sanity check
// on all its accessors. This is actually a case that rarely happens
// in production, where we would at least Initialize it.
TEST_F(BaseFileTest, CreateDestroy) {
  EXPECT_EQ(base::FilePath().value(), base_file_->full_path().value());
}

// Cancel the download explicitly.
TEST_F(BaseFileTest, Cancel) {
  ASSERT_TRUE(InitializeFile());
  EXPECT_TRUE(base::PathExists(base_file_->full_path()));
  base_file_->Cancel();
  EXPECT_FALSE(base::PathExists(base_file_->full_path()));
  EXPECT_NE(base::FilePath().value(), base_file_->full_path().value());
}

// Write data to the file and detach it, so it doesn't get deleted
// automatically when base_file_ is destructed.
TEST_F(BaseFileTest, WriteAndDetach) {
  ASSERT_TRUE(InitializeFile());
  ASSERT_TRUE(AppendDataToFile(kTestData1));
  base_file_->Finish();
  base_file_->Detach();
  expect_file_survives_ = true;
}

// Write data to the file and detach it, and calculate its sha256 hash.
TEST_F(BaseFileTest, WriteWithHashAndDetach) {
  ASSERT_TRUE(InitializeFile());
  ASSERT_TRUE(AppendDataToFile(kTestData1));
  ExpectHashValue(kHashOfTestData1, base_file_->Finish());
  base_file_->Detach();
  expect_file_survives_ = true;
}

// Rename the file after writing to it, then detach.
TEST_F(BaseFileTest, WriteThenRenameAndDetach) {
  ASSERT_TRUE(InitializeFile());

  base::FilePath initial_path(base_file_->full_path());
  EXPECT_TRUE(base::PathExists(initial_path));
  base::FilePath new_path(temp_dir_.GetPath().AppendASCII("NewFile"));
  EXPECT_FALSE(base::PathExists(new_path));

  ASSERT_TRUE(AppendDataToFile(kTestData1));

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, base_file_->Rename(new_path));
  EXPECT_FALSE(base::PathExists(initial_path));
  EXPECT_TRUE(base::PathExists(new_path));

  ExpectHashValue(kHashOfTestData1, base_file_->Finish());
  base_file_->Detach();
  expect_file_survives_ = true;
}

// Write data to the file once.
TEST_F(BaseFileTest, SingleWrite) {
  ASSERT_TRUE(InitializeFile());
  ASSERT_TRUE(AppendDataToFile(kTestData1));
  ExpectHashValue(kHashOfTestData1, base_file_->Finish());
}

// Write data to the file multiple times.
TEST_F(BaseFileTest, MultipleWrites) {
  ASSERT_TRUE(InitializeFile());
  ASSERT_TRUE(AppendDataToFile(kTestData1));
  ASSERT_TRUE(AppendDataToFile(kTestData2));
  ASSERT_TRUE(AppendDataToFile(kTestData3));
  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

// Write data to the file multiple times, interrupt it, and continue using
// another file.  Calculate the resulting combined sha256 hash.
TEST_F(BaseFileTest, MultipleWritesInterruptedWithHash) {
  ASSERT_TRUE(InitializeFile());
  // Write some data
  ASSERT_TRUE(AppendDataToFile(kTestData1));
  ASSERT_TRUE(AppendDataToFile(kTestData2));
  // Get the hash state and file name.
  std::unique_ptr<crypto::SecureHash> hash_state = base_file_->Finish();

  base::FilePath new_file_path(temp_dir_.GetPath().Append(
      base::FilePath(FILE_PATH_LITERAL("second_file"))));

  ASSERT_TRUE(base::CopyFile(base_file_->full_path(), new_file_path));

  // Create another file
  BaseFile second_file(download::DownloadItem::kInvalidId);
  ASSERT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            second_file.Initialize(new_file_path, base::FilePath(),
                                   base::File(), base_file_->bytes_so_far(),
                                   std::string(), std::move(hash_state), false,
                                   &kTestDataBytesWasted));
  std::string data(kTestData3);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            second_file.AppendDataToFile(data.data(), data.size()));
  ExpectHashValue(kHashOfTestData1To3, second_file.Finish());
}

// Rename the file after all writes to it.
TEST_F(BaseFileTest, WriteThenRename) {
  ASSERT_TRUE(InitializeFile());

  base::FilePath initial_path(base_file_->full_path());
  EXPECT_TRUE(base::PathExists(initial_path));
  base::FilePath new_path(temp_dir_.GetPath().AppendASCII("NewFile"));
  EXPECT_FALSE(base::PathExists(new_path));

  ASSERT_TRUE(AppendDataToFile(kTestData1));

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, base_file_->Rename(new_path));
  EXPECT_FALSE(base::PathExists(initial_path));
  EXPECT_TRUE(base::PathExists(new_path));

  ExpectHashValue(kHashOfTestData1, base_file_->Finish());
}

// Rename the file while the download is still in progress.
TEST_F(BaseFileTest, RenameWhileInProgress) {
  ASSERT_TRUE(InitializeFile());

  base::FilePath initial_path(base_file_->full_path());
  EXPECT_TRUE(base::PathExists(initial_path));
  base::FilePath new_path(temp_dir_.GetPath().AppendASCII("NewFile"));
  EXPECT_FALSE(base::PathExists(new_path));

  ASSERT_TRUE(AppendDataToFile(kTestData1));

  EXPECT_TRUE(base_file_->in_progress());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, base_file_->Rename(new_path));
  EXPECT_FALSE(base::PathExists(initial_path));
  EXPECT_TRUE(base::PathExists(new_path));

  ASSERT_TRUE(AppendDataToFile(kTestData2));
  ASSERT_TRUE(AppendDataToFile(kTestData3));

  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221266): Re-enable when RenameWithError works on Fuchsia.
#define MAYBE_RenameWithError DISABLED_RenameWithError
#else
#define MAYBE_RenameWithError RenameWithError
#endif
// Test that a failed rename reports the correct error.
TEST_F(BaseFileTest, MAYBE_RenameWithError) {
  ASSERT_TRUE(InitializeFile());

  // TestDir is a subdirectory in |temp_dir_| that we will make read-only so
  // that the rename will fail.
  base::FilePath test_dir(temp_dir_.GetPath().AppendASCII("TestDir"));
  ASSERT_TRUE(base::CreateDirectory(test_dir));

  base::FilePath new_path(test_dir.AppendASCII("TestFile"));
  EXPECT_FALSE(base::PathExists(new_path));

  {
    base::FilePermissionRestorer restore_permissions_for(test_dir);
    ASSERT_TRUE(base::MakeFileUnwritable(test_dir));
    ExpectPermissionError(base_file_->Rename(new_path));
  }

  base_file_->Finish();
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221266): Re-enable when RenameWithErrorInProgress works on
// Fuchsia.
#define MAYBE_RenameWithErrorInProgress DISABLED_RenameWithErrorInProgress
#else
#define MAYBE_RenameWithErrorInProgress RenameWithErrorInProgress
#endif
// Test that if a rename fails for an in-progress BaseFile, it remains writeable
// and renameable.
TEST_F(BaseFileTest, MAYBE_RenameWithErrorInProgress) {
  ASSERT_TRUE(InitializeFile());

  base::FilePath test_dir(temp_dir_.GetPath().AppendASCII("TestDir"));
  ASSERT_TRUE(base::CreateDirectory(test_dir));

  base::FilePath new_path(test_dir.AppendASCII("TestFile"));
  EXPECT_FALSE(base::PathExists(new_path));

  // Write some data to start with.
  ASSERT_TRUE(AppendDataToFile(kTestData1));
  ASSERT_TRUE(base_file_->in_progress());

  base::FilePath old_path = base_file_->full_path();

  {
    base::FilePermissionRestorer restore_permissions_for(test_dir);
    ASSERT_TRUE(base::MakeFileUnwritable(test_dir));
    ExpectPermissionError(base_file_->Rename(new_path));

    // The file should still be open and we should be able to continue writing
    // to it.
    ASSERT_TRUE(base_file_->in_progress());
    ASSERT_TRUE(AppendDataToFile(kTestData2));
    ASSERT_EQ(old_path.value(), base_file_->full_path().value());

    // Try to rename again, just for kicks. It should still fail.
    ExpectPermissionError(base_file_->Rename(new_path));
  }

  // Now that TestDir is writeable again, we should be able to successfully
  // rename the file.
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, base_file_->Rename(new_path));
  ASSERT_EQ(new_path.value(), base_file_->full_path().value());
  ASSERT_TRUE(AppendDataToFile(kTestData3));

  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221270): Re-enable when WriteWithError works on Fuchsia.
#define MAYBE_WriteWithError DISABLED_WriteWithError
#else
#define MAYBE_WriteWithError WriteWithError
#endif
// Test that a failed write reports an error.
TEST_F(BaseFileTest, MAYBE_WriteWithError) {
  base::FilePath path;
  ASSERT_TRUE(base::CreateTemporaryFile(&path));

  // Pass a file handle which was opened without the WRITE flag.
  // This should result in an error when writing.
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);
  base_file_ = std::make_unique<BaseFile>(download::DownloadItem::kInvalidId);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            base_file_->Initialize(path, base::FilePath(), std::move(file), 0,
                                   std::string(),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
#if BUILDFLAG(IS_WIN)
  set_expected_error(DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);
#elif BUILDFLAG(IS_POSIX)
  set_expected_error(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
#endif
  ASSERT_FALSE(AppendDataToFile(kTestData1));
  base_file_->Finish();
}

// Try to write to uninitialized file.
TEST_F(BaseFileTest, UninitializedFile) {
  expect_in_progress_ = false;
  set_expected_error(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  EXPECT_FALSE(AppendDataToFile(kTestData1));
}

// Create two |BaseFile|s with the same file, and attempt to write to both.
// Overwrite base_file_ with another file with the same name and
// non-zero contents, and make sure the last file to close 'wins'.
TEST_F(BaseFileTest, DuplicateBaseFile) {
  ASSERT_TRUE(InitializeFile());

  // Create another |BaseFile| referring to the file that |base_file_| owns.
  CreateFileWithName(base_file_->full_path());

  ASSERT_TRUE(AppendDataToFile(kTestData1));
  base_file_->Finish();
}

// Create a file and append to it.
TEST_F(BaseFileTest, AppendToBaseFile) {
  // Create a new file.
  base::FilePath existing_file_name = CreateTestFile();
  set_expected_data(kTestData4);

  // Use the file we've just created.
  base_file_ = std::make_unique<BaseFile>(download::DownloadItem::kInvalidId);
  ASSERT_EQ(
      DOWNLOAD_INTERRUPT_REASON_NONE,
      base_file_->Initialize(existing_file_name, base::FilePath(), base::File(),
                             kTestDataLength4, std::string(),
                             std::unique_ptr<crypto::SecureHash>(), false,
                             &kTestDataBytesWasted));

  const base::FilePath file_name = base_file_->full_path();
  EXPECT_NE(base::FilePath::StringType(), file_name.value());

  // Write into the file.
  EXPECT_TRUE(AppendDataToFile(kTestData1));

  base_file_->Finish();
  base_file_->Detach();
  expect_file_survives_ = true;
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221265): Re-enable when ReadonlyBaseFile works on Fuchsia.
#define MAYBE_ReadonlyBaseFile DISABLED_ReadonlyBaseFile
#else
#define MAYBE_ReadonlyBaseFile ReadonlyBaseFile
#endif
// Create a read-only file and attempt to write to it.
TEST_F(BaseFileTest, MAYBE_ReadonlyBaseFile) {
  // Create a new file.
  base::FilePath readonly_file_name = CreateTestFile();

  // Restore permissions to the file when we are done with this test.
  base::FilePermissionRestorer restore_permissions(readonly_file_name);

  // Make it read-only.
  EXPECT_TRUE(base::MakeFileUnwritable(readonly_file_name));

  // Try to overwrite it.
  base_file_ = std::make_unique<BaseFile>(download::DownloadItem::kInvalidId);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
            base_file_->Initialize(readonly_file_name, base::FilePath(),
                                   base::File(), 0, std::string(),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));

  expect_in_progress_ = false;

  const base::FilePath file_name = base_file_->full_path();
  EXPECT_NE(base::FilePath::StringType(), file_name.value());

  // Write into the file.
  set_expected_error(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  EXPECT_FALSE(AppendDataToFile(kTestData1));

  base_file_->Finish();
  base_file_->Detach();
  expect_file_survives_ = true;
}

// Open an existing file and continue writing to it. The hash of the partial
// file is known and matches the existing contents.
TEST_F(BaseFileTest, ExistingBaseFileKnownHash) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  ASSERT_TRUE(base::WriteFile(file_path, kTestData1));

  std::string hash_so_far(std::begin(kHashOfTestData1),
                          std::end(kHashOfTestData1));
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   kTestDataLength1, hash_so_far,
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_data(kTestData1);
  ASSERT_TRUE(AppendDataToFile(kTestData2));
  ASSERT_TRUE(AppendDataToFile(kTestData3));
  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

// Open an existing file and continue writing to it. The hash of the partial
// file is unknown.
TEST_F(BaseFileTest, ExistingBaseFileUnknownHash) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  ASSERT_TRUE(base::WriteFile(file_path, kTestData1));

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   kTestDataLength1, std::string(),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_data(kTestData1);
  ASSERT_TRUE(AppendDataToFile(kTestData2));
  ASSERT_TRUE(AppendDataToFile(kTestData3));
  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

// Open an existing file. The contentsof the file doesn't match the known hash.
TEST_F(BaseFileTest, ExistingBaseFileIncorrectHash) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  ASSERT_TRUE(base::WriteFile(file_path, kTestData2));

  std::string hash_so_far(std::begin(kHashOfTestData1),
                          std::end(kHashOfTestData1));
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   kTestDataLength2, hash_so_far,
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_error(download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH);
}

// Open a large existing file with a known hash and continue writing to it.
TEST_F(BaseFileTest, ExistingBaseFileLargeSizeKnownHash) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  std::string big_buffer(1024 * 200, 'a');
  ASSERT_TRUE(base::WriteFile(file_path, big_buffer));

  // Hash of partial file (1024*200 * 'a')
  const uint8_t kExpectedPartialHash[] = {
      0x4b, 0x4f, 0x0f, 0x46, 0xac, 0x02, 0xd1, 0x77, 0xde, 0xa0, 0xab,
      0x36, 0xa6, 0x6a, 0x65, 0x78, 0x40, 0xe2, 0xfb, 0x98, 0xb2, 0x0b,
      0xb2, 0x7a, 0x68, 0x8d, 0xb4, 0xd8, 0xea, 0x9c, 0xd2, 0x2c};

  // Hash of entire file (1024*400 * 'a')
  const uint8_t kExpectedFullHash[] = {
      0x0c, 0xe9, 0xf6, 0x78, 0x6b, 0x0f, 0x58, 0x49, 0x36, 0xe8, 0x83,
      0xc5, 0x09, 0x16, 0xbc, 0x5e, 0x2d, 0x07, 0x95, 0xb9, 0x42, 0x20,
      0x41, 0x7c, 0xb3, 0x38, 0xd3, 0xf4, 0xe0, 0x78, 0x89, 0x46};

  ASSERT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   big_buffer.size(),
                                   std::string(std::begin(kExpectedPartialHash),
                                               std::end(kExpectedPartialHash)),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_data(big_buffer);  // Contents of the file on Open.
  ASSERT_TRUE(AppendDataToFile(big_buffer));
  ExpectHashValue(kExpectedFullHash, base_file_->Finish());
}

// Open a large existing file. The contents doesn't match the known hash.
TEST_F(BaseFileTest, ExistingBaseFileLargeSizeIncorrectHash) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  std::string big_buffer(1024 * 200, 'a');
  ASSERT_TRUE(base::WriteFile(file_path, big_buffer));

  // Incorrect hash of partial file (1024*200 * 'a')
  const uint8_t kExpectedPartialHash[] = {
      0xc2, 0xa9, 0x08, 0xd9, 0x8f, 0x5d, 0xf9, 0x87, 0xad, 0xe4, 0x1b,
      0x5f, 0xce, 0x21, 0x30, 0x67, 0xef, 0x6c, 0xc2, 0x1e, 0xf2, 0x24,
      0x02, 0x12, 0xa4, 0x1e, 0x54, 0xb5, 0xe7, 0xc2, 0x8a, 0xe5};

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   big_buffer.size(),
                                   std::string(std::begin(kExpectedPartialHash),
                                               std::end(kExpectedPartialHash)),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_error(download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH);
}

// Open an existing file. The size of the file is too short.
TEST_F(BaseFileTest, ExistingBaseFileTooShort) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  ASSERT_TRUE(base::WriteFile(file_path, kTestData1));

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   kTestDataLength1 + 1, std::string(),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_error(download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
}

// Open an existing file. The size is larger than expected.
TEST_F(BaseFileTest, ExistingBaseFileKnownHashTooLong) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  std::string contents;
  contents.append(kTestData1);
  contents.append("Something extra");
  ASSERT_TRUE(base::WriteFile(file_path, contents));

  std::string hash_so_far(std::begin(kHashOfTestData1),
                          std::end(kHashOfTestData1));
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   kTestDataLength1, hash_so_far,
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_data(kTestData1);  // Our starting position.
  ASSERT_TRUE(AppendDataToFile(kTestData2));
  ASSERT_TRUE(AppendDataToFile(kTestData3));
  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

// Open an existing file. The size is large than expected and the hash is
// unknown.
TEST_F(BaseFileTest, ExistingBaseFileUnknownHashTooLong) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  std::string contents;
  contents.append(kTestData1);
  contents.append("Something extra");
  ASSERT_TRUE(base::WriteFile(file_path, contents));

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   kTestDataLength1, std::string(),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  set_expected_data(kTestData1);
  ASSERT_TRUE(AppendDataToFile(kTestData2));
  ASSERT_TRUE(AppendDataToFile(kTestData3));
  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

// Similar to ExistingBaseFileKnownHashTooLong test, but with a file large
// enough to requre multiple Read()s to complete. This provides additional code
// coverage for the CalculatePartialHash() logic.
TEST_F(BaseFileTest, ExistingBaseFileUnknownHashTooLongForLargeFile) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  const size_t kFileSize = 1024 * 1024;
  const size_t kIntermediateSize = kFileSize / 2 + 111;
  // |contents| is 100 bytes longer than kIntermediateSize. The latter is the
  // expected size.
  std::string contents(kIntermediateSize + 100, 'a');
  ASSERT_TRUE(base::WriteFile(file_path, contents));

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            base_file_->Initialize(file_path, base::FilePath(), base::File(),
                                   kIntermediateSize, std::string(),
                                   std::unique_ptr<crypto::SecureHash>(), false,
                                   &kTestDataBytesWasted));
  // The extra bytes should be stripped during Initialize().
  contents.resize(kIntermediateSize, 'a');
  set_expected_data(contents);
  std::string new_data(kFileSize - kIntermediateSize, 'a');
  ASSERT_TRUE(AppendDataToFile(new_data));
  const uint8_t kExpectedHash[] = {
      0x9b, 0xc1, 0xb2, 0xa2, 0x88, 0xb2, 0x6a, 0xf7, 0x25, 0x7a, 0x36,
      0x27, 0x7a, 0xe3, 0x81, 0x6a, 0x7d, 0x4f, 0x16, 0xe8, 0x9c, 0x1e,
      0x7e, 0x77, 0xd0, 0xa5, 0xc4, 0x8b, 0xad, 0x62, 0xb3, 0x60,
  };
  ExpectHashValue(kExpectedHash, base_file_->Finish());
}

// Test that a temporary file is created in the default download directory.
TEST_F(BaseFileTest, CreatedInDefaultDirectory) {
  ASSERT_TRUE(base_file_->full_path().empty());
  ASSERT_TRUE(InitializeFile());
  EXPECT_FALSE(base_file_->full_path().empty());

  // On Windows, CreateTemporaryFileInDir() will cause a path with short names
  // to be expanded into a path with long names. Thus temp_dir.GetPath() might
  // not
  // be a string-wise match to base_file_->full_path().DirName() even though
  // they are in the same directory.
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file));
  ASSERT_FALSE(temp_file.empty());
  EXPECT_STREQ(temp_file.DirName().value().c_str(),
               base_file_->full_path().DirName().value().c_str());
  base_file_->Finish();
}

TEST_F(BaseFileTest, NoDoubleDeleteAfterCancel) {
  ASSERT_TRUE(InitializeFile());
  base::FilePath full_path = base_file_->full_path();
  ASSERT_FALSE(full_path.empty());
  ASSERT_TRUE(base::PathExists(full_path));

  base_file_->Cancel();
  ASSERT_FALSE(base::PathExists(full_path));

  const char kData[] = "hello";
  ASSERT_TRUE(base::WriteFile(full_path, kData));
  // The file that we created here should stick around when the BaseFile is
  // destroyed during TearDown.
  expect_file_survives_ = true;
}

// Test that writing data to a sparse file works.
TEST_F(BaseFileTest, WriteDataToSparseFile) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("existing");
  std::string contents = kTestData1;
  ASSERT_TRUE(base::WriteFile(file_path, contents));

  base_file_->Initialize(file_path, base::FilePath(), base::File(),
                         kTestDataLength1, std::string(),
                         std::unique_ptr<crypto::SecureHash>(), true,
                         &kTestDataBytesWasted);
  // This will create a hole in the file.
  base_file_->WriteDataToFile(kTestDataLength1 + kTestDataLength2, kTestData3,
                              kTestDataLength3);
  // This should fill the hole.
  base_file_->WriteDataToFile(kTestDataLength1, kTestData2, kTestDataLength2);
  set_expected_data(contents + kTestData2 + kTestData3);
  ExpectHashValue(kHashOfTestData1To3, base_file_->Finish());
}

// Test that validating data in a file works.
TEST_F(BaseFileTest, ValidateDataInFile) {
  ASSERT_TRUE(InitializeFile());
  ASSERT_TRUE(AppendDataToFile(kTestData1));

  ASSERT_TRUE(base_file_->ValidateDataInFile(0, "Let's", 5));
  ASSERT_TRUE(base_file_->ValidateDataInFile(1, "et's ", 5));
  ASSERT_TRUE(base_file_->ValidateDataInFile(
      0, "Let's write some data to the file!\n", kTestDataLength1));
  ASSERT_TRUE(base_file_->ValidateDataInFile(kTestDataLength1 - 1, "\n", 1));
  ASSERT_FALSE(base_file_->ValidateDataInFile(kTestDataLength1, "\n", 1));
  ASSERT_FALSE(base_file_->ValidateDataInFile(kTestDataLength1 - 1, "y\n", 2));
  ASSERT_FALSE(base_file_->ValidateDataInFile(0, "et's ", 5));
  ASSERT_FALSE(base_file_->ValidateDataInFile(
      0, "Let's write some data to the file1\n", kTestDataLength1));
  ASSERT_FALSE(base_file_->ValidateDataInFile(
      0, "Let's write some data to the file1!\n", kTestDataLength1 + 1));

  base_file_->Finish();
}

}  // namespace download
