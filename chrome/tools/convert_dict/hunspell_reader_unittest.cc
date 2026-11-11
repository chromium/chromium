// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/convert_dict/hunspell_reader.h"

#include <stdio.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace convert_dict {

namespace {

using ::testing::ElementsAre;

class HunspellReaderTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Writes `contents` to a new temporary file and opens it the same way that
  // AffReader and DicReader open their input files. Each call uses a distinct
  // file, since Windows doesn't allow writing to a file that is still open.
  base::ScopedFILE OpenFileWithContents(std::string_view contents) {
    base::FilePath path;
    if (!base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &path)) {
      ADD_FAILURE() << "Failed to create temp file";
      return nullptr;
    }
    if (!base::WriteFile(path, contents)) {
      ADD_FAILURE() << "Failed to write temp file";
      return nullptr;
    }
    base::ScopedFILE file(base::OpenFile(path, "r"));
    EXPECT_TRUE(file);
    return file;
  }

  // Reads all lines from `contents` using the same loop as AffReader and
  // DicReader.
  std::vector<std::string> ReadAllLines(std::string_view contents) {
    base::ScopedFILE file = OpenFileWithContents(contents);
    std::vector<std::string> lines;
    if (!file) {
      return lines;
    }
    while (!feof(file.get())) {
      lines.push_back(ReadLine(file.get()));
    }
    return lines;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(HunspellReaderTest, ReadLineEmptyFile) {
  base::ScopedFILE file = OpenFileWithContents("");
  ASSERT_TRUE(file);
  EXPECT_EQ(ReadLine(file.get()), "");
  EXPECT_TRUE(feof(file.get()));
  EXPECT_THAT(ReadAllLines(""), ElementsAre(""));
}

TEST_F(HunspellReaderTest, ReadLineWithTrailingNewline) {
  // As with fgets(), EOF is only detected by the read after the last newline,
  // so callers looping on feof() see a final empty line.
  EXPECT_THAT(ReadAllLines("first\nsecond line\n"),
              ElementsAre("first", "second line", ""));
}

TEST_F(HunspellReaderTest, ReadLineWithoutTrailingNewline) {
  base::ScopedFILE file = OpenFileWithContents("first\nsecond");
  ASSERT_TRUE(file);
  EXPECT_EQ(ReadLine(file.get()), "first");
  EXPECT_FALSE(feof(file.get()));
  EXPECT_EQ(ReadLine(file.get()), "second");
  EXPECT_TRUE(feof(file.get()));

  EXPECT_THAT(ReadAllLines("first\nsecond"), ElementsAre("first", "second"));
}

TEST_F(HunspellReaderTest, ReadLineEmptyLines) {
  EXPECT_THAT(ReadAllLines("\n\nword\n\n"),
              ElementsAre("", "", "word", "", ""));
}

TEST_F(HunspellReaderTest, ReadLineTrimsWhitespace) {
  EXPECT_THAT(ReadAllLines("  leading\ntrailing \t\nboth\r\n \t \nlast\r\n"),
              ElementsAre("leading", "trailing", "both", "", "last", ""));
}

TEST_F(HunspellReaderTest, ReadLineKeepsComments) {
  // ReadLine() doesn't strip comments; callers use StripComment() for that.
  EXPECT_THAT(ReadAllLines("# comment\nword # comment\n"),
              ElementsAre("# comment", "word # comment", ""));
}

TEST_F(HunspellReaderTest, ReadLineStripsByteOrderMark) {
  EXPECT_THAT(ReadAllLines("\xEF\xBB\xBFSET UTF-8\nword\n"),
              ElementsAre("SET UTF-8", "word", ""));
}

TEST_F(HunspellReaderTest, ReadLineStripsByteOrderMarkOnlyLine) {
  EXPECT_THAT(ReadAllLines("\xEF\xBB\xBF\nword\n"),
              ElementsAre("", "word", ""));
}

TEST_F(HunspellReaderTest, ReadLineKeepsNonAsciiText) {
  // 0xFF is valid in single-byte encodings such as ISO8859-1 (AffReader's
  // default) and must not be confused with EOF.
  EXPECT_THAT(
      ReadAllLines("caf\xC3\xA9\n\xD0\xBC\xD0\xB8\xD1\x80\n\xFF\n\xFF\xFF"),
      ElementsAre("caf\xC3\xA9", "\xD0\xBC\xD0\xB8\xD1\x80", "\xFF",
                  "\xFF\xFF"));
}

TEST_F(HunspellReaderTest, ReadLineDropsTextAfterEmbeddedNul) {
  // Matches the previous fgets() + std::string(const char*) behavior, which
  // truncated each line at the first NUL byte.
  constexpr char kContents[] = "ab\0cd\n\0rest\nword\nef\0gh";
  EXPECT_THAT(ReadAllLines(std::string_view(kContents, sizeof(kContents) - 1)),
              ElementsAre("ab", "", "word", "ef"));
}

TEST_F(HunspellReaderTest, ReadLineVeryLongLine) {
  // Lines are not limited to any fixed buffer size.
  const std::string long_line(200000, 'a');
  EXPECT_THAT(ReadAllLines(long_line + "\nword\n" + long_line),
              ElementsAre(long_line, "word", long_line));
}

TEST(HunspellReaderTrimLineTest, TrimLine) {
  std::string line = " \t word \r\n";
  TrimLine(&line);
  EXPECT_EQ(line, "word");

  line = "\xEF\xBB\xBF word ";
  TrimLine(&line);
  EXPECT_EQ(line, "word");

  line = "";
  TrimLine(&line);
  EXPECT_EQ(line, "");
}

TEST(HunspellReaderStripCommentTest, StripComment) {
  std::string line = "word/AB # comment";
  StripComment(&line);
  EXPECT_EQ(line, "word/AB");

  line = "# only a comment";
  StripComment(&line);
  EXPECT_EQ(line, "");

  line = "no comment ";
  StripComment(&line);
  EXPECT_EQ(line, "no comment ");
}

}  // namespace

}  // namespace convert_dict
