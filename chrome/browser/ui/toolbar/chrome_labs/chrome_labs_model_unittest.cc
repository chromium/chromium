// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/test/icu_test_util.h"
#include "chrome/browser/about_flags.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/flags_ui/feature_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
std::string CanonicalizeString(std::string original_string) {
  std::string new_string;
  // Trim common separator character used in about_flags
  char kSeparators[] = "-";
  base::RemoveChars(original_string, kSeparators, &new_string);
  return base::ToLowerASCII(
      base::TrimWhitespaceASCII(new_string, base::TRIM_ALL));
}
}  // namespace

class ChromeLabsModelTest : public ChromeViewsTestBase {};

TEST_F(ChromeLabsModelTest, CheckFeaturesHaveSupportedTypes) {
  std::unique_ptr<ChromeLabsModel> model = std::make_unique<ChromeLabsModel>();
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();

  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
            lab.internal_name);
    EXPECT_TRUE(entry->type == flags_ui::FeatureEntry::FEATURE_VALUE ||
                entry->type ==
                    flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  }
}

// Experiments in Chrome Labs must features of type
// FEATURE_WITH_PARAMS_VALUE must have variation descriptions in Chrome Labs
// match those declared in about_flags.
TEST_F(ChromeLabsModelTest, CheckFeatureWithParamsVariations) {
  base::test::ScopedRestoreICUDefaultLocale locale(std::string("en_US"));

  auto model = std::make_unique<ChromeLabsModel>();
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();
  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
            lab.internal_name);
    if (entry->type == flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE) {
      std::vector<std::u16string> translated_descriptions =
          lab.translated_feature_variation_descriptions;
      base::span<const flags_ui::FeatureEntry::FeatureVariation>
          feature_entry_descriptions = entry->feature.feature_variations;
      EXPECT_EQ(feature_entry_descriptions.size(),
                translated_descriptions.size());
      for (int i = 0; i < static_cast<int>(translated_descriptions.size());
           i++) {
        EXPECT_EQ(
            CanonicalizeString(base::UTF16ToUTF8(translated_descriptions[i])),
            CanonicalizeString(feature_entry_descriptions[i].description_text));
      }
    }
  }
}
