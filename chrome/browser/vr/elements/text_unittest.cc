// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(Text, MultiLine) {
  const float kInitialSize = 1.0f;

  // Create an initialize a text element with a long string.
  auto text = std::make_unique<Text>(0.020);
  text->SetFieldWidth(kInitialSize);
  text->SetText(base::UTF8ToUTF16(std::string(1000, 'x')));

  // Make sure we get multiple lines of rendered text from the string.
  text->PrepareToDrawForTest();
  size_t initial_num_lines = text->LinesForTest().size();
  auto initial_size = text->texture_size_for_test();
  EXPECT_GT(initial_num_lines, 1u);
  EXPECT_GT(initial_size.height(), 0.f);

  // Reduce the field width, and ensure that the number of lines increases along
  // with the texture height.
  text->SetFieldWidth(kInitialSize / 2);
  text->PrepareToDrawForTest();
  EXPECT_GT(text->LinesForTest().size(), initial_num_lines);
  EXPECT_GT(text->texture_size_for_test().height(), initial_size.height());
}

}  // namespace vr
