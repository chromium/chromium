// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/js_in_process_fuzzer_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace js_in_process_fuzzer {

TEST(JsInProcessFuzzerUtilsTest, ReturnsFirstLine) {
  EXPECT_EQ(GetFirstNonCommentLine("const x = 1;\nconst y = 2;"),
            "const x = 1;");
}

TEST(JsInProcessFuzzerUtilsTest, SkipsLeadingComments) {
  EXPECT_EQ(GetFirstNonCommentLine("// Saved by Fuzzilli.\n"
                                   "  // Another comment.\n"
                                   "  const x = 1;  \n"
                                   "const y = 2;"),
            "const x = 1;");
}

TEST(JsInProcessFuzzerUtilsTest, ReturnsEmptyWithoutNonCommentLine) {
  EXPECT_EQ(GetFirstNonCommentLine(""), "");
  EXPECT_EQ(GetFirstNonCommentLine("// Only a comment."), "");
  EXPECT_EQ(GetFirstNonCommentLine("// A comment.\n// Another comment.\n"), "");
}

}  // namespace js_in_process_fuzzer
