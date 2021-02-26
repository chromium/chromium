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

  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
            lab.internal_name);
    EXPECT_TRUE(entry->type == flags_ui::FeatureEntry::FEATURE_VALUE ||
                entry->type ==
                    flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  }
}
