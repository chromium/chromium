// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/shared_test_util.h"

namespace headless {

namespace {
constexpr base::FilePath::CharType kExpectationFileSuffix[] =
    FILE_PATH_LITERAL("-expected");
constexpr base::FilePath::CharType kExpectationFileExtension[] =
    FILE_PATH_LITERAL("txt");
}  // namespace

bool IsSharedTestScript(std::string_view script_name) {
  return script_name.starts_with("shared/");
}

base::FilePath GetTestExpectationFilePath(
    const base::FilePath& test_script_path) {
  return test_script_path.InsertBeforeExtension(kExpectationFileSuffix)
      .ReplaceExtension(kExpectationFileExtension);
}

}  // namespace headless
