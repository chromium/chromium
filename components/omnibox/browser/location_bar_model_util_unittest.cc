// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithNoneLevel) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_NONE;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::NONE, &visible_security_state);
  EXPECT_EQ(icon.name, omnibox::kHttpChromeRefreshIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithSecureLevel) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_NONE;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE, &visible_security_state);
  EXPECT_EQ(icon.name, omnibox::kSecurePageInfoChromeRefreshIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithDangerousLevel) {
  base::test::ScopedFeatureList scoped_feature_list_;
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::DANGEROUS, &visible_security_state);
  EXPECT_EQ(icon.name, vector_icons::kDangerousChromeRefreshIcon.name);
}

TEST(LocationBarModelUtilTest,
     GetSecurityVectorIconBillingInterstitialWithDangerousLevel) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_BILLING;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::DANGEROUS, &visible_security_state);
  EXPECT_EQ(icon.name, vector_icons::kNotSecureWarningChromeRefreshIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithWarningLevel) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::WARNING, &visible_security_state);
  EXPECT_EQ(icon.name, vector_icons::kNotSecureWarningChromeRefreshIcon.name);
}

TEST(
    LocationBarModelUtilTest,
    GetSecurityVectorIconWithWarningLevelAndHttpsFirstModeWarning_DialogUiEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      security_interstitials::features::kHttpsFirstDialogUi);
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_NONE;
  visible_security_state.is_https_only_mode_upgraded = true;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::WARNING, &visible_security_state);
  EXPECT_EQ(icon.name, vector_icons::kNoEncryptionIcon.name);
}

TEST(
    LocationBarModelUtilTest,
    GetSecurityVectorIconWithWarningLevelAndHttpsFirstModeWarning_DialogUiDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      security_interstitials::features::kHttpsFirstDialogUi);
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_NONE;
  visible_security_state.is_https_only_mode_upgraded = true;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::WARNING, &visible_security_state);
  EXPECT_EQ(icon.name, vector_icons::kNotSecureWarningChromeRefreshIcon.name);
}
