// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_H_

#include <stddef.h>

#include <vector>

#include "base/files/file_path.h"
#include "testing/data_driven_testing/data_driven_test.h"

namespace shared_highlighting {

// Base class for data-driven tests for Shared Highlighting. Each input is
// a structured set of params necessary for running generation and navigation
// tests.
class SharedHighlightingDataDrivenTest : public testing::DataDrivenTest {
 public:
  static const std::vector<base::FilePath> GetTestFiles();

  SharedHighlightingDataDrivenTest();
  ~SharedHighlightingDataDrivenTest() override = default;

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  virtual void GenerateAndNavigate(std::string html_content,
                                   std::string start_node_name,
                                   int start_offset,
                                   std::string end_node_name,
                                   int end_offset,
                                   std::string selected_text,
                                   std::string highlight_text) = 0;
};

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_H_
