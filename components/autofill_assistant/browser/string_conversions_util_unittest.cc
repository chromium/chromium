// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/string_conversions_util.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Eq;

TEST(StringConversionsTest, ConversionIsSymmetrical) {
  // String containing 1-byte, 2-byte, 3-byte and 4-byte UTF-8 characters.
  std::string input = "Aü万𠜎";

  auto codepoints = UTF8ToUnicode(input);
  std::string output;
  EXPECT_TRUE(UnicodeToUTF8(codepoints, &output));
  EXPECT_EQ(input, output);
}

}  // namespace
}  // namespace autofill_assistant
