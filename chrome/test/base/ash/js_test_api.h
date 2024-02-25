// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_JS_TEST_API_H_
#define CHROME_TEST_BASE_ASH_JS_TEST_API_H_

#include <vector>

#include "base/files/file_path.h"

// Shared configuration for test harnesses using test_api.js.
struct JsTestApiConfig {
  JsTestApiConfig();
  ~JsTestApiConfig();

  // The search path for client-provided libraries (i.e. DIR_TEST_DATA/webui).
  base::FilePath search_path;

  // The set of libraries required for test_api.js.
  std::vector<base::FilePath> default_libraries;
};

#endif  // CHROME_TEST_BASE_ASH_JS_TEST_API_H_
