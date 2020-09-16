// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_util.h"

#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector_icon_types.h"

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithNoneLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::NONE);
  EXPECT_EQ(icon.name, omnibox::kHttpIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithSecureLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE);
  EXPECT_EQ(icon.name, omnibox::kHttpsValidIcon.name);
}

TEST(LocationBarModelUtilTest,
     GetSecurityVectorIconWithSecureWithPolicyInstalledCertLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT);
  EXPECT_EQ(icon.name, vector_icons::kBusinessIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithDangerousLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::DANGEROUS);
  EXPECT_EQ(icon.name, omnibox::kNotSecureWarningIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithWarningLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::WARNING);
  if (security_state::ShouldShowDangerTriangleForWarningLevel()) {
    EXPECT_EQ(icon.name, omnibox::kNotSecureWarningIcon.name);
  } else {
    EXPECT_EQ(icon.name, omnibox::kHttpIcon.name);
  }
}
