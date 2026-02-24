// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/file_conductor.h"

#include <windows.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace installer {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

std::vector<base::FilePath> GetDirectoryContents(const base::FilePath& path) {
  std::vector<base::FilePath> contents;
  base::FileEnumerator(path, /*recursive=*/false,
                       base::FileEnumerator::NAMES_ONLY)
      .ForEach([&contents](const base::FilePath& full_path) {
        contents.push_back(full_path.BaseName());
      });
  return contents;
}

struct VolumeHandleDeleter {
  void operator()(void* handle) const {
    if (handle != INVALID_HANDLE_VALUE) {
      ::FindVolumeClose(handle);
    }
  }
};

using ScopedVolumeHandle = std::unique_ptr<void, VolumeHandleDeleter>;

// Returns a path that resides on a different volume than `path`, or an empty
// path if none can be found.
base::FilePath GetOtherVolume(const base::FilePath& path) {
  // Get the drive letter from `path` with a trailing separator.
  base::FilePath::StringType this_drive_letter;
  if (auto components = path.GetComponents(); !components.empty()) {
    this_drive_letter =
        base::StrCat({base::ToUpperASCII(components[0]), L"\\"});
  } else {
    return {};
  }

  // Build mappings of all mount points to their respective volume and of each
  // volume to all of its mount points.
  absl::flat_hash_map<std::wstring, std::wstring> mount_to_volume;
  absl::flat_hash_map<std::wstring, std::vector<std::wstring>> volume_to_mounts;

  // A volume GUID path is of the form
  // "\\?\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\".
  static constexpr size_t kGuidPathLength = 50;
  std::array<wchar_t, kGuidPathLength> guid_buffer;
  if (ScopedVolumeHandle scan(
          ::FindFirstVolume(guid_buffer.data(), guid_buffer.size()));
      scan.get() != INVALID_HANDLE_VALUE) {
    const std::wstring_view volume_guid(guid_buffer.data());
    do {
      std::array<wchar_t, MAX_PATH> mount_point;
      DWORD num_chars = 0;
      if (::GetVolumePathNamesForVolumeName(
              volume_guid.data(), mount_point.data(), mount_point.size(),
              &num_chars) != 0) {
        auto char_span = base::span(mount_point).first(num_chars);
        while (!char_span.empty() && char_span.front()) {
          size_t len = std::ranges::find(char_span, L'\0') - char_span.begin();
          std::wstring_view path_name(char_span.data(), len);
          mount_to_volume.try_emplace(path_name, volume_guid);
          volume_to_mounts[volume_guid].emplace_back(path_name);
          char_span = char_span.subspan(base::MakeStrictNum(len + 1));
        }
      }
    } while (::FindNextVolume(scan.get(), guid_buffer.data(),
                              guid_buffer.size()) != 0);
  }

  // Which volume is `path` on?
  if (auto this_volume_it = mount_to_volume.find(this_drive_letter);
      this_volume_it != mount_to_volume.end()) {
    // Find any other mount point that isn't on this same volume.
    for (const auto& [volume, mounts] : volume_to_mounts) {
      if (volume != this_volume_it->second) {
        return base::FilePath(mounts.front());
      }
    }
  }

  return {};
}

enum class TestVariant {
  kSingleVolume,            // source, backup, destination on one volume.
  kCrossVolumeBackup,       // backup on a different volume.
  kCrossVolumeDestination,  // destination on a different volume.
};

void PrintTo(TestVariant variant, std::ostream* os) {
  switch (variant) {
    case TestVariant::kSingleVolume:
      *os << "SingleVolume";
      break;
    case TestVariant::kCrossVolumeBackup:
      *os << "CrossVolumeBackup";
      break;
    case TestVariant::kCrossVolumeDestination:
      *os << "CrossVolumeDestination";
      break;
  }
}

// A test fixture that prepares two directories for use by tests.
class FileConductorTest : public testing::TestWithParam<TestVariant> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    source_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("source"));
    ASSERT_PRED1(base::CreateDirectory, source_path_);

    switch (GetParam()) {
      case TestVariant::kSingleVolume:
        // The backup and destination directories are on the same volume as the
        // source.
        backup_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("backup"));
        ASSERT_PRED1(base::CreateDirectory, backup_path_);
        destination_path_ =
            temp_dir_.GetPath().Append(FILE_PATH_LITERAL("destination"));
        ASSERT_PRED1(base::CreateDirectory, destination_path_);
        break;

      case TestVariant::kCrossVolumeBackup:
      case TestVariant::kCrossVolumeDestination:
        // Either the backup or the destination directory is on a different
        // volume than the source.
        if (const base::FilePath other_volume =
                GetOtherVolume(temp_dir_.GetPath());
            !other_volume.empty()) {
          ASSERT_TRUE(other_volume_temp_dir_.CreateUniqueTempDirUnderPath(
              other_volume));
        } else {
          GTEST_SKIP() << "Cross-volume tests require a device with two or "
                          "more volumes.";
        }
        if (GetParam() == TestVariant::kCrossVolumeBackup) {
          backup_path_ = other_volume_temp_dir_.GetPath().Append(
              FILE_PATH_LITERAL("backup"));
          ASSERT_PRED1(base::CreateDirectory, backup_path_);
          destination_path_ =
              temp_dir_.GetPath().Append(FILE_PATH_LITERAL("destination"));
          ASSERT_PRED1(base::CreateDirectory, destination_path_);
        } else {
          backup_path_ =
              temp_dir_.GetPath().Append(FILE_PATH_LITERAL("backup"));
          ASSERT_PRED1(base::CreateDirectory, backup_path_);
          destination_path_ = other_volume_temp_dir_.GetPath().Append(
              FILE_PATH_LITERAL("destination"));
          ASSERT_PRED1(base::CreateDirectory, destination_path_);
        }
        break;
    }
  }

  const base::FilePath& source_path() const { return source_path_; }
  const base::FilePath& backup_path() const { return backup_path_; }
  const base::FilePath& destination_path() const { return destination_path_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir other_volume_temp_dir_;
  base::FilePath source_path_;
  base::FilePath backup_path_;
  base::FilePath destination_path_;
};

// Delete succeeds when there is nothing to delete.
TEST_P(FileConductorTest, DeleteAbsent) {
  base::FilePath not_exist =
      source_path().Append(FILE_PATH_LITERAL("not_exist"));
  ASSERT_FALSE(base::PathExists(not_exist));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(not_exist));
    EXPECT_FALSE(base::PathExists(not_exist));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(not_exist));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

  // Do it again, but now with Undo, which should also be a no-op.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(not_exist));
    file_conductor.Undo();
    EXPECT_FALSE(base::PathExists(not_exist));
  }

  // After cleanup following Undo.
  EXPECT_FALSE(base::PathExists(not_exist));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for a single file.
TEST_P(FileConductorTest, DeleteFileExists) {
  base::FilePath exists = source_path().Append(FILE_PATH_LITERAL("exists"));
  ASSERT_TRUE(base::WriteFile(exists, base::as_byte_span(exists.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(exists));
    EXPECT_FALSE(base::PathExists(exists));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(exists));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for an empty directory.
TEST_P(FileConductorTest, DeleteEmptyDirectory) {
  base::FilePath empty_dir =
      source_path().Append(FILE_PATH_LITERAL("empty_dir"));
  ASSERT_PRED1(base::CreateDirectory, empty_dir);

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(empty_dir));
    EXPECT_FALSE(base::PathExists(empty_dir));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(empty_dir));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for a directory with file(s).
TEST_P(FileConductorTest, DeleteDirectoryWithFiles) {
  base::FilePath non_empty_dir =
      source_path().Append(FILE_PATH_LITERAL("non_empty_dir"));
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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for a directory with directories and files.
TEST_P(FileConductorTest, DeleteDirectoryWithDirectoriesAndFiles) {
  // Create source/non_empty_dir/file1
  base::FilePath non_empty_dir =
      source_path().Append(FILE_PATH_LITERAL("non_empty_dir"));
  ASSERT_PRED1(base::CreateDirectory, non_empty_dir);
  base::FilePath file1 = non_empty_dir.Append(FILE_PATH_LITERAL("file1"));
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));

  // Create source/non_empty_dir/other_dir/file2
  base::FilePath other_dir =
      non_empty_dir.Append(FILE_PATH_LITERAL("other_dir"));
  ASSERT_PRED1(base::CreateDirectory, other_dir);
  base::FilePath file2 = other_dir.Append(FILE_PATH_LITERAL("file2"));
  ASSERT_TRUE(base::WriteFile(file2, base::as_byte_span(file2.value())));

  // Delete non_empty_dir and undo it.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(non_empty_dir));
    EXPECT_FALSE(base::PathExists(non_empty_dir));
    file_conductor.Undo();
  }

  // After cleanup following Undo.
  EXPECT_PRED1(base::DirectoryExists, non_empty_dir);
  EXPECT_PRED1(base::PathExists, file1);
  EXPECT_PRED1(base::DirectoryExists, other_dir);
  EXPECT_PRED1(base::PathExists, file2);
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

  // Do it again without undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(non_empty_dir));
    EXPECT_FALSE(base::PathExists(non_empty_dir));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(non_empty_dir));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete succeeds for a directory with an in-use file.
TEST_P(FileConductorTest, DeleteDirectoryWithInUseFile) {
  base::FilePath non_empty_dir =
      source_path().Append(FILE_PATH_LITERAL("non_empty_dir"));
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
TEST_P(FileConductorTest, DeleteDirectoryWithImmovbleFile) {
  base::FilePath non_empty_dir =
      source_path().Append(FILE_PATH_LITERAL("non_empty_dir"));
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
    EXPECT_THAT(GetDirectoryContents(non_empty_dir),
                Contains(file2.BaseName()));
  }

  // After cleanup without Undo.
  EXPECT_THAT(GetDirectoryContents(non_empty_dir), Contains(file2.BaseName()));

  // Do it again, but now with Undo, which should recover the directory.
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));
  ASSERT_TRUE(base::WriteFile(file3, base::as_byte_span(file3.value())));
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.DeleteEntry(non_empty_dir));
    file_conductor.Undo();
    EXPECT_THAT(GetDirectoryContents(non_empty_dir),
                UnorderedElementsAre(file1.BaseName(), file2.BaseName(),
                                     file3.BaseName()));
  }

  // After cleanup following Undo.
  EXPECT_THAT(GetDirectoryContents(non_empty_dir),
              UnorderedElementsAre(file1.BaseName(), file2.BaseName(),
                                   file3.BaseName()));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete fails when the backup directory cannot be used.
TEST_P(FileConductorTest, DeleteBackupUnusable) {
  base::FilePath file = source_path().Append(FILE_PATH_LITERAL("file"));

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
TEST_P(FileConductorTest, MoveEmptySource) {
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Move fails on an empty destination path.
TEST_P(FileConductorTest, MoveEmptyDestination) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete fails for a non-existent source.
TEST_P(FileConductorTest, MoveSourceDoesNotExist) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a single file can be moved and undone when there are no conflicts.
TEST_P(FileConductorTest, MoveSingleFile) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a single read-only file can be moved and undone when there are no
// conflicts.
TEST_P(FileConductorTest, MoveSingleReadOnlyFile) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));
  auto attributes = ::GetFileAttributes(source.value().c_str());
  ASSERT_NE(attributes, INVALID_FILE_ATTRIBUTES);
  ASSERT_NE(::SetFileAttributes(source.value().c_str(),
                                attributes | FILE_ATTRIBUTE_READONLY),
            0);

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_PRED1(base::PathExists, destination);
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Move fails if the destination exists.
TEST_P(FileConductorTest, MoveDestinationExists) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Delete followed by Move works when a file in the destination is in-use.
TEST_P(FileConductorTest, DeleteAndMoveDirWithInUseFile) {
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));
  ASSERT_PRED1(base::CreateDirectory, destination);
  base::FilePath file1 = destination.Append(FILE_PATH_LITERAL("file1"));
  ASSERT_TRUE(base::WriteFile(file1, base::as_byte_span(file1.value())));
  std::optional<base::MemoryMappedFile> mapped_file;
  ASSERT_TRUE(mapped_file.emplace().Initialize(
      base::File(file1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE)));

  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  ASSERT_PRED1(base::CreateDirectory, source);
  base::FilePath source_file = source.Append(file1.BaseName());
  ASSERT_TRUE(
      base::WriteFile(source_file, base::as_byte_span(source_file.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.DeleteEntry(destination));
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_THAT(GetDirectoryContents(destination),
                UnorderedElementsAre(source_file.BaseName()));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_THAT(GetDirectoryContents(destination),
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
    EXPECT_THAT(GetDirectoryContents(source),
                UnorderedElementsAre(source_file.BaseName()));
    EXPECT_THAT(GetDirectoryContents(destination),
                UnorderedElementsAre(source_file.BaseName()));
  }

  // After cleanup following Undo.
  EXPECT_THAT(GetDirectoryContents(source),
              UnorderedElementsAre(source_file.BaseName()));
  EXPECT_THAT(GetDirectoryContents(destination),
              UnorderedElementsAre(source_file.BaseName()));
}

// Tests that a single file can be moved and undone when it is open and mapped
// into a process.
TEST_P(FileConductorTest, MoveSingleFileSourceInUseMapped) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a directory can be moved when there are no conflicts.
TEST_P(FileConductorTest, MoveDirectoryNoConflict) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath file1(FILE_PATH_LITERAL("file1"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source.Append(file1), base::as_byte_span(file1.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_THAT(GetDirectoryContents(destination),
                UnorderedElementsAre(file1.BaseName()));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_THAT(GetDirectoryContents(destination),
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
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a source directory with an in-use file can be moved.
TEST_P(FileConductorTest, MoveDirectorySourceInUse) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath file1(FILE_PATH_LITERAL("file1"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

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
    EXPECT_THAT(GetDirectoryContents(destination), UnorderedElementsAre(file1));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_THAT(GetDirectoryContents(destination), UnorderedElementsAre(file1));

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
    EXPECT_THAT(GetDirectoryContents(source), UnorderedElementsAre(file1));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.
  EXPECT_THAT(GetDirectoryContents(source), UnorderedElementsAre(file1));
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a single file cannot be moved when it cannot be deleted in strict
// mode.
TEST_P(FileConductorTest, MoveSingleFileCantDeleteSource) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));

  std::optional<base::MemoryMappedFile> mapped_source;
  ASSERT_TRUE(mapped_source.emplace().Initialize(
      base::File(source, base::File::FLAG_OPEN | base::File::FLAG_READ)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.MoveEntry(source, destination));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
    file_conductor.Undo();
  }

  // After cleanup following undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_FALSE(base::PathExists(destination));
}

// Tests that a single file can be moved when it cannot be deleted in lenient
// mode.
TEST_P(FileConductorTest, MoveSingleFileCantDeleteSourceLenient) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));

  std::optional<base::MemoryMappedFile> mapped_source;
  ASSERT_TRUE(mapped_source.emplace().Initialize(
      base::File(source, base::File::FLAG_OPEN | base::File::FLAG_READ)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination,
                                         /*lenient_deletion=*/true));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
    file_conductor.Undo();
  }

  // After cleanup following Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_FALSE(base::PathExists(destination));

  // Do it again without undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.MoveEntry(source, destination,
                                         /*lenient_deletion=*/true));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_PRED1(base::PathExists, destination);
}

// Copy fails on an empty source path
TEST_P(FileConductorTest, CopyEntryEmpty) {
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry({}, destination));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(destination));

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry({}, destination));
    file_conductor.Undo();
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.
  EXPECT_FALSE(base::PathExists(destination));
  // Nothing in the backup directory.
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Copy fails on an empty destination path
TEST_P(FileConductorTest, CopyEntryEmptyDestination) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry(source, {}));
    EXPECT_TRUE(base::PathExists(source));
  }

  // After cleanup without Undo.
  EXPECT_TRUE(base::PathExists(source));

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry(source, {}));
    file_conductor.Undo();
    EXPECT_TRUE(base::PathExists(source));
  }

  // After cleanup following Undo.
  EXPECT_TRUE(base::PathExists(source));
  // Nothing in the backup directory.
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Copy fails for a non-existent source.
TEST_P(FileConductorTest, CopySourceDoesNotExist) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry(source, destination));
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup without Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_FALSE(base::PathExists(destination));

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry(source, destination));
    file_conductor.Undo();
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_FALSE(base::PathExists(destination));
  // Nothing in the backup directory.
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a single file can be copied and undone when there are no
// conflicts.
TEST_P(FileConductorTest, CopySingleFile) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without Undo.

  EXPECT_PRED1(base::PathExists, source);
  EXPECT_PRED1(base::PathExists, destination);
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());

  // Do it again, but now with Undo.
  ASSERT_PRED1(base::DeleteFile, destination);
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup with Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Copy fails if the destination exists.
TEST_P(FileConductorTest, CopyDestinationExists) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));
  ASSERT_TRUE(base::WriteFile(destination, base::as_byte_span(source.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry(source, destination));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_PRED1(base::PathExists, destination);

  // Do it again, but now with Undo.
  {
    FileConductor file_conductor(backup_path());
    ASSERT_FALSE(file_conductor.CopyEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup with Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_PRED1(base::PathExists, destination);
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Copy succeeds if the source in in use.
TEST_P(FileConductorTest, CopyInUse) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  ASSERT_TRUE(base::WriteFile(source, base::as_byte_span(source.value())));
  std::optional<base::MemoryMappedFile> mapped_source;
  ASSERT_TRUE(mapped_source.emplace().Initialize(
      base::File(source, base::File::FLAG_OPEN | base::File::FLAG_READ |
                             base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                             base::File::FLAG_WIN_SHARE_DELETE)));

  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_PRED1(base::PathExists, destination);
  }

  // After cleanup without Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_PRED1(base::PathExists, destination);

  // Do it again, but now with Undo.
  ASSERT_PRED1(base::DeleteFile, destination);
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup with Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a directory can be copied when there are no conflicts.
TEST_P(FileConductorTest, CopyDirectoryNoConflict) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath file1(FILE_PATH_LITERAL("file1"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source.Append(file1), base::as_byte_span(file1.value())));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_THAT(GetDirectoryContents(destination),
                UnorderedElementsAre(file1.BaseName()));
    EXPECT_THAT(base::ReadFileToBytes(source.Append(file1)),
                Eq(base::ReadFileToBytes(destination.Append(file1))));
  }

  // After cleanup without Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_THAT(GetDirectoryContents(destination), UnorderedElementsAre(file1));

  // Do it again, but now with Undo.
  ASSERT_PRED1(base::DeletePathRecursively, destination);
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    file_conductor.Undo();
    EXPECT_PRED1(base::PathExists, source);
    EXPECT_FALSE(base::PathExists(destination));
  }

  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Tests that a source directory with an in-use file can be copied.
TEST_P(FileConductorTest, CopyDirectorySourceInUse) {
  base::FilePath source = source_path().Append(FILE_PATH_LITERAL("source"));
  base::FilePath file1(FILE_PATH_LITERAL("file1"));
  base::FilePath destination =
      destination_path().Append(FILE_PATH_LITERAL("destination"));

  ASSERT_PRED1(base::CreateDirectory, source);
  ASSERT_TRUE(
      base::WriteFile(source.Append(file1), base::as_byte_span(file1.value())));

  std::optional<base::MemoryMappedFile> mapped_source;
  ASSERT_TRUE(mapped_source.emplace().Initialize(base::File(
      source.Append(file1), base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                base::File::FLAG_WIN_SHARE_DELETE)));

  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    EXPECT_THAT(GetDirectoryContents(destination),
                UnorderedElementsAre(file1.BaseName()));
    EXPECT_THAT(base::ReadFileToBytes(source.Append(file1)),
                Eq(base::ReadFileToBytes(destination.Append(file1))));
  }

  // After cleanup without Undo.
  EXPECT_PRED1(base::PathExists, source);
  EXPECT_THAT(GetDirectoryContents(destination), UnorderedElementsAre(file1));

  // Do it again, but now with Undo, which should recover the directory.
  ASSERT_PRED1(base::DeletePathRecursively, destination);
  {
    FileConductor file_conductor(backup_path());
    ASSERT_TRUE(file_conductor.CopyEntry(source, destination));
    file_conductor.Undo();
    EXPECT_THAT(GetDirectoryContents(source), UnorderedElementsAre(file1));
    EXPECT_FALSE(base::PathExists(destination));
  }

  // After cleanup following Undo.
  EXPECT_THAT(GetDirectoryContents(source), UnorderedElementsAre(file1));
  EXPECT_FALSE(base::PathExists(destination));
  EXPECT_THAT(GetDirectoryContents(backup_path()), IsEmpty());
}

// Run all tests three times -- once with both directories on the same volume,
// once with the backup directory on a different volume (if one is found), and
// once with the destination directory on a different volume (if one is found).
INSTANTIATE_TEST_SUITE_P(
    ,
    FileConductorTest,
    ::testing::Values(TestVariant::kSingleVolume,
                      TestVariant::kCrossVolumeBackup,
                      TestVariant::kCrossVolumeDestination),
    ::testing::PrintToStringParamName());

}  // namespace installer
