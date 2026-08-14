// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "components/services/filesystem/directory_test_helper.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace filesystem {
namespace {

class DirectoryImplTest : public testing::Test {
 public:
  DirectoryImplTest() = default;

  DirectoryImplTest(const DirectoryImplTest&) = delete;
  DirectoryImplTest& operator=(const DirectoryImplTest&) = delete;

  mojo::Remote<mojom::Directory> CreateTempDir() {
    return test_helper_.CreateTempDir();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  DirectoryTestHelper test_helper_;
};

constexpr char kData[] = "one two three";

TEST_F(DirectoryImplTest, Read) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Make some files.
  struct FilesToCreate {
    const char* name;
    uint32_t open_flags;
  };
  const auto files_to_create = std::to_array<FilesToCreate>({
      {"my_file1", mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate},
      {"my_file2", mojom::kFlagWrite | mojom::kFlagCreate},
      {"my_file3", mojom::kFlagAppend | mojom::kFlagCreate},
  });
  for (size_t i = 0; i < std::size(files_to_create); i++) {
    error = base::File::Error::FILE_ERROR_FAILED;
    base::File tmp_base_file;
    bool handled = directory->OpenFileHandle(files_to_create[i].name,
                                             files_to_create[i].open_flags,
                                             &error, &tmp_base_file);
    ASSERT_TRUE(handled);
    EXPECT_EQ(base::File::Error::FILE_OK, error);
    tmp_base_file.Close();
  }
  // Make a directory.
  error = base::File::Error::FILE_ERROR_FAILED;
  bool handled = directory->OpenDirectory(
      "my_dir", mojo::NullReceiver(),
      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  error = base::File::Error::FILE_ERROR_FAILED;
  std::optional<std::vector<mojom::DirectoryEntryPtr>> directory_contents;
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

// TODO(vtl): Properly test OpenDirectory() (including flags).

TEST_F(DirectoryImplTest, BasicRenameDelete) {
  mojo::Remote<mojom::Directory> directory = CreateTempDir();
  base::File::Error error;

  // Create my_file.
  error = base::File::Error::FILE_ERROR_FAILED;
  base::File tmp_base_file;
  bool handled = directory->OpenFileHandle(
      "my_file", mojom::kFlagWrite | mojom::kFlagCreate, &error,
      &tmp_base_file);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Opening my_file should succeed.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFileHandle(
      "my_file", mojom::kFlagRead | mojom::kFlagOpen, &error, &tmp_base_file);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  tmp_base_file.Close();

  // Rename my_file to my_new_file.
  handled = directory->Rename("my_file", "my_new_file", &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  // Opening my_file should fail.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFileHandle(
      "my_file", mojom::kFlagRead | mojom::kFlagOpen, &error, &tmp_base_file);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND, error);
  tmp_base_file.Close();

  // Opening my_new_file should succeed.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFileHandle("my_new_file",
                                      mojom::kFlagRead | mojom::kFlagOpen,
                                      &error, &tmp_base_file);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  tmp_base_file.Close();

  // Delete my_new_file (no flags).
  handled = directory->Delete("my_new_file", 0, &error);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  tmp_base_file.Close();

  // Opening my_new_file should fail.
  error = base::File::Error::FILE_ERROR_FAILED;
  handled = directory->OpenFileHandle("my_new_file",
                                      mojom::kFlagRead | mojom::kFlagOpen,
                                      &error, &tmp_base_file);
  ASSERT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND, error);
  tmp_base_file.Close();
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
    base::File tmp_file_handle;
    error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = directory->OpenFileHandle(
        "my_file", mojom::kFlagRead | mojom::kFlagOpen, &error,
        &tmp_file_handle);
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

  std::vector<uint8_t> data(kData, UNSAFE_TODO(kData + strlen(kData)));
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

  std::vector<uint8_t> data(kData, UNSAFE_TODO(kData + strlen(kData)));
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
    std::vector<uint8_t> data(kData, UNSAFE_TODO(kData + strlen(kData)));
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

class ReadOnlyDirectoryImplTest : public testing::Test {
 public:
  ReadOnlyDirectoryImplTest() = default;

  ReadOnlyDirectoryImplTest(const ReadOnlyDirectoryImplTest&) = delete;
  ReadOnlyDirectoryImplTest& operator=(const ReadOnlyDirectoryImplTest&) =
      delete;

  mojo::Remote<mojom::Directory> CreateReadOnlyTempDir() {
    return test_helper_.CreateReadOnlyTempDir();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  DirectoryTestHelper test_helper_;
};

TEST_F(ReadOnlyDirectoryImplTest, ReadAndStat) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  base::File::Error error = base::File::Error::FILE_ERROR_FAILED;
  std::optional<std::vector<mojom::DirectoryEntryPtr>> directory_contents;
  bool handled = directory->Read(&error, &directory_contents);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_TRUE(directory_contents.has_value());
  if (directory_contents.has_value()) {
    EXPECT_EQ(2u, directory_contents->size());
  }

  error = base::File::Error::FILE_ERROR_FAILED;
  mojom::FileInformationPtr file_info;
  handled = directory->StatFile("existing_file", &error, &file_info);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_TRUE(file_info);

  error = base::File::Error::FILE_ERROR_FAILED;
  bool is_writable = true;
  handled = directory->IsWritable("existing_file", &error, &is_writable);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_FALSE(is_writable);
}

TEST_F(ReadOnlyDirectoryImplTest, OpenFileHandleReadOnly) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  base::File::Error error = base::File::Error::FILE_ERROR_FAILED;
  base::File file_handle;
  bool handled = directory->OpenFileHandle("existing_file",
                                           mojom::kFlagRead | mojom::kFlagOpen,
                                           &error, &file_handle);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);
  EXPECT_TRUE(file_handle.IsValid());
  file_handle.Close();

  error = base::File::Error::FILE_OK;
  handled = directory->OpenFileHandle("existing_file",
                                      mojom::kFlagWrite | mojom::kFlagOpen,
                                      &error, &file_handle);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);
  EXPECT_FALSE(file_handle.IsValid());

  error = base::File::Error::FILE_OK;
  handled = directory->OpenFileHandle(
      "existing_file", mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagOpen,
      &error, &file_handle);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);
  EXPECT_FALSE(file_handle.IsValid());
}

TEST_F(ReadOnlyDirectoryImplTest, OpenFileHandlesReadOnly) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  std::vector<mojom::FileOpenDetailsPtr> details;

  mojom::FileOpenDetailsPtr read_detail = mojom::FileOpenDetails::New();
  read_detail->path = "existing_file";
  read_detail->open_flags = mojom::kFlagRead | mojom::kFlagOpen;
  details.push_back(std::move(read_detail));

  mojom::FileOpenDetailsPtr write_detail = mojom::FileOpenDetails::New();
  write_detail->path = "existing_file";
  write_detail->open_flags = mojom::kFlagWrite | mojom::kFlagOpen;
  details.push_back(std::move(write_detail));

  std::vector<mojom::FileOpenResultPtr> results;
  bool handled = directory->OpenFileHandles(std::move(details), &results);
  EXPECT_TRUE(handled);
  EXPECT_EQ(2u, results.size());
  if (results.size() == 2u) {
    EXPECT_EQ("existing_file", results[0]->path);
    EXPECT_EQ(base::File::Error::FILE_OK, results[0]->error);
    EXPECT_TRUE(results[0]->file_handle.IsValid());

    EXPECT_EQ("existing_file", results[1]->path);
    EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, results[1]->error);
    EXPECT_FALSE(results[1]->file_handle.IsValid());
  }
}

TEST_F(ReadOnlyDirectoryImplTest, WriteFileRejected) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  base::File::Error error = base::File::Error::FILE_OK;
  std::vector<uint8_t> data = {0x1, 0x2, 0x3};
  bool handled = directory->WriteFile("existing_file", data, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);

  error = base::File::Error::FILE_OK;
  handled = directory->WriteFile("new_file", data, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);
}

TEST_F(ReadOnlyDirectoryImplTest, DeleteRejected) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  base::File::Error error = base::File::Error::FILE_OK;
  bool handled = directory->Delete("existing_file", 0, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);

  error = base::File::Error::FILE_OK;
  handled =
      directory->Delete("existing_dir", mojom::kDeleteFlagRecursive, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);
}

TEST_F(ReadOnlyDirectoryImplTest, RenameAndReplaceRejected) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  base::File::Error error = base::File::Error::FILE_OK;
  bool handled = directory->Rename("existing_file", "renamed_file", &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);

  error = base::File::Error::FILE_OK;
  handled = directory->Replace("existing_file", "renamed_file", &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);
}

TEST_F(ReadOnlyDirectoryImplTest, OpenDirectoryAndClonePropagation) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  mojo::Remote<mojom::Directory> sub_dir;
  base::File::Error error = base::File::Error::FILE_ERROR_FAILED;
  bool handled = directory->OpenDirectory(
      "existing_dir", sub_dir.BindNewPipeAndPassReceiver(),
      mojom::kFlagRead | mojom::kFlagOpen, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_OK, error);

  error = base::File::Error::FILE_OK;
  handled = sub_dir->Delete("sub_file", 0, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);

  mojo::Remote<mojom::Directory> cloned_dir;
  directory->Clone(cloned_dir.BindNewPipeAndPassReceiver());
  error = base::File::Error::FILE_OK;
  handled = cloned_dir->Delete("existing_file", 0, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);
}

TEST_F(ReadOnlyDirectoryImplTest, OpenDirectoryCreateRejected) {
  mojo::Remote<mojom::Directory> directory = CreateReadOnlyTempDir();
  mojo::Remote<mojom::Directory> new_dir;
  base::File::Error error = base::File::Error::FILE_OK;
  bool handled = directory->OpenDirectory(
      "new_dir", new_dir.BindNewPipeAndPassReceiver(),
      mojom::kFlagRead | mojom::kFlagWrite | mojom::kFlagCreate, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);

  error = base::File::Error::FILE_OK;
  new_dir.reset();
  handled = directory->OpenDirectory(
      "new_dir", new_dir.BindNewPipeAndPassReceiver(),
      mojom::kFlagRead | mojom::kFlagOpenAlways, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_SECURITY, error);

  error = base::File::Error::FILE_OK;
  new_dir.reset();
  handled =
      directory->OpenDirectory("new_dir", new_dir.BindNewPipeAndPassReceiver(),
                               mojom::kFlagRead | mojom::kFlagOpen, &error);
  EXPECT_TRUE(handled);
  EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND, error);
}

}  // namespace filesystem
