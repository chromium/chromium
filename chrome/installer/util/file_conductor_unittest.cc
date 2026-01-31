// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/file_conductor.h"

#include <windows.h>

#include <deque>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class FileConductorTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_PRED1(base::CreateDirectory, backup_path());
  }

  const base::FilePath& temp_path() const { return temp_dir_.GetPath(); }
  base::FilePath backup_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("backup"));
  }

  static std::deque<base::FilePath> DirectoryContents(
      const base::FilePath& path) {
    std::deque<base::FilePath> contents;
    base::FileEnumerator(path, /*recursive=*/false,
                         base::FileEnumerator::NAMES_ONLY)
        .ForEach([&contents](const base::FilePath& full_path) {
          contents.push_back(full_path.BaseName());
        });
    return contents;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

// Delete succeeds when there is nothing to delete.
TEST_F(FileConductorTest, DeleteAbsent) {
  base::FilePath not_exist = temp_path().Append(FILE_PATH_LITERAL("not_exist"));
  ASSERT_FALSE(base::PathExists(not_exist));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(not_exist));
    EXPECT_FALSE(base::PathExists(not_exist));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(not_exist));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());

  // Do it again, but now with Undo, which should also be a no-op.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(not_exist));
    file_conductor.Undo();
    EXPECT_FALSE(base::PathExists(not_exist));
  }

  // After cleanup following Undo.
  EXPECT_FALSE(base::PathExists(not_exist));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for a single file.
TEST_F(FileConductorTest, DeleteFileExists) {
  base::FilePath exists = temp_path().Append(FILE_PATH_LITERAL("exists"));
  ASSERT_TRUE(base::WriteFile(exists, base::as_byte_span(exists.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(exists));
    EXPECT_FALSE(base::PathExists(exists));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(exists));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());

  // Do it again, but now with Undo, which should recover the file.
  ASSERT_TRUE(base::WriteFile(exists, base::as_byte_span(exists.value())));
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(exists));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, exists);
  }

  // After cleanup following Undo.
  EXPECT_PRED1(base::PathExists, exists);
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for an empty directory.
TEST_F(FileConductorTest, DeleteEmptyDirectory) {
  base::FilePath empty_dir = temp_path().Append(FILE_PATH_LITERAL("empty_dir"));
  ASSERT_PRED1(base::CreateDirectory, empty_dir);

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(empty_dir));
    EXPECT_FALSE(base::PathExists(empty_dir));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(empty_dir));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());

  // Do it again, but now with Undo, which should recover the directory.
  ASSERT_PRED1(base::CreateDirectory, empty_dir);
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(empty_dir));
    file_conductor.Undo();
    EXPECT_PRED1(base::DirectoryExists, empty_dir);
  }

  // After cleanup following Undo.
  EXPECT_PRED1(base::DirectoryExists, empty_dir);
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for a directory with file(s).
TEST_F(FileConductorTest, DeleteDirectoryWithFiles) {
  base::FilePath non_empty_dir =
      temp_path().Append(FILE_PATH_LITERAL("non_empty_dir"));
  ASSERT_PRED1(base::CreateDirectory, non_empty_dir);
  base::FilePath file1 = non_empty_dir.Append(FILE_PATH_LITERAL("file1"));
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(non_empty_dir));
    EXPECT_FALSE(base::PathExists(non_empty_dir));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(non_empty_dir));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());

  // Do it again, but now with Undo, which should recover the directory.
  ASSERT_PRED1(base::CreateDirectory, non_empty_dir);
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(non_empty_dir));
    file_conductor.Undo();
    EXPECT_PRED1(base::DirectoryExists, non_empty_dir);
    EXPECT_PRED1(base::PathExists, file1);
  }

  // After cleanup following Undo.
  EXPECT_PRED1(base::DirectoryExists, non_empty_dir);
  EXPECT_PRED1(base::PathExists, file1);
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for a directory with an in-use file.
TEST_F(FileConductorTest, DeleteDirectoryWithInUseFile) {
  base::FilePath non_empty_dir =
      temp_path().Append(FILE_PATH_LITERAL("non_empty_dir"));
  ASSERT_PRED1(base::CreateDirectory, non_empty_dir);
  base::FilePath file1 = non_empty_dir.Append(FILE_PATH_LITERAL("file1"));
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));

  std::optional<base::MemoryMappedFile> mapped_file;
  ASSERT_TRUE(mapped_file.emplace().Initialize(
      base::File(file1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(non_empty_dir));
    EXPECT_FALSE(base::PathExists(non_empty_dir));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(non_empty_dir));

  // Do it again, but now with Undo, which should recover the directory.
  mapped_file.reset();
  ASSERT_PRED1(base::CreateDirectory, non_empty_dir);
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));
  ASSERT_TRUE(mapped_file.emplace().Initialize(
      base::File(file1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE)));
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(non_empty_dir));
    file_conductor.Undo();
    EXPECT_PRED1(base::DirectoryExists, non_empty_dir);
    EXPECT_PRED1(base::PathExists, file1);
  }

  // After cleanup following Undo.
  EXPECT_PRED1(base::DirectoryExists, non_empty_dir);
  EXPECT_PRED1(base::PathExists, file1);
}

// Delete fails for a directory with an immovable file.
TEST_F(FileConductorTest, DeleteDirectoryWithImmovbleFile) {
  base::FilePath non_empty_dir =
      temp_path().Append(FILE_PATH_LITERAL("non_empty_dir"));
  ASSERT_PRED1(base::CreateDirectory, non_empty_dir);
  base::FilePath file1 = non_empty_dir.Append(FILE_PATH_LITERAL("file1"));
  base::FilePath file2 = non_empty_dir.Append(FILE_PATH_LITERAL("file2"));
  base::FilePath file3 = non_empty_dir.Append(FILE_PATH_LITERAL("file3"));
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));
  ASSERT_TRUE(base::WriteFile(file2, base::as_byte_span(file2.value())));
  ASSERT_TRUE(base::WriteFile(file3, base::as_byte_span(file3.value())));

  std::optional<base::MemoryMappedFile> mapped_file;
  ASSERT_TRUE(mapped_file.emplace().Initialize(
      base::File(file2, base::File::FLAG_OPEN | base::File::FLAG_READ)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.DeleteEntry(non_empty_dir));
    EXPECT_THAT(DirectoryContents(non_empty_dir), Contains(file2.BaseName()));
  }

  // After cleanup without Undo.
  EXPECT_THAT(DirectoryContents(non_empty_dir), Contains(file2.BaseName()));

  // Do it again, but now with Undo, which should recover the directory.
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));
  ASSERT_TRUE(base::WriteFile(file3, base::as_byte_span(file3.value())));
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.DeleteEntry(non_empty_dir));
    file_conductor.Undo();
    EXPECT_THAT(DirectoryContents(non_empty_dir),
                UnorderedElementsAre(file1.BaseName(), file2.BaseName(),
                                     file3.BaseName()));
  }

  // After cleanup following Undo.
  EXPECT_THAT(DirectoryContents(non_empty_dir),
              UnorderedElementsAre(file1.BaseName(), file2.BaseName(),
                                   file3.BaseName()));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Delete fails when the backup directory cannot be used.
TEST_F(FileConductorTest, DeleteBackupUnusable) {
  base::FilePath file = temp_path().Append(FILE_PATH_LITERAL("file"));

  ASSERT_TRUE(base::WriteFile(file, base::as_byte_span(file.value())));

  {
    // Use a backup path that does not exist.
    FileConductor file_conductor(
        backup_path().Append(FILE_PATH_LITERAL("noexist")));
    ASSERT_FALSE(file_conductor.DeleteEntry(file));
    EXPECT_PRED1(base::PathExists, file);
  }

  // After cleanup without Undo.
  EXPECT_PRED1(base::PathExists, file);

  // Do it again, but now with Undo, which should recover the directory.
  {
    FileConductor file_conductor(
        backup_path().Append(FILE_PATH_LITERAL("noexist")));
    ASSERT_FALSE(file_conductor.DeleteEntry(file));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, file);
  }

  // After cleanup following Undo.
  EXPECT_PRED1(base::PathExists, file);
}

// Move fails on an empty source path.
TEST_F(FileConductorTest, MoveEmptySource) {
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry({}, destination));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(destination));

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry({}, destination));
    file_conductor.Undo();
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.
  EXPECT_FALSE(base::PathExists(destination));
  // Nothing in the backup directory.
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Move fails on an empty destination path.
TEST_F(FileConductorTest, MoveEmptyDestination) {
  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry(source, {}));
    EXPECT_TRUE(base::PathExists(source));
  }

  // After cleanup without Undo.
  EXPECT_TRUE(base::PathExists(source));

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry(source, {}));
    file_conductor.Undo();
    EXPECT_TRUE(base::PathExists(source));
  }

  // After cleanup following Undo.
  EXPECT_TRUE(base::PathExists(source));
  // Nothing in the backup directory.
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Delete fails for a non-existent source.
TEST_F(FileConductorTest, MoveSourceDoesNotExist) {
  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_FALSE(base::PathExists(destination));

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry(source, destination));
    file_conductor.Undo();
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_FALSE(base::PathExists(destination));
  // Nothing in the backup directory.
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Tests that a single file can be moved and undone when there are no conflicts.
TEST_F(FileConductorTest, MoveSingleFile) {
  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_PRED1(base::PathExists, destination);
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());

  // Do it again, but now with Undo.
  ASSERT_PRED1(base::DeleteFile, destination);
  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup with Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Move fails if the destination exists.
TEST_F(FileConductorTest, MoveDestinationExists) {
  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));
  ASSERT_TRUE(base::WriteFile(destination, base::as_byte_span(source.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry(source, destination));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_PRED1(base::PathExists, destination);

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup with Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_PRED1(base::PathExists, destination);
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Delete followed by Move works when a file in the destination is in-use.
TEST_F(FileConductorTest, DeleteAndMoveDirWithInUseFile) {
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));
  ASSERT_PRED1(base::CreateDirectory, destination);
  base::FilePath file1 = destination.Append(FILE_PATH_LITERAL("file1"));
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));
  std::optional<base::MemoryMappedFile> mapped_file;
  ASSERT_TRUE(mapped_file.emplace().Initialize(
      base::File(file1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE)));

  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  ASSERT_PRED1(base::CreateDirectory, source);
  base::FilePath source_file = source.Append(file1.BaseName());
  ASSERT_TRUE(
      base::WriteFile(source_file, base::as_byte_span(source_file.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(destination));
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_THAT(DirectoryContents(destination),
                UnorderedElementsAre(source_file.BaseName()));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_THAT(DirectoryContents(destination),
              UnorderedElementsAre(source_file.BaseName()));

  // Do it again, but now with Undo, which should recover the directory.
  mapped_file.reset();
  ASSERT_TRUE(mapped_file.emplace().Initialize(
      base::File(file1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE)));
  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source_file, base::as_byte_span(source_file.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(destination));
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    file_conductor.Undo();
    EXPECT_THAT(DirectoryContents(source),
                UnorderedElementsAre(source_file.BaseName()));
    EXPECT_THAT(DirectoryContents(destination),
                UnorderedElementsAre(source_file.BaseName()));
  }

  // After cleanup following Undo.
  EXPECT_THAT(DirectoryContents(source),
              UnorderedElementsAre(source_file.BaseName()));
  EXPECT_THAT(DirectoryContents(destination),
              UnorderedElementsAre(source_file.BaseName()));
}

// Tests that a single file can be moved and undone when it is open and mapped
// into a process.
TEST_F(FileConductorTest, MoveSingleFileSourceInUseMapped) {
  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));

  std::optional<base::MemoryMappedFile> mapped_source;
  ASSERT_TRUE(mapped_source.emplace().Initialize(
      base::File(source, base::File::FLAG_OPEN | base::File::FLAG_READ |
                             base::File::FLAG_WIN_SHARE_DELETE)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_PRED1(base::PathExists, destination);

  // Do it again, but now with Undo, which should recover the file.
  mapped_source.reset();
  ASSERT_PRED1(base::DeleteFile, destination);
  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));
  ASSERT_TRUE(mapped_source.emplace().Initialize(
      base::File(source, base::File::FLAG_OPEN | base::File::FLAG_READ |
                             base::File::FLAG_WIN_SHARE_DELETE)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup with Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Tests that a directory can be moved when there are no conflicts.
TEST_F(FileConductorTest, MoveDirectoryNoConflict) {
  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath file1(FILE_PATH_LITERAL("file1"));
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source.Append(file1), base::as_byte_span(file1.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_THAT(DirectoryContents(destination),
                UnorderedElementsAre(file1.BaseName()));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_THAT(DirectoryContents(destination),
              UnorderedElementsAre(file1.BaseName()));

  // Do it again, but now with Undo.
  ASSERT_PRED1(base::DeletePathRecursively, destination);
  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source.Append(file1), base::as_byte_span(file1.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.

  EXPECT_PRED1(base::PathExists, source);
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

// Tests that a source directory with an in-use file can be moved.
TEST_F(FileConductorTest, MoveDirectorySourceInUse) {
  base::FilePath source = temp_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath file1(FILE_PATH_LITERAL("file1"));
  base::FilePath destination =
      temp_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source.Append(file1), base::as_byte_span(file1.value())));

  std::optional<base::MemoryMappedFile> mapped_source;
  ASSERT_TRUE(mapped_source.emplace().Initialize(base::File(
      source.Append(file1), base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WIN_SHARE_DELETE)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_THAT(DirectoryContents(destination), UnorderedElementsAre(file1));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_THAT(DirectoryContents(destination), UnorderedElementsAre(file1));

  // Do it again, but now with Undo, which should recover the directory.
  mapped_source.reset();
  ASSERT_PRED1(base::DeletePathRecursively, destination);
  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source.Append(file1), base::as_byte_span(file1.value())));
  ASSERT_TRUE(mapped_source.emplace().Initialize(base::File(
      source.Append(file1), base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WIN_SHARE_DELETE)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    file_conductor.Undo();
    EXPECT_THAT(DirectoryContents(source), UnorderedElementsAre(file1));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.
  EXPECT_THAT(DirectoryContents(source), UnorderedElementsAre(file1));
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(DirectoryContents(backup_path()), IsEmpty());
}

}  // namespace installer
