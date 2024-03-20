// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/app_service/publisher_helper.h"

#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(PublisherHelperTest, ConvertWebAppManagementTypeToShortcutSource) {
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kSync),
            apps::ShortcutSource::kUser);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kWebAppStore),
            apps::ShortcutSource::kUser);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kOneDriveIntegration),
            apps::ShortcutSource::kUser);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kPolicy),
            apps::ShortcutSource::kPolicy);
  ASSERT_EQ(
      ConvertWebAppManagementTypeToShortcutSource(WebAppManagement::Type::kOem),
      apps::ShortcutSource::kDefault);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kApsDefault),
            apps::ShortcutSource::kDefault);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kDefault),
            apps::ShortcutSource::kDefault);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kKiosk),
            apps::ShortcutSource::kUnknown);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kSystem),
            apps::ShortcutSource::kUnknown);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kIwaShimlessRma),
            apps::ShortcutSource::kUnknown);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kIwaPolicy),
            apps::ShortcutSource::kPolicy);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kIwaUserInstalled),
            apps::ShortcutSource::kUser);
  ASSERT_EQ(ConvertWebAppManagementTypeToShortcutSource(
                WebAppManagement::Type::kSubApp),
            apps::ShortcutSource::kUnknown);
}

}  // namespace web_app
