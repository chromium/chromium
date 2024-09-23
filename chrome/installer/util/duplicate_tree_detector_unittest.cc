// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/duplicate_tree_detector.h"

#include <windows.h>

#include <fstream>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DuplicateTreeDetectorTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_source_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dest_dir_.CreateUniqueTempDir());
  }

  // Simple function to dump some text into a new file.
  void CreateTextFile(const std::string& filename,
                      const std::wstring& contents) {
    std::wofstream file;
    file.open(filename.c_str());
    ASSERT_TRUE(file.is_open());
    file << contents;
    file.close();
  }

  // Creates a two level deep source dir with a file in each in |first_root| and
  // copy it (files properties will be identical) in |second_root|.
  void CreateTwoIdenticalHierarchies(const base::FilePath& first_root,
                                     const base::FilePath& second_root) {
    base::FilePath d1(first_root);
    d1 = d1.AppendASCII("D1");
    base::CreateDirectory(d1);
    ASSERT_TRUE(base::PathExists(d1));

    base::FilePath f1(d1);
    f1 = f1.AppendASCII("F1");
    CreateTextFile(f1.MaybeAsASCII(), text_content_1_);
    ASSERT_TRUE(base::PathExists(f1));

    base::FilePath d2(d1);
    d2 = d2.AppendASCII("D2");
    base::CreateDirectory(d2);
    ASSERT_TRUE(base::PathExists(d2));

    base::FilePath f2(d2);
    f2 = f2.AppendASCII("F2");
    CreateTextFile(f2.MaybeAsASCII(), text_content_2_);
    ASSERT_TRUE(base::PathExists(f2));

    ASSERT_TRUE(base::CopyDirectory(d1, second_root, true));
  }

  base::ScopedTempDir temp_source_dir_;
  base::ScopedTempDir temp_dest_dir_;

  static const wchar_t text_content_1_[];
  static const wchar_t text_content_2_[];
  static const wchar_t text_content_3_[];
};

const wchar_t DuplicateTreeDetectorTest::text_content_1_[] =
    L"Gooooooooooooooooooooogle";
const wchar_t DuplicateTreeDetectorTest::text_content_2_[] = L"Overwrite Me";
const wchar_t DuplicateTreeDetectorTest::text_content_3_[] =
    L"I'd rather see your watermelon and raise you ham and a half.";

}  // namespace

// Test the DuplicateTreeChecker's definition of identity on two identical
// directory structures.
TEST_F(DuplicateTreeDetectorTest, TestIdenticalDirs) {
  CreateTwoIdenticalHierarchies(temp_source_dir_.GetPath(),
                                temp_dest_dir_.GetPath());

  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(temp_source_dir_.GetPath(),
                                                  temp_dest_dir_.GetPath()));
}

// Test when source entirely contains dest but contains other files as well.
// IsIdenticalTo should return false in this case.
TEST_F(DuplicateTreeDetectorTest, TestSourceContainsDest) {
  CreateTwoIdenticalHierarchies(temp_source_dir_.GetPath(),
                                temp_dest_dir_.GetPath());

  base::FilePath new_file(temp_source_dir_.GetPath());
  new_file = new_file.AppendASCII("FNew");
  CreateTextFile(new_file.MaybeAsASCII(), text_content_1_);
  ASSERT_TRUE(base::PathExists(new_file));

  EXPECT_FALSE(installer::IsIdenticalFileHierarchy(temp_source_dir_.GetPath(),
                                                   temp_dest_dir_.GetPath()));
}

// Test when dest entirely contains source but contains other files as well.
// IsIdenticalTo should return true in this case.
TEST_F(DuplicateTreeDetectorTest, TestDestContainsSource) {
  CreateTwoIdenticalHierarchies(temp_source_dir_.GetPath(),
                                temp_dest_dir_.GetPath());

  base::FilePath new_file(temp_dest_dir_.GetPath());
  new_file = new_file.AppendASCII("FNew");
  CreateTextFile(new_file.MaybeAsASCII(), text_content_1_);
  ASSERT_TRUE(base::PathExists(new_file));

  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(temp_source_dir_.GetPath(),
                                                  temp_dest_dir_.GetPath()));
}

// Test when the file hierarchies are the same but one of the files is changed.
TEST_F(DuplicateTreeDetectorTest, TestIdenticalDirsDifferentFiles) {
  CreateTwoIdenticalHierarchies(temp_source_dir_.GetPath(),
                                temp_dest_dir_.GetPath());

  base::FilePath existing_file(temp_dest_dir_.GetPath());
  existing_file =
      existing_file.AppendASCII("D1").AppendASCII("D2").AppendASCII("F2");
  CreateTextFile(existing_file.MaybeAsASCII(), text_content_3_);

  EXPECT_FALSE(installer::IsIdenticalFileHierarchy(temp_source_dir_.GetPath(),
                                                   temp_dest_dir_.GetPath()));
}

// Test when both file hierarchies are empty.
TEST_F(DuplicateTreeDetectorTest, TestEmptyDirs) {
  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(temp_source_dir_.GetPath(),
                                                  temp_dest_dir_.GetPath()));
}

// Test on single files.
TEST_F(DuplicateTreeDetectorTest, TestSingleFiles) {
  // Create a source file.
  base::FilePath source_file(temp_source_dir_.GetPath());
  source_file = source_file.AppendASCII("F1");
  CreateTextFile(source_file.MaybeAsASCII(), text_content_1_);

  // This file should be the same.
  base::FilePath dest_file(temp_dest_dir_.GetPath());
  dest_file = dest_file.AppendASCII("F1");
  ASSERT_TRUE(base::CopyFile(source_file, dest_file));

  // This file should be different.
  base::FilePath other_file(temp_dest_dir_.GetPath());
  other_file = other_file.AppendASCII("F2");
  CreateTextFile(other_file.MaybeAsASCII(), text_content_2_);

  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(source_file, dest_file));
  EXPECT_FALSE(installer::IsIdenticalFileHierarchy(source_file, other_file));
}
