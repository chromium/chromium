// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/js_test_api.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

JsTestApiConfig::JsTestApiConfig() {
  constexpr const base::FilePath::CharType* kLibraries[] = {
      FILE_PATH_LITERAL("chrome/third_party/mock4js/mock4js.js"),
      FILE_PATH_LITERAL("third_party/chaijs/chai.js"),
      FILE_PATH_LITERAL("third_party/accessibility-audit/axs_testing.js"),
  };
  constexpr base::FilePath::CharType kWebUITestFolder[] =
      FILE_PATH_LITERAL("webui");

  base::FilePath test_data_directory;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory));

  search_path = test_data_directory.Append(kWebUITestFolder);

  for (const auto* path : kLibraries)
    default_libraries.emplace_back(path);

  default_libraries.push_back(search_path.AppendASCII("test_api.js"));
}

JsTestApiConfig::~JsTestApiConfig() = default;
