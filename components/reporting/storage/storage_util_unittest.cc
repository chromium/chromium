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

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir location_;
  StorageOptions storage_options_;
};
}  // namespace
}  // namespace reporting
