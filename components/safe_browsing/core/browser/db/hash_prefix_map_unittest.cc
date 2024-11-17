// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"

#include <type_traits>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace safe_browsing {
namespace {

using ::testing::ElementsAre;

class HashPrefixMapTest : public PlatformTest {
 public:
  HashPrefixMapTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  std::string GetContents(const std::string& extension) {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(GetPath(extension), &contents));
    return contents;
  }

  base::FilePath GetPath(const std::string& extension) {
    return GetBasePath().AddExtensionASCII(extension);
  }

  base::FilePath GetBasePath() {
    return temp_dir_.GetPath().AppendASCII("HashPrefixMapTest");
  }

  std::string StringWithLeadingBytes(uint8_t byte1, uint8_t byte2 = 0) {
    std::string s;
    s.push_back(byte1);
    s.push_back(byte2);
    s.append("ab");
    return s;
  }

  base::Time time_ = base::Time::Now();
  base::ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_env_;
};

TEST_F(HashPrefixMapTest, WriteFile) {
  HashPrefixMap map(GetBasePath());
  map.Append(4, "fooo");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  EXPECT_EQ(file_format.hash_files().size(), 1);
  const auto& hash_file = file_format.hash_files(0);
  EXPECT_EQ(hash_file.prefix_size(), 4);
  EXPECT_EQ(GetContents(hash_file.extension()), "fooo");

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

TEST_F(HashPrefixMapTest, FailedWrite) {
  HashPrefixMap map(
      GetBasePath().AppendASCII("bad_dir").AppendASCII("some.store"));
  map.Append(4, "foo");

  V4StoreFileFormat file_format;
  EXPECT_FALSE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), MMAP_FAILURE);
}

TEST_F(HashPrefixMapTest, WriteMultipleFiles) {
  HashPrefixMap map(GetBasePath());
  map.Append(4, "fooo");
  map.Append(2, "ba");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  auto hash_files = file_format.hash_files();
  base::ranges::sort(hash_files, {}, &HashFile::prefix_size);
  EXPECT_EQ(hash_files.size(), 2);

  const auto& file1 = file_format.hash_files(0);
  EXPECT_EQ(file1.prefix_size(), 2);
  EXPECT_EQ(GetContents(file1.extension()), "ba");

  const auto& file2 = file_format.hash_files(1);
  EXPECT_EQ(file2.prefix_size(), 4);
  EXPECT_EQ(GetContents(file2.extension()), "fooo");

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 2u);
  EXPECT_EQ(view[4], "fooo");
  EXPECT_EQ(view[2], "ba");
}

TEST_F(HashPrefixMapTest, BuffersWrites) {
  HashPrefixMap map(GetBasePath(),
                    base::SequencedTaskRunner::GetCurrentDefault(),
                    /*buffer_size=*/4);

  map.Append(4, "fooo");
  EXPECT_EQ(GetContents(map.GetExtensionForTesting(4)), "");

  map.Append(4, "barr");
  EXPECT_EQ(GetContents(map.GetExtensionForTesting(4)), "fooo");

  map.Append(4, "somemore");
  EXPECT_EQ(GetContents(map.GetExtensionForTesting(4)), "fooobarrsomemore");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));

  EXPECT_EQ(file_format.hash_files().size(), 1);
  const auto& hash_file = file_format.hash_files(0);
  EXPECT_EQ(hash_file.prefix_size(), 4);
  EXPECT_EQ(GetContents(hash_file.extension()), "fooobarrsomemore");
}

TEST_F(HashPrefixMapTest, ReadFile) {
  base::WriteFile(GetPath("foo"), "fooo");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  HashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), APPLY_UPDATE_SUCCESS);
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

TEST_F(HashPrefixMapTest, ReadMultipleFiles) {
  base::WriteFile(GetPath("foo"), "fooo");
  base::WriteFile(GetPath("bar"), "barr");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(2);
  hash_file->set_extension("bar");
  hash_file->set_file_size(4);

  HashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), APPLY_UPDATE_SUCCESS);
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 2u);
  EXPECT_EQ(view[4], "fooo");
  EXPECT_EQ(view[2], "barr");
}

TEST_F(HashPrefixMapTest, ReadFileInvalid) {
  // No file has been created.
  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  HashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), MMAP_FAILURE);
  EXPECT_EQ(map.IsValid(), MMAP_FAILURE);
}

TEST_F(HashPrefixMapTest, ReadFileWrongSize) {
  base::WriteFile(GetPath("foo"), "");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  HashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), MMAP_FAILURE);
}

TEST_F(HashPrefixMapTest, ReadFileInvalidSize) {
  // The file size must be a multiple of the prefix size.
  base::WriteFile(GetPath("foo"), "foo");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");
  hash_file->set_file_size(3);

  HashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), ADDITIONS_SIZE_UNEXPECTED_FAILURE);
}

TEST_F(HashPrefixMapTest, WriteAndReadFile) {
  HashPrefixMap map(GetBasePath());
  map.Append(4, "fooo");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMap map_read(GetBasePath());
  EXPECT_EQ(map_read.ReadFromDisk(file_format), APPLY_UPDATE_SUCCESS);
  EXPECT_EQ(map_read.IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMapView view = map_read.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

TEST_F(HashPrefixMapTest, ClearingMapBeforeWriteDeletesFile) {
  HashPrefixMap map(GetBasePath(),
                    base::SequencedTaskRunner::GetCurrentDefault(),
                    /*buffer_size=*/1);
  map.Append(4, "foo");

  std::string extension = map.GetExtensionForTesting(4);
  EXPECT_EQ(GetContents(extension), "foo");

  map.ClearAndWaitForTesting();
  EXPECT_FALSE(base::PathExists(GetPath(extension)));
}

TEST_F(HashPrefixMapTest, GetMatchingHashPrefix) {
  HashPrefixMap map(GetBasePath());

  std::string s = "abcd";
  map.Append(4, s);

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  EXPECT_EQ(file_format.hash_files().size(), 1);

  EXPECT_EQ(map.GetMatchingHashPrefix(s), s);
  EXPECT_EQ(map.GetMatchingHashPrefix(s + "foobar"), s);
  EXPECT_EQ(map.GetMatchingHashPrefix("blah"), "");

  EXPECT_EQ(map.MigrateFileFormat(GetBasePath(), &file_format),
            HashPrefixMap::MigrateResult::kNotNeeded);
}

// Tests that the data in a map is still valid after writing it.
TEST_F(HashPrefixMapTest, ValidAfterWrite) {
  HashPrefixMap hash_prefix_map(GetBasePath());
  hash_prefix_map.Append(4, "fooo");

  V4StoreFileFormat file_format;
  ASSERT_TRUE(hash_prefix_map.WriteToDisk(&file_format));

  HashPrefixMapView view = hash_prefix_map.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "fooo");
}

}  // namespace
}  // namespace safe_browsing
