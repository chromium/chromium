// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_H_

#include <stddef.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/shared_highlighting/core/common/shared_highlighting_data_driven_test_results.h"
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

  virtual SharedHighlightingDataDrivenTestResults GenerateAndNavigate(
      std::string html_content,
      std::string* start_parent_id,
      int start_offset_in_parent,
      std::optional<int> start_text_offset,
      std::string* end_parent_id,
      int end_offset_in_parent,
      std::optional<int> end_text_offset,
      std::string selected_text,
      std::string* highlight_text) = 0;
};

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_H_
