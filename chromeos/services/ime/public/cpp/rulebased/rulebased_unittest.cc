// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"

#include "chromeos/services/ime/public/cpp/rulebased/engine.h"
#include "chromeos/services/ime/public/cpp/rulebased/rules_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace ime {

namespace {

struct KeyVerifyEntry {
  const char* key;
  uint8_t modifiers;
  const wchar_t* expected_str;
};

}  // namespace

class RulebasedImeTest : public testing::Test {
 protected:
  RulebasedImeTest() = default;
  ~RulebasedImeTest() override = default;

  // testing::Test:
  void SetUp() override { engine_.reset(new rulebased::Engine); }

  void VerifyKeys(std::vector<KeyVerifyEntry> entries) {
    for (auto entry : entries) {
      rulebased::ProcessKeyResult res =
          engine_->ProcessKey(entry.key, entry.modifiers);
      EXPECT_TRUE(res.key_handled);
      std::string expected_str = base::WideToUTF8(entry.expected_str);
      EXPECT_EQ(expected_str, res.commit_text);
    }
  }

  std::unique_ptr<rulebased::Engine> engine_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RulebasedImeTest);
};

TEST_F(RulebasedImeTest, Arabic) {
  engine_->Activate("ar");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyA", rulebased::MODIFIER_SHIFT, L"\u0650"});
  entries.push_back({"KeyB", 0, L"\u0644\u0627"});
  entries.push_back({"Space", 0, L" "});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, Persian) {
  engine_->Activate("fa");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyA", 0, L"\u0634"});
  entries.push_back({"KeyV", rulebased::MODIFIER_SHIFT, L""});
  entries.push_back({"Space", rulebased::MODIFIER_SHIFT, L"\u200c"});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, Thai) {
  engine_->Activate("th");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyA", 0, L"\u0e1f"});
  entries.push_back({"KeyA", rulebased::MODIFIER_ALTGR, L""});
  VerifyKeys(entries);

  engine_->Activate("th_pattajoti");
  entries.clear();
  entries.push_back({"KeyA", 0, L"\u0e49"});
  entries.push_back({"KeyB", rulebased::MODIFIER_SHIFT, L"\u0e31\u0e49"});
  VerifyKeys(entries);

  engine_->Activate("th_tis");
  entries.clear();
  entries.push_back({"KeyA", 0, L"\u0e1f"});
  entries.push_back({"KeyM", rulebased::MODIFIER_SHIFT, L"?"});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, ParseKeyMap) {
  // Empty.
  rulebased::KeyMap key_map = rulebased::ParseKeyMapForTesting(L"", false);
  EXPECT_TRUE(key_map.empty());

  // Single char mapping.
  key_map = rulebased::ParseKeyMapForTesting(L"abcde", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);

  // Brackets for multiple chars.
  key_map = rulebased::ParseKeyMapForTesting(L"ab{{cc}}de", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);
  EXPECT_EQ("cc", key_map["Digit2"]);

  key_map = rulebased::ParseKeyMapForTesting(L"ab((cc))de", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);
  EXPECT_EQ("cc", key_map["Digit2"]);

  // Brackets for empty.
  key_map = rulebased::ParseKeyMapForTesting(L"ab{{}}de", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);
  EXPECT_EQ("", key_map["Digit2"]);

  // Incorrect brackets.
  key_map = rulebased::ParseKeyMapForTesting(L"ab{{cc))de", false);
  EXPECT_EQ(10UL, key_map.size());

  // End with brackets.
  key_map = rulebased::ParseKeyMapForTesting(L"abc{{", false);
  EXPECT_EQ(5UL, key_map.size());
}

}  // namespace ime
}  // namespace chromeos
