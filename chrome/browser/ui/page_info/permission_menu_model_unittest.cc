// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/permission_menu_model.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCallback {
 public:
  TestCallback() : current_(-1) {}

  PermissionMenuModel::ChangeCallback callback() {
    return base::Bind(&TestCallback::PermissionChanged, base::Unretained(this));
  }
  void PermissionChanged(const PageInfoUI::PermissionInfo& permission) {
    current_ = permission.setting;
  }

  int current_;
};

class PermissionMenuModelTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

}  // namespace

TEST_F(PermissionMenuModelTest, TestDefault) {
  TestCallback callback;
  PageInfoUI::PermissionInfo permission;
  permission.type = ContentSettingsType::COOKIES;
  permission.setting = CONTENT_SETTING_ALLOW;
  permission.default_setting = CONTENT_SETTING_ALLOW;
  permission.source = content_settings::SETTING_SOURCE_USER;
  permission.is_incognito = false;

  PermissionMenuModel model(profile(), GURL("http://www.google.com"),
                            permission, callback.callback());
  EXPECT_EQ(3, model.GetItemCount());
}

TEST_F(PermissionMenuModelTest, TestDefaultMediaHttp) {
  for (int i = 0; i < 2; ++i) {
    ContentSettingsType type = i ? ContentSettingsType::MEDIASTREAM_MIC
                                 : ContentSettingsType::MEDIASTREAM_CAMERA;
    TestCallback callback;
    PageInfoUI::PermissionInfo permission;
    permission.type = type;
    permission.setting = CONTENT_SETTING_ALLOW;
    permission.default_setting = CONTENT_SETTING_ALLOW;
    permission.source = content_settings::SETTING_SOURCE_USER;
    permission.is_incognito = false;
    PermissionMenuModel model(profile(), GURL("http://www.google.com"),
                              permission, callback.callback());
    EXPECT_EQ(2, model.GetItemCount());
  }
}

TEST_F(PermissionMenuModelTest, TestIncognitoNotifications) {
  TestCallback callback;
  PageInfoUI::PermissionInfo permission;
  permission.type = ContentSettingsType::NOTIFICATIONS;
  permission.setting = CONTENT_SETTING_ASK;
  permission.default_setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;

  permission.is_incognito = false;
  PermissionMenuModel regular_model(profile(), GURL("https://www.google.com"),
                                    permission, callback.callback());
  EXPECT_EQ(3, regular_model.GetItemCount());

  permission.is_incognito = true;
  PermissionMenuModel incognito_model(profile(), GURL("https://www.google.com"),
                                      permission, callback.callback());
  EXPECT_EQ(2, incognito_model.GetItemCount());
}

TEST_F(PermissionMenuModelTest, TestUsbGuard) {
  TestCallback callback;
  PageInfoUI::PermissionInfo permission;
  permission.type = ContentSettingsType::USB_GUARD;
  permission.setting = CONTENT_SETTING_ASK;
  permission.default_setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;
  permission.is_incognito = false;

  PermissionMenuModel model(profile(), GURL("http://www.google.com"),
                            permission, callback.callback());
  EXPECT_EQ(3, model.GetItemCount());
}

TEST_F(PermissionMenuModelTest, TestSerialGuard) {
  const GURL kUrl("http://www.google.com");
  TestCallback callback;
  PageInfoUI::PermissionInfo permission;
  permission.type = ContentSettingsType::SERIAL_GUARD;
  permission.setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;
  permission.is_incognito = false;

  permission.default_setting = CONTENT_SETTING_ASK;
  PermissionMenuModel default_ask_model(profile(), kUrl, permission,
                                        callback.callback());
  ASSERT_EQ(3, default_ask_model.GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("Ask (default)"),
            default_ask_model.GetLabelAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("Block"), default_ask_model.GetLabelAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("Ask"), default_ask_model.GetLabelAt(2));

  permission.default_setting = CONTENT_SETTING_BLOCK;
  PermissionMenuModel default_block_model(profile(), kUrl, permission,
                                          callback.callback());
  ASSERT_EQ(3, default_block_model.GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("Block (default)"),
            default_block_model.GetLabelAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("Block"), default_block_model.GetLabelAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("Ask"), default_block_model.GetLabelAt(2));
}

TEST_F(PermissionMenuModelTest, TestBluetoothScanning) {
  const GURL kUrl("http://www.google.com");
  TestCallback callback;
  PageInfoUI::PermissionInfo permission;
  permission.type = ContentSettingsType::BLUETOOTH_SCANNING;
  permission.setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;
  permission.is_incognito = false;

  permission.default_setting = CONTENT_SETTING_ASK;
  PermissionMenuModel default_ask_model(profile(), kUrl, permission,
                                        callback.callback());
  ASSERT_EQ(3, default_ask_model.GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("Ask (default)"),
            default_ask_model.GetLabelAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("Block"), default_ask_model.GetLabelAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("Ask"), default_ask_model.GetLabelAt(2));

  permission.default_setting = CONTENT_SETTING_BLOCK;
  PermissionMenuModel default_block_model(profile(), kUrl, permission,
                                          callback.callback());
  ASSERT_EQ(3, default_block_model.GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("Block (default)"),
            default_block_model.GetLabelAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("Block"), default_block_model.GetLabelAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("Ask"), default_block_model.GetLabelAt(2));
}
