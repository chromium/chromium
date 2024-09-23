// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/js_test_api.h"

#include "base/check.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

JsTestApiConfig::JsTestApiConfig() {
  constexpr const base::FilePath::CharType* kLibraries[] = {
      FILE_PATH_LITERAL("chrome/test/data/webui/chromeos/chai_v4.js"),
  };
  constexpr base::FilePath::CharType kWebUIChromeOSTestFolder[] =
      FILE_PATH_LITERAL("webui/chromeos");

  base::FilePath test_data_directory;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory));

  search_path = test_data_directory.Append(kWebUIChromeOSTestFolder);

  for (const auto* path : kLibraries)
    default_libraries.emplace_back(path);

  default_libraries.push_back(search_path.AppendASCII("test_api.js"));
}

JsTestApiConfig::~JsTestApiConfig() = default;
