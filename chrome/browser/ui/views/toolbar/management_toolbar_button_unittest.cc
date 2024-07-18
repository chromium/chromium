// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/management_toolbar_button.h"

#include "base/test/with_feature_override.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagementToolbarButtonUnitTest : public base::test::WithFeatureOverride,
                                        public TestWithBrowserView {
 public:
  ManagementToolbarButtonUnitTest()
      : base::test::WithFeatureOverride(features::kManagementToolbarButton) {}

  bool IsManagementToolbarButtonEnabled() const {
    return IsParamFeatureEnabled();
  }
};

TEST_P(ManagementToolbarButtonUnitTest, Visibility) {
  auto* management_toolbar_button =
      browser_view()->toolbar()->management_toolbar_button();
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(nullptr, management_toolbar_button);
#else
  ASSERT_NE(nullptr, management_toolbar_button);
  EXPECT_EQ(IsManagementToolbarButtonEnabled(),
            management_toolbar_button->GetVisible());
  EXPECT_TRUE(management_toolbar_button->GetText().empty());

  GetProfile()->GetPrefs()->SetString(prefs::kEnterpriseCustomLabel, "value");
  EXPECT_TRUE(management_toolbar_button->GetVisible());
  EXPECT_EQ(u"value", management_toolbar_button->GetText());

  GetProfile()->GetPrefs()->ClearPref(prefs::kEnterpriseCustomLabel);
  EXPECT_EQ(IsManagementToolbarButtonEnabled(),
            management_toolbar_button->GetVisible());
  EXPECT_TRUE(management_toolbar_button->GetText().empty());

  GetProfile()->GetPrefs()->SetString(prefs::kEnterpriseLogoUrl, "value");
  EXPECT_TRUE(management_toolbar_button->GetVisible());
  EXPECT_TRUE(management_toolbar_button->GetText().empty());

  GetProfile()->GetPrefs()->ClearPref(prefs::kEnterpriseLogoUrl);
  EXPECT_EQ(IsManagementToolbarButtonEnabled(),
            management_toolbar_button->GetVisible());
#endif
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ManagementToolbarButtonUnitTest);
