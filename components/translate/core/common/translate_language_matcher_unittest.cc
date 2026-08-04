// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_language_matcher.h"

#include <optional>
#include <string>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

TEST(TranslateLanguageMatcherTest, MatcherFallback) {
  const auto& matcher = GetTranslateLanguageMatcher();
  const auto& converter = base::i18n::LanguageTagConverter::GetInstance();

  // Norwegian (nb) maps to Norwegian (no).
  auto nb_tag = converter.FromString("nb");
  ASSERT_TRUE(nb_tag.has_value());
  auto matched = matcher.Match(*nb_tag);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ("no", matched->tag_string());

  // Indonesian (in) maps to Indonesian (id).
  auto in_tag = converter.FromString("in");
  ASSERT_TRUE(in_tag.has_value());
  matched = matcher.Match(*in_tag);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ("id", matched->tag_string());

  // Chinese cases.
  auto zh_hk_tag = converter.FromString("zh-HK");
  ASSERT_TRUE(zh_hk_tag.has_value());
  matched = matcher.Match(*zh_hk_tag);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ("zh-TW", matched->tag_string());

  auto zh_mo_tag = converter.FromString("zh-MO");
  ASSERT_TRUE(zh_mo_tag.has_value());
  matched = matcher.Match(*zh_mo_tag);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ("zh-TW", matched->tag_string());

  auto zh_sg_tag = converter.FromString("zh-SG");
  ASSERT_TRUE(zh_sg_tag.has_value());
  matched = matcher.Match(*zh_sg_tag);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ("zh-CN", matched->tag_string());

  auto cmn_hans_tag = converter.FromString("cmn-hans");
  ASSERT_TRUE(cmn_hans_tag.has_value());
  matched = matcher.Match(*cmn_hans_tag);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ("zh-CN", matched->tag_string());

  auto cmn_hant_tag = converter.FromString("cmn-hant");
  ASSERT_TRUE(cmn_hant_tag.has_value());
  matched = matcher.Match(*cmn_hant_tag);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ("zh-TW", matched->tag_string());
}

}  // namespace translate
