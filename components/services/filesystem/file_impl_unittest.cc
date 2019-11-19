// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "components/services/filesystem/directory_test_helper.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace filesystem {
namespace {

class FileImplTest : public testing::Test {
 public:
  FileImplTest() = default;

  mojo::Remote<mojom::Directory> CreateTempDir() {
    return test_helper_.CreateTempDir();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  DirectoryTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(FileImplTest);
};

TEST_F(FileImplTest, CreateWriteCloseRenameOpenRead) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;
  bool handled = false;

  {
    // Create my_file.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = base::File::Error::FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                          &error, &num_bytes_written);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Close((&error));
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  // Rename it.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->Rename("my_file", "your_file", &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  {
    // Open my_file again.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("your_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagRead | mojom::kFlagOpen, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Read from it.
    base::Optional<std::vector<uint8_t>> bytes_read;
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Read(3, 1, mojom::Whence::FROM_BEGIN, &error, &bytes_read);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    ASSERT_TRUE(bytes_read.has_value());
    ASSERT_EQ(3u, bytes_read.value().size());
    EXPECT_EQ(static_cast<uint8_t>('e'), bytes_read.value()[0]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read.value()[1]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read.value()[2]);
  }

  // TODO(vtl): Test read/write offset options.
}

TEST_F(FileImplTest, CantWriteInReadMode) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  std::vector<uint8_t> bytes_to_write;
  bytes_to_write.push_back(static_cast<uint8_t>('h'));
  bytes_to_write.push_back(static_cast<uint8_t>('e'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('o'));

  {
    // Create my_file.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Write to it.
    error = base::File::Error::FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                          &error, &num_bytes_written);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Close((&error));
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    // Open my_file again, this time with read only mode.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagRead | mojom::kFlagOpen, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Try to write in read mode; it should fail.
    error = base::File::Error::FILE_OK;
    uint32_t num_bytes_written = 0;
    handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                          &error, &num_bytes_written);

    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_ERROR_FAILED, error);
    EXPECT_EQ(0u, num_bytes_written);

    // Close it.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Close((&error));
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }
}

TEST_F(FileImplTest, OpenInAppendMode) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  {
    // Create my_file.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = base::File::Error::FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                          &error, &num_bytes_written);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Close(&error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    // Append to my_file.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagAppend | mojom::kFlagOpen, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('g'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('d'));
    bytes_to_write.push_back(static_cast<uint8_t>('b'));
    bytes_to_write.push_back(static_cast<uint8_t>('y'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    error = base::File::Error::FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                          &error, &num_bytes_written);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Close((&error));
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    // Open my_file again.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagRead | mojom::kFlagOpen, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Read from it.
    base::Optional<std::vector<uint8_t>> bytes_read;
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Read(12, 0, mojom::Whence::FROM_BEGIN, &error, &bytes_read);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    ASSERT_TRUE(bytes_read.has_value());
    ASSERT_EQ(12u, bytes_read.value().size());
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read.value()[3]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read.value()[4]);
    EXPECT_EQ(static_cast<uint8_t>('g'), bytes_read.value()[5]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read.value()[6]);
  }
}

TEST_F(FileImplTest, OpenInTruncateMode) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  {
    // Create my_file.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = base::File::Error::FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                          &error, &num_bytes_written);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Close(&error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    // Append to my_file.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenFile(
        "my_file", file.BindNewPipeAndPassReceiver(),
        mojom::kFlagWrite | mojom::kFlagOpenTruncated, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('g'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('d'));
    bytes_to_write.push_back(static_cast<uint8_t>('b'));
    bytes_to_write.push_back(static_cast<uint8_t>('y'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    error = base::File::Error::FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                          &error, &num_bytes_written);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Close(&error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    // Open my_file again.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagRead | mojom::kFlagOpen, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Read from it.
    base::Optional<std::vector<uint8_t>> bytes_read;
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Read(7, 0, mojom::Whence::FROM_BEGIN, &error, &bytes_read);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    ASSERT_TRUE(bytes_read.has_value());
    ASSERT_EQ(7u, bytes_read.value().size());
    EXPECT_EQ(static_cast<uint8_t>('g'), bytes_read.value()[0]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read.value()[1]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read.value()[2]);
    EXPECT_EQ(static_cast<uint8_t>('d'), bytes_read.value()[3]);
  }
}

// Note: Ignore nanoseconds, since it may not always be supported. We expect at
// least second-resolution support though.
TEST_F(FileImplTest, StatTouch) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  mojo::Remote<mojom::File> file;
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled =
      directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                          mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Stat it.
  error = base::File::Error::FILE_ERROR_FAILED;
  mojom::FileInformationPtr file_info;
  handled = file->Stat(&error, &file_info);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(mojom::FsFileType::REGULAR_FILE, file_info->type);
  EXPECT_EQ(0, file_info->size);
  EXPECT_GT(file_info->atime, 0);  // Expect that it's not 1970-01-01.
  EXPECT_GT(file_info->mtime, 0);
  double first_mtime = file_info->mtime;

  // Touch only the atime.
  error = base::File::Error::FILE_ERROR_FAILED;
  mojom::TimespecOrNowPtr t(mojom::TimespecOrNow::New());
  t->now = false;
  const int64_t kPartyTime1 = 1234567890;  // Party like it's 2009-02-13.
  t->seconds = kPartyTime1;
  handled = file->Touch(std::move(t), nullptr, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Stat again.
  error = base::File::Error::FILE_ERROR_FAILED;
  file_info.reset();
  handled = file->Stat(&error, &file_info);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime);
  EXPECT_EQ(first_mtime, file_info->mtime);

  // Touch only the mtime.
  t = mojom::TimespecOrNow::New();
  t->now = false;
  const int64_t kPartyTime2 = 1425059525;  // No time like the present.
  t->seconds = kPartyTime2;
  handled = file->Touch(nullptr, std::move(t), &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Stat again.
  error = base::File::Error::FILE_ERROR_FAILED;
  file_info.reset();
  handled = file->Stat(&error, &file_info);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime);
  EXPECT_EQ(kPartyTime2, file_info->mtime);

  // TODO(vtl): Also test non-zero file size.
  // TODO(vtl): Also test Touch() "now" options.
  // TODO(vtl): Also test touching both atime and mtime.
}

TEST_F(FileImplTest, TellSeek) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  mojo::Remote<mojom::File> file;
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled =
      directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                          mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write(1000, '!');
  error = base::File::Error::FILE_ERROR_FAILED;
  uint32_t num_bytes_written = 0;
  handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT, &error,
                        &num_bytes_written);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int size = static_cast<int>(num_bytes_written);

  // Tell.
  error = base::File::Error::FILE_ERROR_FAILED;
  int64_t position = -1;
  handled = file->Tell(&error, &position);
  ASSERT_TRUE(handled);
  // Should be at the end.
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(size, position);

  // Seek back 100.
  error = base::File::Error::FILE_ERROR_FAILED;
  position = -1;
  handled = file->Seek(-100, mojom::Whence::FROM_CURRENT, &error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(size - 100, position);

  // Tell.
  error = base::File::Error::FILE_ERROR_FAILED;
  position = -1;
  handled = file->Tell(&error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(size - 100, position);

  // Seek to 123 from start.
  error = base::File::Error::FILE_ERROR_FAILED;
  position = -1;
  handled = file->Seek(123, mojom::Whence::FROM_BEGIN, &error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(123, position);

  // Tell.
  error = base::File::Error::FILE_ERROR_FAILED;
  position = -1;
  handled = file->Tell(&error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(123, position);

  // Seek to 123 back from end.
  error = base::File::Error::FILE_ERROR_FAILED;
  position = -1;
  handled = file->Seek(-123, mojom::Whence::FROM_END, &error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(size - 123, position);

  // Tell.
  error = base::File::Error::FILE_ERROR_FAILED;
  position = -1;
  handled = file->Tell(&error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(size - 123, position);

  // TODO(vtl): Check that seeking actually affects reading/writing.
  // TODO(vtl): Check that seeking can extend the file?
}

TEST_F(FileImplTest, Dup) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  mojo::Remote<mojom::File> file1;
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled = directory->OpenFile(
      "my_file", file1.BindNewPipeAndPassReceiver(),
      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write;
  bytes_to_write.push_back(static_cast<uint8_t>('h'));
  bytes_to_write.push_back(static_cast<uint8_t>('e'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('o'));
  error = base::File::Error::FILE_ERROR_FAILED;
  uint32_t num_bytes_written = 0;
  handled = file1->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT, &error,
                         &num_bytes_written);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int end_hello_pos = static_cast<int>(num_bytes_written);

  // Dup it.
  mojo::Remote<mojom::File> file2;
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = file1->Dup(file2.BindNewPipeAndPassReceiver(), &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // |file2| should have the same position.
  error = base::File::Error::FILE_ERROR_FAILED;
  int64_t position = -1;
  handled = file2->Tell(&error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(end_hello_pos, position);

  // Write using |file2|.
  std::vector<uint8_t> more_bytes_to_write;
  more_bytes_to_write.push_back(static_cast<uint8_t>('w'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('o'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('r'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('l'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('d'));
  error = base::File::Error::FILE_ERROR_FAILED;
  num_bytes_written = 0;
  handled = file2->Write(more_bytes_to_write, 0, mojom::Whence::FROM_CURRENT,
                         &error, &num_bytes_written);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(more_bytes_to_write.size(), num_bytes_written);
  const int end_world_pos = end_hello_pos + static_cast<int>(num_bytes_written);

  // |file1| should have the same position.
  error = base::File::Error::FILE_ERROR_FAILED;
  position = -1;
  handled = file1->Tell(&error, &position);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(end_world_pos, position);

  // Close |file1|.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = file1->Close(&error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Read everything using |file2|.
  base::Optional<std::vector<uint8_t>> bytes_read;
  error = base::File::Error::FILE_ERROR_FAILED;
  handled =
      file2->Read(1000, 0, mojom::Whence::FROM_BEGIN, &error, &bytes_read);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  ASSERT_TRUE(bytes_read.has_value());
  ASSERT_EQ(static_cast<size_t>(end_world_pos), bytes_read.value().size());
  // Just check the first and last bytes.
  EXPECT_EQ(static_cast<uint8_t>('h'), bytes_read.value()[0]);
  EXPECT_EQ(static_cast<uint8_t>('d'), bytes_read.value()[end_world_pos - 1]);

  // TODO(vtl): Test that |file2| has the same open options as |file1|.
}

TEST_F(FileImplTest, Truncate) {
  const uint32_t kInitialSize = 1000;
  const uint32_t kTruncatedSize = 654;

  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  mojo::Remote<mojom::File> file;
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled =
      directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                          mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write(kInitialSize, '!');
  error = base::File::Error::FILE_ERROR_FAILED;
  uint32_t num_bytes_written = 0;
  handled = file->Write(bytes_to_write, 0, mojom::Whence::FROM_CURRENT, &error,
                        &num_bytes_written);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_EQ(kInitialSize, num_bytes_written);

  // Stat it.
  error = base::File::Error::FILE_ERROR_FAILED;
  mojom::FileInformationPtr file_info;
  handled = file->Stat(&error, &file_info);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kInitialSize, file_info->size);

  // Truncate it.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = file->Truncate(kTruncatedSize, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Stat again.
  error = base::File::Error::FILE_ERROR_FAILED;
  file_info.reset();
  handled = file->Stat(&error, &file_info);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kTruncatedSize, file_info->size);
}

TEST_F(FileImplTest, AsHandle) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  {
    // Create my_file.
    mojo::Remote<mojom::File> file1;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenFile(
        "my_file", file1.BindNewPipeAndPassReceiver(),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Fetch the file.
    error = base::File::Error::FILE_ERROR_FAILED;
    base::File raw_file;
    handled = file1->AsHandle(&error, &raw_file);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    ASSERT_TRUE(raw_file.IsValid());
    EXPECT_EQ(5, raw_file.WriteAtCurrentPos("hello", 5));
  }

  {
    // Reopen my_file.
    mojo::Remote<mojom::File> file2;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file2.BindNewPipeAndPassReceiver(),
                            mojom::kFlagRead | mojom::kFlagOpen, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Verify that we wrote data raw on the file descriptor.
    base::Optional<std::vector<uint8_t>> bytes_read;
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file2->Read(5, 0, mojom::Whence::FROM_BEGIN, &error, &bytes_read);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    ASSERT_TRUE(bytes_read.has_value());
    ASSERT_EQ(5u, bytes_read.value().size());
    EXPECT_EQ(static_cast<uint8_t>('h'), bytes_read.value()[0]);
    EXPECT_EQ(static_cast<uint8_t>('e'), bytes_read.value()[1]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read.value()[2]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read.value()[3]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read.value()[4]);
  }
}

TEST_F(FileImplTest, SimpleLockUnlock) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  mojo::Remote<mojom::File> file;
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled = directory->OpenFile(
      "my_file", file.BindNewPipeAndPassReceiver(),
      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Lock the file.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = file->Lock(&error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Unlock the file.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = file->Unlock(&error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
}

TEST_F(FileImplTest, CantDoubleLock) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  mojo::Remote<mojom::File> file;
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled = directory->OpenFile(
      "my_file", file.BindNewPipeAndPassReceiver(),
      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Lock the file.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = file->Lock(&error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Lock the file again.
  error = base::File::Error::FILE_OK;
  handled = file->Lock(&error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_FAILED, error);
}

TEST_F(FileImplTest, ClosingFileClearsLock) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  {
    // Create my_file.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenFile(
        "my_file", file.BindNewPipeAndPassReceiver(),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagOpenAlways, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // Lock the file.
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Lock(&error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    // Open the file again.
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenFile(
        "my_file", file.BindNewPipeAndPassReceiver(),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagOpenAlways, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    // The file shouldn't be locked (and we check by trying to lock it).
    error = base::File::Error::FILE_ERROR_FAILED;
    handled = file->Lock(&error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }
}

}  // namespace
}  // namespace filesystem
