// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// When understanding these test cases, the nuspell docs are a useful reference:
//   https://github.com/nuspell/nuspell/wiki/Affix-File-Format

#include "testing/gtest/include/gtest/gtest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/tools/convert_dict/aff_reader.h"

base::FilePath TestFilePath(const std::string& name) {
  base::FilePath db_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &db_path);
  return db_path.AppendASCII("convert_dict").AppendASCII(name);
}

using convert_dict::AffReader;

TEST(AffReaderTest, EmptyFile) {
  AffReader reader(TestFilePath("empty.aff"));
  EXPECT_TRUE(reader.Read());
}

TEST(AffReaderTest, LeadingComment) {
  AffReader reader(TestFilePath("leading-comment.aff"));
  EXPECT_TRUE(reader.Read());
  EXPECT_NE(std::string::npos, reader.comments().find("Foobar"));
}

TEST(AffReaderTest, AffixFlags) {
  AffReader reader(TestFilePath("affix-flags.aff"));
  EXPECT_TRUE(reader.Read());
  EXPECT_EQ(reader.GetAffixGroups()[0], "AF Foo");
  EXPECT_EQ(reader.GetAffixGroups()[1], "AF Bar");
  EXPECT_EQ(reader.GetAffixGroups()[2], "AF Foobar");
}

TEST(AffReaderTest, PrefixSuffix) {
  AffReader reader(TestFilePath("prefix-suffix.aff"));
  EXPECT_TRUE(reader.Read());
  EXPECT_EQ(reader.affix_rules()[0], "PFX VERB Y 1");
  EXPECT_EQ(reader.affix_rules()[1], "PFX VERB 0 re .");
}

TEST(AffReaderTest, IndexedAffix) {
  AffReader reader(TestFilePath("indexed-affix.aff"));
  EXPECT_TRUE(reader.Read());
  // The class name ("ADJECTIVE" in the input) should have been converted into
  // an index in the AffReader's internal class table.
  EXPECT_EQ(reader.affix_rules()[3], "SFX VERB 0 able/1 .");
}

TEST(AffReaderTest, Rep) {
  AffReader reader(TestFilePath("rep.aff"));
  EXPECT_TRUE(reader.Read());
  EXPECT_EQ(reader.replacements()[0].first, "f");
  EXPECT_EQ(reader.replacements()[0].second, "ph");
  EXPECT_EQ(reader.replacements()[2].first, "^alot$");
  // The "_" in the input should have been converted to a space - this is how
  // multi-word suggestions are represented in AFF files.
  EXPECT_EQ(reader.replacements()[2].second, "a lot");
}

TEST(AffReaderTest, OtherCommands) {
  AffReader reader(TestFilePath("other-commands.aff"));
  EXPECT_TRUE(reader.Read());
  EXPECT_EQ(reader.other_commands()[0], "FOOBAR foo bar");
}
