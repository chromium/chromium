// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/permissions_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace cast_receiver {

// TODO(crbug.com/40236247): Add tests for ApplicationStateObserver.
class PermissionsManagerImplTest : public testing::Test {
 public:
  PermissionsManagerImplTest()
      : app_url_("https://www.netflix.com"), permissions_manager_(kAppId) {
    permissions_manager_.AddOrigin(url::Origin::Create(app_url_));
  }
  ~PermissionsManagerImplTest() override = default;

  bool HasPermission(blink::PermissionType permission, const GURL& origin) {
    return permissions_manager_.GetPermissionStatus(permission, origin) ==
           blink::mojom::PermissionStatus::GRANTED;
  }

 protected:
  const std::string kAppId = "app id";
  const GURL app_url_;

  PermissionsManagerImpl permissions_manager_;
};

TEST_F(PermissionsManagerImplTest, Test) {
  const GURL http_url("http://www.netflix.com");
  const GURL sub_url("https://www.netflix.com/some/tv/show");
  const GURL other_url("http://www.amazon.com");
  EXPECT_EQ(kAppId, permissions_manager_.GetAppId());

  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, app_url_));
  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, http_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, sub_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, other_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::MIDI, app_url_));
  EXPECT_FALSE(HasPermission(blink::PermissionType::DURABLE_STORAGE, app_url_));

  permissions_manager_.AddPermission(blink::PermissionType::NOTIFICATIONS);
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, app_url_));
  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, http_url));
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, sub_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, other_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::MIDI, app_url_));
  EXPECT_FALSE(HasPermission(blink::PermissionType::DURABLE_STORAGE, app_url_));

  permissions_manager_.AddOrigin(url::Origin::Create(http_url));
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, app_url_));
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, http_url));
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, sub_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, other_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::MIDI, app_url_));
  EXPECT_FALSE(HasPermission(blink::PermissionType::DURABLE_STORAGE, app_url_));

  permissions_manager_.AddPermission(blink::PermissionType::MIDI);
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, app_url_));
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, http_url));
  EXPECT_TRUE(HasPermission(blink::PermissionType::NOTIFICATIONS, sub_url));
  EXPECT_FALSE(HasPermission(blink::PermissionType::NOTIFICATIONS, other_url));
  EXPECT_TRUE(HasPermission(blink::PermissionType::MIDI, app_url_));
  EXPECT_FALSE(HasPermission(blink::PermissionType::DURABLE_STORAGE, app_url_));
}

}  // namespace cast_receiver
