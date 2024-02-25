// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_data_driven_test.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

namespace shared_highlighting {

namespace {

const base::FilePath::CharType kFeatureName[] =
    FILE_PATH_LITERAL("shared_highlighting");
const base::FilePath::CharType kTestName[] =
    FILE_PATH_LITERAL("generate_navigate");
const base::FilePath::CharType kFileNamePattern[] = FILE_PATH_LITERAL("*.json");

const char kHtmlFileNameField[] = "htmlFileName";
const char kStartParentIdField[] = "startParentId";
const char kStartOffsetInParentField[] = "startOffsetInParent";
const char kStartTextOffsetField[] = "startTextOffset";
const char kEndParentIdField[] = "endParentId";
const char kEndOffsetInParentField[] = "endOffsetInParent";
const char kEndTextOffsetField[] = "endTextOffset";
const char kSelectedTextField[] = "selectedText";
const char kHighlightTextField[] = "highlightText";

const std::string kTestSuccess = "Success";
const std::string kTestFailure = "Failure";

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
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
    return dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data");
  }());
  return *dir;
}

const base::FilePath GetInputDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
    return dir.AppendASCII("third_party")
        .AppendASCII("text-fragments-polyfill")
        .AppendASCII("src")
        .AppendASCII("test")
        .AppendASCII("data-driven")
        .AppendASCII("input");
  }());
  return *dir;
}

const base::FilePath GetHtmlDir() {
  return GetInputDir().AppendASCII("html");
}
}  // namespace

const std::vector<base::FilePath>
SharedHighlightingDataDrivenTest::GetTestFiles() {
  base::FilePath dir = GetInputDir().AppendASCII("test-params");
  base::FileEnumerator input_files(dir, false, base::FileEnumerator::FILES,
                                   kFileNamePattern);
  std::vector<base::FilePath> files;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    files.push_back(input_file);
  }
  std::sort(files.begin(), files.end());

#if BUILDFLAG(IS_MAC)
  base::apple::ClearAmIBundledCache();
#endif  // BUILDFLAG(IS_MAC)

  return files;
}

SharedHighlightingDataDrivenTest::SharedHighlightingDataDrivenTest()
    : testing::DataDrivenTest(GetTestDataDir(), kFeatureName, kTestName) {}

void SharedHighlightingDataDrivenTest::GenerateResults(const std::string& input,
                                                       std::string* output) {
  std::string html_file_name;
  std::string* start_parent_id;
  int start_offset_in_parent;
  std::optional<int> start_text_offset;
  std::string* end_parent_id;
  int end_offset_in_parent;
  std::optional<int> end_text_offset;
  std::string selected_text;
  std::string* highlight_text;

  std::optional<base::Value> parsed_input = base::JSONReader::Read(input);
  ASSERT_TRUE(parsed_input.has_value() && parsed_input->is_dict());

  base::Value::Dict& input_dict = parsed_input->GetDict();
  html_file_name = *input_dict.FindString(kHtmlFileNameField);
  start_parent_id = input_dict.FindString(kStartParentIdField);
  start_offset_in_parent = *input_dict.FindInt(kStartOffsetInParentField);
  start_text_offset = input_dict.FindInt(kStartTextOffsetField);
  end_parent_id = input_dict.FindString(kEndParentIdField);
  end_offset_in_parent = *input_dict.FindInt(kEndOffsetInParentField);
  end_text_offset = input_dict.FindInt(kEndTextOffsetField);
  selected_text = *input_dict.FindString(kSelectedTextField);
  highlight_text = input_dict.FindString(kHighlightTextField);

  std::string html_content;
  ReadFile(GetHtmlDir().AppendASCII(html_file_name), &html_content);
  auto results = GenerateAndNavigate(
      html_content, start_parent_id, start_offset_in_parent, start_text_offset,
      end_parent_id, end_offset_in_parent, end_text_offset, selected_text,
      highlight_text);

  *output = "GENERATION: " +
            (results.generation_success ? kTestSuccess : kTestFailure) + "\n" +
            "HIGHLIGHTING: " +
            (results.highlighting_success ? kTestSuccess : kTestFailure) + "\n";
}

}  // namespace shared_highlighting
