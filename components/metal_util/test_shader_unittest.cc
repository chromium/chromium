// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/test_shader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(GetAlteredLibraryDataTest, TestInvalidAccess) {
  for (int j = 0; j < 4096; ++j) {
    std::vector<uint8_t> data = metal::GetAlteredLibraryData();
    EXPECT_EQ(data.size(), metal::kTestLibSize);
    for (size_t i = 0; i < metal::kLiteralSize; ++i)
      EXPECT_NE(data[metal::kLiteralOffset + i], 0);
  }
}

}  // namespace
