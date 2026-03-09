// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/extraction/filter_extractor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

class FilterExtractorTest : public ::testing::Test {
 public:
  FilterExtractorTest() = default;
  ~FilterExtractorTest() override = default;

 protected:
  FilterExtractor extractor;
};

}  // namespace multistep_filter
