// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace test {
#include "components/url_formatter/spoof_checks/common_words/common_words_test-inc.cc"
}

using url_formatter::common_words::IsCommonWord;
using url_formatter::common_words::SetCommonWordDAFSAForTesting;

TEST(CommonWordsUtilTest, CommonWordsListContainsWhatsExpected) {
  SetCommonWordDAFSAForTesting(test::kDafsa);

  EXPECT_TRUE(IsCommonWord("alphabet"));
  EXPECT_FALSE(IsCommonWord("bravo"));

  url_formatter::common_words::ResetCommonWordDAFSAForTesting();
}
