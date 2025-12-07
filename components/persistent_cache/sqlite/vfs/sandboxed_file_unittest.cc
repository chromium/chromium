// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace persistent_cache {

namespace {

constexpr size_t kTestBufferLength = 1024;

class SandboxedFileTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temporary_directory_.CreateUniqueTempDir());
    shared_region_ =
        base::UnsafeSharedMemoryRegion::Create(sizeof(SharedAtomicLock));
  }

  std::unique_ptr<SandboxedFile> CreateEmptyFile(const std::string& file_name) {
    base::WritableSharedMemoryMapping mapped_shared_lock = shared_region_.Map();

    base::FilePath path = temporary_directory_.GetPath().AppendASCII(file_name);
    base::File file(path, base::File::FLAG_CREATE_ALWAYS |
                              base::File::FLAG_READ | base::File::FLAG_WRITE);
    return std::make_unique<SandboxedFile>(
        std::move(file), SandboxedFile::AccessRights::kReadWrite,
        std::move(mapped_shared_lock));
  }

  // A helper that takes ownership of a `SandboxedFile` and opens it for its
  // lifetime.
  class OpenedFile {
   public:
    explicit OpenedFile(std::unique_ptr<SandboxedFile> file)
        : file_(std::move(file)) {
      file_->OnFileOpened(
          file_->TakeUnderlyingFile(SandboxedFile::FileType::kMainDb));
    }
    ~OpenedFile() { file_->Close(); }
    SandboxedFile* operator->() { return file_.get(); }

   private:
    std::unique_ptr<SandboxedFile> file_;
  };

  OpenedFile CreateAndOpenEmptyFile(std::string_view file_name) {
    return OpenedFile(CreateEmptyFile(std::string(file_name)));
  }

  // Simulate an OpenFile from the VFS delegate.
  void OpenFile(SandboxedFile* file) {
    file->OnFileOpened(
        file->TakeUnderlyingFile(SandboxedFile::FileType::kMainDb));
  }

  int ReadToBuffer(SandboxedFile* file, size_t offset) {
    // Prepare the buffer used for readback.
    buffer_.resize(kTestBufferLength);
    std::fill(buffer_.begin(), buffer_.end(), 0xCD);

    // Read from the underlying file.
    auto buffer_as_span = base::span(buffer_);
    return file->Read(buffer_as_span.data(), buffer_as_span.size(), offset);
  }

  int WriteToFile(SandboxedFile* file,
                  size_t offset,
                  std::string_view content) {
    return file->Write(content.data(), content.size(), offset);
  }

  base::span<uint8_t> GetReadBuffer() { return base::span(buffer_); }

 private:
  base::ScopedTempDir temporary_directory_;
  std::vector<uint8_t> buffer_;
  base::UnsafeSharedMemoryRegion shared_region_;
};

TEST_F(SandboxedFileTest, OpenClose) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("open");
  EXPECT_FALSE(file->IsValid());

  OpenFile(file.get());
  EXPECT_TRUE(file->IsValid());
  EXPECT_FALSE(
      file->TakeUnderlyingFile(SandboxedFile::FileType::kMainDb).IsValid());

  file->Close();
  EXPECT_FALSE(file->IsValid());
}

TEST_F(SandboxedFileTest, ReOpen) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("re-open");
  OpenFile(file.get());
  file->Close();

  // It is valid to re-open a file after a close.
  OpenFile(file.get());
  EXPECT_TRUE(file->IsValid());
  file->Close();
  EXPECT_FALSE(file->IsValid());
}

TEST_F(SandboxedFileTest, BasicReadWrite) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("basic");
  OpenFile(file.get());

  std::vector<uint8_t> content(kTestBufferLength, 0xCA);
  EXPECT_EQ(file->Write(content.data(), content.size(), 0), SQLITE_OK);

  // Read back data.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_OK);
  EXPECT_EQ(GetReadBuffer(), base::span(content));
}

TEST_F(SandboxedFileTest, ReadToShort) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("short");
  OpenFile(file.get());

  const std::string content = "This is a short text";
  EXPECT_EQ(WriteToFile(file.get(), 0, content), SQLITE_OK);

  // Read back data. Read(..) must fill the buffer with zeroes.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto expected_buffer_as_span = base::span(expected_buffer);
  std::copy(content.begin(), content.end(), expected_buffer_as_span.begin());

  EXPECT_EQ(GetReadBuffer(), expected_buffer_as_span);
}

TEST_F(SandboxedFileTest, ReadTooFar) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("too-short");
  OpenFile(file.get());

  const std::string content = "This is a too short text";
  EXPECT_EQ(WriteToFile(file.get(), 0, content), SQLITE_OK);

  // SQLite itself does not treat reading beyond the end of the file as an
  // error.
  constexpr size_t kTooFarOffset = 0x100000;
  EXPECT_EQ(ReadToBuffer(file.get(), kTooFarOffset), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer. A buffer full of zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, ReadWithOffset) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("offset");
  OpenFile(file.get());

  const std::string content = "The answer is 42";
  EXPECT_EQ(WriteToFile(file.get(), 0, content), SQLITE_OK);

  // Read back data. Read(..) must fill the buffer with zeroes.
  const size_t kReadOffset = content.find("42");
  EXPECT_EQ(ReadToBuffer(file.get(), kReadOffset), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto content_at_offset = base::byte_span_with_nul_from_cstring("42");
  std::copy(content_at_offset.begin(), content_at_offset.end(),
            expected_buffer.begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, WriteWithOffset) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("offset");
  OpenFile(file.get());

  // Write pass end-of-file should increase the file size and fill the gab with
  // zeroes.
  const std::string content = "The answer is 42";
  constexpr size_t kWriteOffset = 42;
  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset, content), SQLITE_OK);

  // Read back data. Read(..) must fill the buffer with zeroes.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto expected_buffer_as_span = base::span(expected_buffer);
  std::copy(content.begin(), content.end(),
            expected_buffer_as_span.subspan(kWriteOffset).begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, OverlappingWrites) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("writes");
  OpenFile(file.get());

  const std::string content1 = "aaa";
  const std::string content2 = "bbb";
  const std::string content3 = "ccc";

  const size_t kWriteOffset1 = 0;
  const size_t kWriteOffset2 = 4;
  const size_t kWriteOffset3 = 2;

  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset1, content1), SQLITE_OK);
  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset2, content2), SQLITE_OK);
  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset3, content3), SQLITE_OK);

  // Read back data.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto expected_text = base::byte_span_with_nul_from_cstring("aacccbb");
  std::copy(expected_text.begin(), expected_text.end(),
            expected_buffer.begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, Truncate) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("truncate");
  OpenFile(file.get());

  std::vector<uint8_t> content(kTestBufferLength, 0xCA);
  EXPECT_EQ(file->Write(content.data(), content.size(), 0), SQLITE_OK);

  // Validate filesize before truncate.
  sqlite3_int64 file_size = 0;
  EXPECT_EQ(file->FileSize(&file_size), SQLITE_OK);
  EXPECT_EQ(static_cast<size_t>(file_size), kTestBufferLength);

  // Truncate the content of the file.
  constexpr size_t kTruncateLength = 10;
  file->Truncate(kTruncateLength);

  // Ensure the filesize changed after truncate.
  EXPECT_EQ(file->FileSize(&file_size), SQLITE_OK);
  EXPECT_EQ(static_cast<size_t>(file_size), kTruncateLength);

  // Read back data.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto content_as_span = base::span(content);
  std::copy(content_as_span.begin(), content_as_span.begin() + kTruncateLength,
            expected_buffer.begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, LockBasics) {
  auto file = CreateAndOpenEmptyFile("lock");
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_NONE);

  EXPECT_EQ(file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);

  EXPECT_EQ(file->Lock(SQLITE_LOCK_RESERVED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_RESERVED);

  EXPECT_EQ(file->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);

  EXPECT_EQ(file->Unlock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);

  EXPECT_EQ(file->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_NONE);
}

TEST_F(SandboxedFileTest, AcquireSameLockLevel) {
  auto file = CreateAndOpenEmptyFile("lock");

  EXPECT_EQ(file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_RESERVED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_RESERVED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);

  // Re-acquire EXCLUSIVE must be valid.
  EXPECT_EQ(file->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);
}

TEST_F(SandboxedFileTest, AcquireLowerLockLevel) {
  auto file = CreateAndOpenEmptyFile("lock");

  EXPECT_EQ(file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_RESERVED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_RESERVED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);

  // Acquiring the lower SHARED lock when the connection is at EXCLUSIVE must be
  // valid.
  EXPECT_EQ(file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);
}

TEST_F(SandboxedFileTest, UnlockWhenNone) {
  auto file = CreateAndOpenEmptyFile("lock");

  EXPECT_EQ(file->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_NONE);
}

TEST_F(SandboxedFileTest, UnlockToNone) {
  auto file = CreateAndOpenEmptyFile("lock");

  EXPECT_EQ(file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_RESERVED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_RESERVED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);

  // Downgrade the connection from EXCLUSIVE to NONE.
  EXPECT_EQ(file->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_NONE);
}

TEST_F(SandboxedFileTest, UnlockToShared) {
  auto file = CreateAndOpenEmptyFile("lock");

  EXPECT_EQ(file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_RESERVED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_RESERVED);
  EXPECT_EQ(file->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);

  // Downgrade the connection from EXCLUSIVE to SHARED.
  EXPECT_EQ(file->Unlock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);
}

TEST_F(SandboxedFileTest, MultipleLocks) {
  auto reader1 = CreateAndOpenEmptyFile("multi-lock");
  auto reader2 = CreateAndOpenEmptyFile("multi-lock");
  auto reader3 = CreateAndOpenEmptyFile("multi-lock");
  auto writer1 = CreateAndOpenEmptyFile("multi-lock");
  auto writer2 = CreateAndOpenEmptyFile("multi-lock");

  // Take SHARED lock for the first reader.
  EXPECT_EQ(reader1->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(reader1->LockModeForTesting(), SQLITE_LOCK_SHARED);

  // First writer is getting the RESERVED lock.
  EXPECT_EQ(writer1->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(writer1->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(writer1->Lock(SQLITE_LOCK_RESERVED), SQLITE_OK);
  EXPECT_EQ(writer1->LockModeForTesting(), SQLITE_LOCK_RESERVED);

  // Second reader should be able to get a SHARED lock even with the RESERVED
  // lock.
  EXPECT_EQ(reader2->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(reader2->LockModeForTesting(), SQLITE_LOCK_SHARED);

  // Second writer can't get the RESERVED lock.
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_RESERVED), SQLITE_BUSY);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_SHARED);

  // Try to upgrade the lock to EXCLUSIVE but fails since there are pending
  // readers.
  EXPECT_EQ(writer1->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_BUSY);
  EXPECT_EQ(writer1->LockModeForTesting(), SQLITE_LOCK_PENDING);

  // No new writer can get the lock EXCLUSIVE.
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_RESERVED), SQLITE_BUSY);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_BUSY);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(writer2->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_NONE);

  // No new reader can enter while a writer has the PENDING lock.
  EXPECT_EQ(reader3->Lock(SQLITE_LOCK_SHARED), SQLITE_BUSY);

  // Release a SHARED lock.
  EXPECT_EQ(reader1->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(reader1->LockModeForTesting(), SQLITE_LOCK_NONE);

  // Try to upgrade the lock to EXCLUSIVE but fails since there are still
  // pending readers.
  EXPECT_EQ(writer1->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_BUSY);
  EXPECT_EQ(writer1->LockModeForTesting(), SQLITE_LOCK_PENDING);

  // Release the last SHARED lock.
  EXPECT_EQ(reader2->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(reader2->LockModeForTesting(), SQLITE_LOCK_NONE);

  // The write lock can now be upgraded to EXCLUSIVE.
  EXPECT_EQ(writer1->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(writer1->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);

  // No other writer can get the lock.
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_SHARED), SQLITE_BUSY);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_NONE);

  // Unlock the writer.
  EXPECT_EQ(writer1->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(writer1->LockModeForTesting(), SQLITE_LOCK_NONE);

  // Second writer can now get the lock.
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_SHARED);
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_RESERVED), SQLITE_OK);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_RESERVED);
  EXPECT_EQ(writer2->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);
  EXPECT_EQ(writer2->Unlock(SQLITE_LOCK_NONE), SQLITE_OK);
  EXPECT_EQ(writer2->LockModeForTesting(), SQLITE_LOCK_NONE);
}

TEST_F(SandboxedFileTest, LockHotJournal) {
  auto file = CreateAndOpenEmptyFile("lock");

  EXPECT_EQ(file->Lock(SQLITE_LOCK_SHARED), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_SHARED);

  // It is valid to skip the RESERVED lock in the presence of am hot-journal.
  // There can't be any RESERVED on any connection at that point since any
  // attempt to open the database will get into the same state (an hot-journal
  // that forced an exclusive lock).
  EXPECT_EQ(file->Lock(SQLITE_LOCK_EXCLUSIVE), SQLITE_OK);
  EXPECT_EQ(file->LockModeForTesting(), SQLITE_LOCK_EXCLUSIVE);
}

TEST_F(SandboxedFileTest, GetFile) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("get_file");
  EXPECT_FALSE(file->IsValid());

  // Before opening, GetFile() should return the underlying file.
  EXPECT_EQ(file->GetFile().GetPlatformFile(),
            file->UnderlyingFileForTesting().GetPlatformFile());

  OpenFile(file.get());
  EXPECT_TRUE(file->IsValid());

  // After opening, GetFile() should return the opened file.
  EXPECT_EQ(file->GetFile().GetPlatformFile(),
            file->OpenedFileForTesting().GetPlatformFile());

  file->Close();
  EXPECT_FALSE(file->IsValid());

  // After closing, GetFile() should return the new underlying file, which was
  // the opened file.
  EXPECT_EQ(file->GetFile().GetPlatformFile(),
            file->UnderlyingFileForTesting().GetPlatformFile());
}

}  // namespace

}  // namespace persistent_cache
