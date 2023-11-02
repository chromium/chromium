// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/chinese_script_classifier.h"

#include <string>
#include <utility>
#include <vector>
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace {

class ChineseScriptClassifierTest : public testing::Test {
 protected:
  ChineseScriptClassifier classifier_;
};

TEST_F(ChineseScriptClassifierTest, Simplified) {
  // ChineseScriptClassifier returns zh-Hans in this case.
  const std::vector<std::string> zh_hans_strings = {
      "正体字/繁体字", "台湾", "中国", "简化字", "经举发后仍不办理而行驶"};
  for (const auto& zh_hans_string : zh_hans_strings) {
    EXPECT_EQ("zh-Hans", classifier_.Classify(zh_hans_string));
  }
}

TEST_F(ChineseScriptClassifierTest, Traditional) {
  // ChineseScriptClassifier returns zh-Hant in this case.
  const std::vector<std::string> zh_hant_strings = {
      "正體字/繁體字", "臺灣", "美國", "簡化字", "經舉發後仍不辦理而行駛"};
  for (const auto& zh_hant_string : zh_hant_strings) {
    EXPECT_EQ("zh-Hant", classifier_.Classify(zh_hant_string));
  }
}

TEST_F(ChineseScriptClassifierTest, AmbiguousWithOnlyCharsValidForBothScripts) {
  // ChineseScriptClassifier returns zh-Hans in this case.
  const std::vector<std::string> zh_strings = {"我看到你", "你好",
                                               "我有很多工作要做"};
  for (const auto& zh_string : zh_strings) {
    EXPECT_EQ("zh-Hans", classifier_.Classify(zh_string)) << zh_string;
  }

  // ChineseScriptClassifier should not be used for non-Chinese text, but will
  // return zh-Hans in this case.
  const std::vector<std::string> non_zh_strings = {"", " ",
                                                   "This is English text."};
  for (const auto& non_zh_string : non_zh_strings) {
    EXPECT_EQ("zh-Hans", classifier_.Classify(non_zh_string)) << non_zh_string;
  }
}

TEST_F(ChineseScriptClassifierTest,
       AmbiguousWithMixedSimplifiedOnlyAndTraditionalOnly) {
  // ChineseScriptClassifier returns zh-Hans in this case.
  const std::vector<std::pair<std::string, std::string>> ambiguous_zh_strings =
      {
          // 4 zh-Hant chars and 1 zh-Hans char.
          {"國國國國国", "zh-Hant"},
          // 1 zh-Hant char and 4 zh-Hans chars.
          {"國国国国国", "zh-Hans"},
      };
  for (const auto& ambiguous_item : ambiguous_zh_strings) {
    EXPECT_EQ(ambiguous_item.second,
              classifier_.Classify(ambiguous_item.first));
  }
}

}  // namespace
}  // namespace translate
