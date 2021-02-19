// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/browser/about_flags.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/flags_ui/feature_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeLabsBubbleViewModelTest : public ChromeViewsTestBase {};

TEST_F(ChromeLabsBubbleViewModelTest, CheckFeaturesHaveSupportedTypes) {
  std::unique_ptr<ChromeLabsBubbleViewModel> model =
      std::make_unique<ChromeLabsBubbleViewModel>();
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();

  // Make sure feature flags are set in about_flags, because a previous
  // test might have cleared them.
  size_t num_features;
  std::vector<flags_ui::FeatureEntry> feature_vec;
  const flags_ui::FeatureEntry* feature_array =
      about_flags::testing::GetFeatureEntries(&num_features);
  for (size_t i = 0; i < num_features; ++i)
    feature_vec.push_back(feature_array[i]);

  about_flags::testing::SetFeatureEntries(feature_vec);

  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
            lab.internal_name);
    EXPECT_TRUE(entry->type == flags_ui::FeatureEntry::FEATURE_VALUE ||
                entry->type ==
                    flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  }
}
