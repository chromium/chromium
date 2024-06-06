// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_util.h"

#include <limits>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/reporting/storage/storage_configuration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::SizeIs;

namespace reporting {

namespace {

class StorageDirectoryTest : public ::testing::Test {
 public:
  StorageDirectoryTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
    storage_options_.set_directory(location_.GetPath());
  }

  // Returns a randomly generated GUID.
  static GenerationGuid CreateGenerationGuid() {
    return base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

  // Creates an empty metadata file in `queue_directory`.
  static void CreateMetaDataFileInDirectory(
      const base::FilePath queue_directory) {
    CHECK(base::DirectoryExists(queue_directory));

    const auto meta_file_path =
        queue_directory.Append(StorageDirectory::kMetadataFileNamePrefix);
    auto file = base::File(meta_file_path,
                           base::File::Flags::FLAG_CREATE_ALWAYS |
                               base::File::FLAG_WRITE | base::File::FLAG_READ);
    CHECK(file.created());
    CHECK(file.IsValid());
    CHECK(PathExists(meta_file_path));
  }

  // Creates a record file with zero size in `queue_directory` and returns the
  // filepath. In the context of `StorageDirectory`, a record file is just a
  // non-metadata file.
  static base::FilePath CreateEmptyRecordFileInDirectory(
      const base::FilePath queue_directory) {
    base::FilePath file_path;
    CHECK(base::CreateTemporaryFileInDir(queue_directory, &file_path));
    return file_path;
  }

  // Creates a record file with non-zero size and returns the filepath. In the
  // context of `StorageDirectory`, a record file is just a non-metadata file.
  static void CreateRecordFileInDirectory(
      const base::FilePath queue_directory) {
    auto file_path = CreateEmptyRecordFileInDirectory(queue_directory);
    base::AppendToFile(file_path, "data");
  }

  // Returns the full path for a queue directory of some priority - caller
  // should not care which priority.
  base::FilePath queue_directory() const {
    const auto [_, queue_options] =
        storage_options_.ProduceQueuesOptionsList()[0];
    return queue_options.directory();
  }

  // Returns the full path for a legacy queue directory.
  base::FilePath GetLegacyQueueDirectoryPath() const {
    return queue_directory().RemoveExtension();
  }

  // Returns the full path for a multigenerational queue directory.
  base::FilePath GetMultigenerationQueueDirectoryPath() const {
    return queue_directory().RemoveExtension().AddExtensionASCII(
        CreateGenerationGuid());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir location_;
  StorageOptions storage_options_;
};

TEST_F(StorageDirectoryTest, MultigenerationQueueDirectoriesAreFound) {
  const auto queue_options_list = storage_options_.ProduceQueuesOptionsList();

  StorageDirectory::Set expected_priority_generation_guid_pairs;

  // Create a queue directory for each priority
  for (const auto& [priority, queue_options] : queue_options_list) {
    // Create multigenerational queue directory filepath. Multigenerational
    // queues have a generation guid as an extension, e.g.
    // foo/bar/FastBatch.JsK32KLs. Remove any existing extension first so that
    // we are certain what the extension is and then add a generation guid as
    // the extension.
    const GenerationGuid generation_guid = CreateGenerationGuid();
    const auto queue_directory_filepath =
        queue_options.directory().RemoveExtension().AddExtensionASCII(
            generation_guid);

    // Create multigenerational queue directory
    ASSERT_TRUE(base::CreateDirectory(queue_directory_filepath));

    expected_priority_generation_guid_pairs.emplace(
        std::make_tuple(priority, generation_guid));
  }

  const auto kExpectedNumQueueDirectories = queue_options_list.size();
  const auto priority_generation_guid_pairs =
      StorageDirectory::FindQueueDirectories(
          storage_options_.directory(),
          storage_options_.ProduceQueuesOptionsList());

  EXPECT_THAT(priority_generation_guid_pairs,
              SizeIs(kExpectedNumQueueDirectories));
  EXPECT_THAT(priority_generation_guid_pairs,
              Eq(expected_priority_generation_guid_pairs));
}

TEST_F(StorageDirectoryTest, LegacyQueueDirectoriesAreFound) {
  const auto queue_options = storage_options_.ProduceQueuesOptionsList();
  StorageDirectory::Set expected_priority_generation_guid_pairs;

  // Create a queue directory for each priority
  for (const auto& [priority, options] : queue_options) {
    // Create legacy queue directory filepath. These filepaths do not have
    // generation guid extensions, e.g. foo/bar/Security as opposed to
    // foo/bar/Security.XHf45KT
    const auto legacy_queue_filepath = options.directory().RemoveExtension();

    // Create legacy queue directory
    ASSERT_TRUE(base::CreateDirectory(legacy_queue_filepath));

    // Generation guid should be empty string
    expected_priority_generation_guid_pairs.emplace(
        std::make_tuple(priority, ""));
  }

  const auto kExpectedNumLegacyQueueDirectories = queue_options.size();
  const auto priority_generation_guid_pairs =
      StorageDirectory::FindQueueDirectories(
          storage_options_.directory(),
          storage_options_.ProduceQueuesOptionsList());

  EXPECT_THAT(priority_generation_guid_pairs,
              SizeIs(kExpectedNumLegacyQueueDirectories));
  EXPECT_THAT(priority_generation_guid_pairs,
              Eq(expected_priority_generation_guid_pairs));
}

TEST_F(StorageDirectoryTest, MixedQueueDirectoriesAreFound) {
  // Create a multigenerational queue directory
  const base::FilePath multigenerational_queue_directory_path =
      GetMultigenerationQueueDirectoryPath();
  ASSERT_TRUE(base::CreateDirectory(multigenerational_queue_directory_path));

  // Create a legacy queue directory
  const base::FilePath legacy_queue_directory_path =
      GetLegacyQueueDirectoryPath();
  ASSERT_TRUE(base::CreateDirectory(legacy_queue_directory_path));

  const int expected_num_queue_directories = 2;
  const auto priority_generation_guid_pairs =
      StorageDirectory::FindQueueDirectories(
          storage_options_.directory(),
          storage_options_.ProduceQueuesOptionsList());

  EXPECT_THAT(priority_generation_guid_pairs,
              SizeIs(expected_num_queue_directories));
}

TEST_F(StorageDirectoryTest, EmptyLegacyQueueDirectoriesAreNotDeleted) {
  // Create a legacy queue directory
  const base::FilePath legacy_queue_directory_path =
      GetLegacyQueueDirectoryPath();
  ASSERT_TRUE(base::CreateDirectory(legacy_queue_directory_path));

  // Fill the legacy queue directories such it represents a queue
  // which has sent some records and all of them have been confirmed by the
  // server.
  CreateEmptyRecordFileInDirectory(legacy_queue_directory_path);
  CreateMetaDataFileInDirectory(legacy_queue_directory_path);

  EXPECT_TRUE(StorageDirectory::DeleteEmptyMultigenerationQueueDirectories(
      storage_options_.directory()));

  // Shouldn't have been deleted.
  EXPECT_TRUE(base::DirectoryExists(legacy_queue_directory_path));

  const int expected_num_queue_directories = 1;
  const auto priority_generation_guid_pairs =
      StorageDirectory::FindQueueDirectories(
          storage_options_.directory(),
          storage_options_.ProduceQueuesOptionsList());
  EXPECT_THAT(priority_generation_guid_pairs,
              SizeIs(expected_num_queue_directories));
}

// Verifies that multigenerational queue directories are deleted when they
// contain no unconfirmed records.
TEST_F(StorageDirectoryTest, EmptyMultigenerationQueueDirectoriesAreDeleted) {
  // Create a multigenerational queue directory
  const base::FilePath multigenerational_queue_directory_path =
      GetMultigenerationQueueDirectoryPath();
  ASSERT_TRUE(base::CreateDirectory(multigenerational_queue_directory_path));

  // Fill the multigenerational queue directories such it represents a queue
  // which has sent some records and all of them have been confirmed by the
  // server.
  CreateEmptyRecordFileInDirectory(multigenerational_queue_directory_path);
  CreateMetaDataFileInDirectory(multigenerational_queue_directory_path);

  EXPECT_TRUE(StorageDirectory::DeleteEmptyMultigenerationQueueDirectories(
      storage_options_.directory()));

  // `multigenerational_queue_directory_path` should be deleted
  EXPECT_FALSE(base::DirectoryExists(multigenerational_queue_directory_path));

  // We should find zero queue directories
  const int expected_num_queue_directories = 0;
  const auto priority_generation_guid_pairs =
      StorageDirectory::FindQueueDirectories(
          storage_options_.directory(),
          storage_options_.ProduceQueuesOptionsList());
  EXPECT_THAT(priority_generation_guid_pairs,
              SizeIs(expected_num_queue_directories));
}

TEST_F(StorageDirectoryTest,
       MultigenerationQueueDirectoriesWithRecordsAreNotDeleted) {
  // Create a multigenerational queue directory.
  const base::FilePath multigenerational_queue_directory_path =
      GetMultigenerationQueueDirectoryPath();
  ASSERT_TRUE(base::CreateDirectory(multigenerational_queue_directory_path));

  // Fill the multigenerational queue directories such that it represents a
  // queue which has unconfirmed records, i.e. at least one non-empty record
  // file.
  CreateRecordFileInDirectory(multigenerational_queue_directory_path);
  CreateEmptyRecordFileInDirectory(multigenerational_queue_directory_path);
  CreateMetaDataFileInDirectory(multigenerational_queue_directory_path);
  ASSERT_FALSE(base::IsDirectoryEmpty(multigenerational_queue_directory_path));

  EXPECT_TRUE(StorageDirectory::DeleteEmptyMultigenerationQueueDirectories(
      storage_options_.directory()));

  // `multigenerational_queue_directory_path` should not have been deleted since
  // it contains a record with data.
  EXPECT_TRUE(base::DirectoryExists(multigenerational_queue_directory_path));
  const int expected_num_queue_directories = 1;
  const auto priority_generation_guid_pairs =
      StorageDirectory::FindQueueDirectories(
          storage_options_.directory(),
          storage_options_.ProduceQueuesOptionsList());
  EXPECT_THAT(priority_generation_guid_pairs,
              SizeIs(expected_num_queue_directories));
}

}  // namespace
}  // namespace reporting
