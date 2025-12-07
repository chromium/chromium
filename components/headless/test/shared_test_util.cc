// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/shared_test_util.h"

namespace headless {

namespace {
constexpr base::FilePath::CharType kHeadlessMode[] =
    FILE_PATH_LITERAL("-headless-mode");
constexpr base::FilePath::CharType kHeadlessShell[] =
    FILE_PATH_LITERAL("-headless-shell");
constexpr base::FilePath::CharType kExpectationFileSuffix[] =
    FILE_PATH_LITERAL("-expected");
constexpr base::FilePath::CharType kExpectationFileExtension[] =
    FILE_PATH_LITERAL("txt");
}  // namespace

base::FilePath GetTestExpectationFilePath(
    const base::FilePath& test_script_path,
    const TestMetaInfo& test_meta_info,
    HeadlessType headless_type) {
  base::FilePath expectation_path = test_script_path;
  switch (headless_type) {
    case HeadlessType::kUnspecified:
      break;
    case HeadlessType::kHeadlessMode:
      if (test_meta_info.fork_headless_mode_expectations) {
        expectation_path =
            test_script_path.InsertBeforeExtension(kHeadlessMode);
      }
      break;
    case HeadlessType::kHeadlessShell:
      if (test_meta_info.fork_headless_shell_expectations) {
        expectation_path =
            test_script_path.InsertBeforeExtension(kHeadlessShell);
      }
      break;
  }

  return expectation_path.InsertBeforeExtension(kExpectationFileSuffix)
      .ReplaceExtension(kExpectationFileExtension);
}

}  // namespace headless
