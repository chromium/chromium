// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_path_set.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {
const wchar_t kFileNameFull[] = L"C:\\Filename";
const wchar_t kLongFileName[] = L"Long File Name.bla";
const wchar_t kLongFileNameFull[] = L"C:\\Long File Name.bla";
}  // namespace

TEST(FilePathSetTests, Empty) {
  FilePathSet file_paths;
  // Start empty.
  EXPECT_TRUE(file_paths.empty());
  // Clear should be callable on empty sets.
  file_paths.clear();
}

TEST(FilePathSetTests, InsertedOnce) {
  FilePathSet file_paths;
  // Same path should be inserted only once.
  base::FilePath file_path1(kFileNameFull);
  EXPECT_TRUE(file_paths.Insert(file_path1));
  EXPECT_FALSE(file_paths.Insert(file_path1));
  EXPECT_EQ(1UL, file_paths.size());
  // But still should be found.
  EXPECT_TRUE(file_paths.Contains(file_path1));
}

TEST(FilePathSetTests, EqualOperator) {
  FilePathSet file_paths1;
  base::FilePath file_path(kFileNameFull);
  EXPECT_TRUE(file_paths1.Insert(file_path));
  EXPECT_EQ(file_paths1, file_paths1);

  FilePathSet file_paths2;
  EXPECT_TRUE(file_paths2.Insert(file_path));
  EXPECT_EQ(file_paths1, file_paths2);

  base::FilePath long_file_path(kLongFileNameFull);
  EXPECT_TRUE(file_paths1.Insert(long_file_path));
  EXPECT_TRUE(file_paths2.Insert(long_file_path));
  EXPECT_EQ(file_paths1, file_paths2);

  base::FilePath other_path(L"C:\\other_path.txt");
  EXPECT_TRUE(file_paths2.Insert(other_path));
  // To avoid implementing operator!=();
  EXPECT_FALSE(file_paths1 == file_paths2);
}

TEST(FilePathSetTests, LongName) {
  FilePathSet file_paths;
  // Long paths should also be found, even by their short version.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath short_name_path;
  base::FilePath long_name_path(
      scoped_temp_dir.GetPath().Append(kLongFileName));
  CreateFileAndGetShortName(long_name_path, &short_name_path);
  EXPECT_TRUE(file_paths.Insert(long_name_path));
  EXPECT_FALSE(file_paths.Insert(long_name_path));
  EXPECT_FALSE(file_paths.Insert(short_name_path));
  base::FilePath long_name_path_upper(
      base::ToUpperASCII(long_name_path.value()));
  EXPECT_FALSE(file_paths.Insert(long_name_path_upper));

  // And they should be found-able in all its different forms.
  EXPECT_TRUE(file_paths.Contains(long_name_path));
  EXPECT_TRUE(file_paths.Contains(short_name_path));
  EXPECT_TRUE(file_paths.Contains(long_name_path_upper));
}

TEST(FilePathSetTests, ShortName) {
  FilePathSet file_paths;
  // Short paths should also be found, even by their long version.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath short_name_path;
  base::FilePath long_name_path(
      scoped_temp_dir.GetPath().Append(kLongFileName));
  CreateFileAndGetShortName(long_name_path, &short_name_path);
  EXPECT_TRUE(file_paths.Insert(short_name_path));
  EXPECT_FALSE(file_paths.Insert(short_name_path));
  EXPECT_FALSE(file_paths.Insert(long_name_path));
  base::FilePath long_name_path_upper(
      base::ToUpperASCII(long_name_path.value()));
  EXPECT_FALSE(file_paths.Insert(long_name_path_upper));

  // And they should be found-able in all its different forms.
  EXPECT_TRUE(file_paths.Contains(long_name_path));
  EXPECT_TRUE(file_paths.Contains(short_name_path));
  EXPECT_TRUE(file_paths.Contains(long_name_path_upper));
}

TEST(FilePathSetTests, ProperOrder) {
  // Ensure that all the items in a folder are listed before the folder.
  const base::FilePath folder(L"C:\\folder");
  base::FilePath file = folder.Append(L"file.exe");
  base::FilePath sub_folder = folder.Append(L"sub_folder");
  base::FilePath sub_file = sub_folder.Append(L"sub_file.txt");

  FilePathSet file_paths;
  EXPECT_TRUE(file_paths.Insert(folder));
  EXPECT_TRUE(file_paths.Insert(file));
  EXPECT_TRUE(file_paths.Insert(sub_folder));
  EXPECT_TRUE(file_paths.Insert(sub_file));

  const auto sorted_files = file_paths.ReverseSorted();
  ASSERT_EQ(4UL, sorted_files.size());
  EXPECT_EQ(sub_file, sorted_files[0]);
  EXPECT_EQ(sub_folder, sorted_files[1]);
  EXPECT_EQ(file, sorted_files[2]);
  EXPECT_EQ(folder, sorted_files[3]);
}

TEST(FilePathMap, Find) {
  // Long paths should also be found, even by their short version.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath short_name_path;
  base::FilePath long_name_path(
      scoped_temp_dir.GetPath().Append(kLongFileName));
  CreateFileAndGetShortName(long_name_path, &short_name_path);

  FilePathMap<int> map;
  EXPECT_TRUE(map.Insert(short_name_path, 42));

  const int* found_value = nullptr;
  ASSERT_NE(nullptr, found_value = map.Find(short_name_path));
  EXPECT_EQ(42, *found_value);

  ASSERT_NE(nullptr, found_value = map.Find(long_name_path));
  EXPECT_EQ(42, *found_value);

  EXPECT_EQ(nullptr, map.Find(base::FilePath(kFileNameFull)));
}

TEST(FilePathMap, DoNotInsertDuplicates) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath short_name_path;
  base::FilePath long_name_path(
      scoped_temp_dir.GetPath().Append(kLongFileName));
  CreateFileAndGetShortName(long_name_path, &short_name_path);

  FilePathMap<int> map;
  EXPECT_TRUE(map.Insert(long_name_path, 42));
  EXPECT_FALSE(map.Insert(long_name_path, 43));
  EXPECT_FALSE(map.Insert(short_name_path, 44));
  EXPECT_EQ(1u, map.map().size());

  const int* found_value = nullptr;
  EXPECT_NE(nullptr, found_value = map.Find(short_name_path));
  EXPECT_EQ(42, *found_value);
}

}  // namespace chrome_cleaner
