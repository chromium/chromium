// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "components/services/filesystem/directory_test_helper.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace filesystem {
namespace {

class DirectoryImplTest : public testing::Test {
 public:
  DirectoryImplTest() = default;

  mojo::Remote<mojom::Directory> CreateTempDir() {
    return test_helper_.CreateTempDir();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  DirectoryTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryImplTest);
};

constexpr char kData[] = "one two three";

TEST_F(DirectoryImplTest, Read) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Make some files.
  const struct {
    const char* name;
    uint32_t open_flags;
  } files_to_create[] = {
      {"my_file1", mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate},
      {"my_file2", mojom::kFlagWrite | mojom::kFlagCreate},
      {"my_file3", mojom::kFlagAppend | mojom::kFlagCreate}};
  for (size_t i = 0; i < base::size(files_to_create); i++) {
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile(files_to_create[i].name, mojo::NullReceiver(),
                            files_to_create[i].open_flags, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }
  // Make a directory.
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled = directory->OpenDirectory(
      "my_dir", mojo::NullReceiver(),
      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  error = base::File::Error::FILE_ERROR_FAILED;
  base::Optional<std::vector<mojom::DirectoryEntryPtr>> directory_contents;
  handled = directory->Read(&error, &directory_contents);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  ASSERT_TRUE(directory_contents.has_value());

  // Expected contents of the directory.
  std::map<std::string, mojom::FsFileType> expected_contents;
  expected_contents["my_file1"] = mojom::FsFileType::REGULAR_FILE;
  expected_contents["my_file2"] = mojom::FsFileType::REGULAR_FILE;
  expected_contents["my_file3"] = mojom::FsFileType::REGULAR_FILE;
  expected_contents["my_dir"] = mojom::FsFileType::DIRECTORY;
  // Note: We don't expose ".." or ".".

  EXPECT_EQ(expected_contents.size(), directory_contents->size());
  for (size_t i = 0; i < directory_contents->size(); i++) {
    auto& item = directory_contents.value()[i];
    ASSERT_TRUE(item);
    auto it = expected_contents.find(item->name.AsUTF8Unsafe());
    ASSERT_TRUE(it != expected_contents.end());
    EXPECT_EQ(it->second, item->type);
    expected_contents.erase(it);
  }
}

// TODO(vtl): Properly test OpenFile() and OpenDirectory() (including flags).

TEST_F(DirectoryImplTest, BasicRenameDelete) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled =
      directory->OpenFile("my_file", mojo::NullReceiver(),
                          mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Opening my_file should succeed.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFile("my_file", mojo::NullReceiver(),
                                mojom::kFlagRead | mojom::kFlagOpen, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Rename my_file to my_new_file.
  handled = directory->Rename("my_file", "my_new_file", &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Opening my_file should fail.

  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFile("my_file", mojo::NullReceiver(),
                                mojom::kFlagRead | mojom::kFlagOpen, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND, error);

  // Opening my_new_file should succeed.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFile("my_new_file", mojo::NullReceiver(),
                                mojom::kFlagRead | mojom::kFlagOpen, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Delete my_new_file (no flags).
  handled = directory->Delete("my_new_file", 0, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Opening my_new_file should fail.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFile("my_new_file", mojo::NullReceiver(),
                                mojom::kFlagRead | mojom::kFlagOpen, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND, error);
}

TEST_F(DirectoryImplTest, CantOpenDirectoriesAsFiles) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  {
    // Create a directory called 'my_file'
    mojo::Remote<mojom::Directory> my_file_directory;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenDirectory(
        "my_file", my_file_directory.BindNewPipeAndPassReceiver(),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    // Attempt to open that directory as a file. This must fail!
    mojo::Remote<mojom::File> file;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled =
        directory->OpenFile("my_file", file.BindNewPipeAndPassReceiver(),
                            mojom::kFlagRead | mojom::kFlagOpen, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_A_FILE, error);
  }
}

TEST_F(DirectoryImplTest, Clone) {
  mojo::Remote<mojom::Directory> clone_one;
  mojo::Remote<mojom::Directory> clone_two;
  base::File::Error error;

  {
    mojo::Remote<mojom::Directory> directory = CreateTempDir();
    directory->Clone(clone_one.BindNewPipeAndPassReceiver());
    directory->Clone(clone_two.BindNewPipeAndPassReceiver());

    // Original temporary directory goes out of scope here; shouldn't be
    // deleted since it has clones.
  }

  std::vector<uint8_t> data(kData, kData + strlen(kData));
  {
    bool handled = clone_one->WriteFile("data", data, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    std::vector<uint8_t> file_contents;
    bool handled = clone_two->ReadEntireFile("data", &error, &file_contents);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    EXPECT_EQ(data, file_contents);
  }
}

TEST_F(DirectoryImplTest, WriteFileReadFile) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  std::vector<uint8_t> data(kData, kData + strlen(kData));
  {
    bool handled = directory->WriteFile("data", data, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    std::vector<uint8_t> file_contents;
    bool handled = directory->ReadEntireFile("data", &error, &file_contents);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);

    EXPECT_EQ(data, file_contents);
  }
}

TEST_F(DirectoryImplTest, ReadEmptyFileIsNotFoundError) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  {
    std::vector<uint8_t> file_contents;
    bool handled =
        directory->ReadEntireFile("doesnt_exist", &error, &file_contents);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND, error);
  }
}

TEST_F(DirectoryImplTest, CantReadEntireFileOnADirectory) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create a directory
  {
    mojo::Remote<mojom::Directory> my_file_directory;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenDirectory(
        "my_dir", my_file_directory.BindNewPipeAndPassReceiver(),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  // Try to read it as a file
  {
    std::vector<uint8_t> file_contents;
    bool handled = directory->ReadEntireFile("my_dir", &error, &file_contents);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_A_FILE, error);
  }
}

TEST_F(DirectoryImplTest, CantWriteFileOnADirectory) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create a directory
  {
    mojo::Remote<mojom::Directory> my_file_directory;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenDirectory(
        "my_dir", my_file_directory.BindNewPipeAndPassReceiver(),
        mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }

  {
    std::vector<uint8_t> data(kData, kData + strlen(kData));
    bool handled = directory->WriteFile("my_dir", data, &error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_A_FILE, error);
  }
}

TEST_F(DirectoryImplTest, Flush) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  {
    bool handled = directory->Flush(&error);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
  }
}

}  // namespace
}  // namespace filesystem
