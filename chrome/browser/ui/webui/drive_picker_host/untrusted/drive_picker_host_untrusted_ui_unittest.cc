// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class DrivePickerUntrustedHostUITest : public testing::Test {
 public:
  DrivePickerUntrustedHostUITest() = default;
  ~DrivePickerUntrustedHostUITest() override = default;

  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(DrivePickerUntrustedHostUITest, IsWebUIEnabled_FeatureEnabled) {
  DrivePickerUntrustedHostUIConfig config;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_TRUE(config.IsWebUIEnabled(profile()));
}

TEST_F(DrivePickerUntrustedHostUITest, IsWebUIEnabled_FeatureDisabled) {
  DrivePickerUntrustedHostUIConfig config;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_FALSE(config.IsWebUIEnabled(profile()));
}
