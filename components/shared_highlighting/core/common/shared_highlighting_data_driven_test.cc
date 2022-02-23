// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_data_driven_test.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/foundation_util.h"
#endif

namespace shared_highlighting {

namespace {

const base::FilePath::CharType kFeatureName[] =
    FILE_PATH_LITERAL("shared_highlighting");
const base::FilePath::CharType kTestName[] =
    FILE_PATH_LITERAL("generate_navigate");
const base::FilePath::CharType kFileNamePattern[] = FILE_PATH_LITERAL("*.in");

const char kHtmlFileNameField[] = "HTML_FILE_NAME";
const char kStartNodeNameField[] = "START_NODE_NAME";
const char kStartOffsetField[] = "START_OFFSET";
const char kEndNodeNameField[] = "END_NODE_NAME";
const char kEndOffsetField[] = "END_OFFSET";
const char kSelectedTextField[] = "SELECTED_TEXT";
const char kHighlightTextField[] = "HIGHLIGHT_TEXT";

const char kFieldSeparator[] = ":";

// Reads |file| into |content|, and converts Windows line-endings to Unix ones.
// Returns true on success.
bool ReadFile(const base::FilePath& file, std::string* content) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  if (!base::ReadFileToString(file, content))
    return false;

  base::ReplaceSubstringsAfterOffset(content, 0, "\r\n", "\n");
  return true;
}

const base::FilePath GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &dir);
    return dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data");
  }());
  return *dir;
}

const base::FilePath GetInputDir() {
  base::FilePath dir = GetTestDataDir();
  return dir.Append(kFeatureName).Append(kTestName).AppendASCII("input");
}
}  // namespace

const std::vector<base::FilePath>
SharedHighlightingDataDrivenTest::GetTestFiles() {
  base::FilePath dir = GetInputDir().AppendASCII("test_params");
  base::FileEnumerator input_files(dir, false, base::FileEnumerator::FILES,
                                   kFileNamePattern);
  std::vector<base::FilePath> files;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    files.push_back(input_file);
  }
  std::sort(files.begin(), files.end());

#if BUILDFLAG(IS_MAC)
  base::mac::ClearAmIBundledCache();
#endif  // BUILDFLAG(IS_MAC)

  return files;
}

SharedHighlightingDataDrivenTest::SharedHighlightingDataDrivenTest()
    : testing::DataDrivenTest(GetTestDataDir(), kFeatureName, kTestName) {}

void SharedHighlightingDataDrivenTest::GenerateResults(const std::string& input,
                                                       std::string* output) {
  std::string html_file_name;
  std::string start_node_name;
  int start_offset;
  std::string end_node_name;
  int end_offset;
  std::string selected_text;
  std::string highlight_text;

  // Parse the input line by line.
  std::vector<std::string> lines = base::SplitString(
      input, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string line : lines) {
    size_t separator_pos = line.find(kFieldSeparator);
    ASSERT_FALSE(separator_pos == std::string::npos);

    std::string field_type = line.substr(0, separator_pos);
    do {
      ++separator_pos;
    } while (separator_pos < line.size() && line[separator_pos] == ' ');
    std::string value_utf8 = line.substr(separator_pos);

    if (field_type == kHtmlFileNameField) {
      html_file_name = value_utf8;
    } else if (field_type == kStartNodeNameField) {
      start_node_name = value_utf8;
    } else if (field_type == kStartOffsetField) {
      base::StringToInt(value_utf8, &start_offset);
    } else if (field_type == kEndNodeNameField) {
      end_node_name = value_utf8;
    } else if (field_type == kEndOffsetField) {
      base::StringToInt(value_utf8, &end_offset);
    } else if (field_type == kSelectedTextField) {
      selected_text = value_utf8;
    } else if (field_type == kHighlightTextField) {
      highlight_text = value_utf8;
    } else {
      NOTREACHED();
      return;
    }
  }
  std::string html_content;
  ReadFile(GetInputDir().AppendASCII(html_file_name), &html_content);
  GenerateAndNavigate(html_content, start_node_name, start_offset,
                      end_node_name, end_offset, selected_text, highlight_text);
}

}  // namespace shared_highlighting
