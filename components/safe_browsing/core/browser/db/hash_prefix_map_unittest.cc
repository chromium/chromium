// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/ranges/algorithm.h"
#include "testing/platform_test.h"

namespace safe_browsing {
namespace {

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

  base::Time time_ = base::Time::Now();
  base::ScopedTempDir temp_dir_;
};

TEST_F(HashPrefixMapTest, WriteFile) {
  MmapHashPrefixMap map(GetBasePath());
  map.Append(4, "foo");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  EXPECT_EQ(file_format.hash_files().size(), 1);
  const auto& hash_file = file_format.hash_files(0);
  EXPECT_EQ(hash_file.prefix_size(), 4);
  EXPECT_EQ(GetContents(hash_file.extension()), "foo");

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "foo");
}

TEST_F(HashPrefixMapTest, FailedWrite) {
  MmapHashPrefixMap map(
      GetBasePath().AppendASCII("bad_dir").AppendASCII("some.store"));
  map.Append(4, "foo");

  V4StoreFileFormat file_format;
  EXPECT_FALSE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), MMAP_FAILURE);
}

TEST_F(HashPrefixMapTest, WriteMultipleFiles) {
  MmapHashPrefixMap map(GetBasePath());
  map.Append(4, "foo");
  map.Append(2, "bar");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  auto hash_files = file_format.hash_files();
  base::ranges::sort(hash_files, {}, &HashFile::prefix_size);
  EXPECT_EQ(hash_files.size(), 2);

  const auto& file1 = file_format.hash_files(0);
  EXPECT_EQ(file1.prefix_size(), 2);
  EXPECT_EQ(GetContents(file1.extension()), "bar");

  const auto& file2 = file_format.hash_files(1);
  EXPECT_EQ(file2.prefix_size(), 4);
  EXPECT_EQ(GetContents(file2.extension()), "foo");

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 2u);
  EXPECT_EQ(view[4], "foo");
  EXPECT_EQ(view[2], "bar");
}

TEST_F(HashPrefixMapTest, BuffersWrites) {
  MmapHashPrefixMap map(GetBasePath(), /*buffer_size=*/4);

  map.Append(4, "foo");
  EXPECT_EQ(GetContents(map.GetExtensionForTesting(4)), "");

  map.Append(4, "bar");
  EXPECT_EQ(GetContents(map.GetExtensionForTesting(4)), "foo");

  map.Append(4, "somemore");
  EXPECT_EQ(GetContents(map.GetExtensionForTesting(4)), "foobarsomemore");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));

  EXPECT_EQ(file_format.hash_files().size(), 1);
  const auto& hash_file = file_format.hash_files(0);
  EXPECT_EQ(hash_file.prefix_size(), 4);
  EXPECT_EQ(GetContents(hash_file.extension()), "foobarsomemore");
}

TEST_F(HashPrefixMapTest, ReadFile) {
  base::WriteFile(GetPath("foo"), "foo");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");

  MmapHashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), APPLY_UPDATE_SUCCESS);
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "foo");
}

TEST_F(HashPrefixMapTest, ReadMultipleFiles) {
  base::WriteFile(GetPath("foo"), "foo");
  base::WriteFile(GetPath("bar"), "bar");

  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");

  hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(2);
  hash_file->set_extension("bar");

  MmapHashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), APPLY_UPDATE_SUCCESS);
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMapView view = map.view();
  EXPECT_EQ(view.size(), 2u);
  EXPECT_EQ(view[4], "foo");
  EXPECT_EQ(view[2], "bar");
}

TEST_F(HashPrefixMapTest, ReadFileInvalid) {
  // No file has been created.
  V4StoreFileFormat file_format;
  auto* hash_file = file_format.add_hash_files();
  hash_file->set_prefix_size(4);
  hash_file->set_extension("foo");

  MmapHashPrefixMap map(GetBasePath());
  EXPECT_EQ(map.ReadFromDisk(file_format), MMAP_FAILURE);
  EXPECT_EQ(map.IsValid(), MMAP_FAILURE);
}

TEST_F(HashPrefixMapTest, WriteAndReadFile) {
  MmapHashPrefixMap map(GetBasePath());
  map.Append(4, "foo");

  V4StoreFileFormat file_format;
  EXPECT_TRUE(map.WriteToDisk(&file_format));
  EXPECT_EQ(map.IsValid(), APPLY_UPDATE_SUCCESS);

  MmapHashPrefixMap map_read(GetBasePath());
  EXPECT_EQ(map_read.ReadFromDisk(file_format), APPLY_UPDATE_SUCCESS);
  EXPECT_EQ(map_read.IsValid(), APPLY_UPDATE_SUCCESS);

  HashPrefixMapView view = map_read.view();
  EXPECT_EQ(view.size(), 1u);
  EXPECT_EQ(view[4], "foo");
}

TEST_F(HashPrefixMapTest, ClearingMapBeforeWriteDeletesFile) {
  MmapHashPrefixMap map(GetBasePath(), /*buffer_size=*/1);
  map.Append(4, "foo");

  std::string extension = map.GetExtensionForTesting(4);
  EXPECT_EQ(GetContents(extension), "foo");

  map.Clear();
  EXPECT_FALSE(base::PathExists(GetPath(extension)));
}

}  // namespace
}  // namespace safe_browsing
