// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_util.h"

#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector_icon_types.h"

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithNoneLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::NONE,
      /*use_updated_connection_security_indicators=*/false);
  EXPECT_EQ(icon.name, omnibox::kHttpIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithSecureLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE,
      /*use_updated_connection_security_indicators=*/false);
  EXPECT_EQ(icon.name, vector_icons::kHttpsValidIcon.name);
}

TEST(LocationBarModelUtilTest,
     GetSecurityVectorIconWithSecureLevelUpdatedIcon) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE,
      /*use_updated_connection_security_indicators=*/true);
  EXPECT_EQ(icon.name, vector_icons::kHttpsValidArrowIcon.name);
}

TEST(LocationBarModelUtilTest,
     GetSecurityVectorIconWithSecureWithPolicyInstalledCertLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT,
      /*use_updated_connection_security_indicators=*/false);
  EXPECT_EQ(icon.name, vector_icons::kBusinessIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithDangerousLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::DANGEROUS,
      /*use_updated_connection_security_indicators=*/false);
  EXPECT_EQ(icon.name, vector_icons::kNotSecureWarningIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithWarningLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::WARNING,
      /*use_updated_connection_security_indicators=*/false);
  EXPECT_EQ(icon.name, vector_icons::kNotSecureWarningIcon.name);
}
