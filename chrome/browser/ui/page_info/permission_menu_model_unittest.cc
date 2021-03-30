// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/permission_menu_model.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCallback {
 public:
  TestCallback() : current_(-1) {}

  PermissionMenuModel::ChangeCallback callback() {
    return base::BindRepeating(&TestCallback::PermissionChanged,
                               base::Unretained(this));
  }
  void PermissionChanged(const PageInfo::PermissionInfo& permission) {
    current_ = permission.setting;
  }

  int current_;
};

class PermissionMenuModelTest : public testing::Test {
 public:
  PermissionMenuModelTest() { SetPageInfoUiDelegate(); }

 protected:
  TestingProfile* profile() { return &profile_; }
  PageInfoUiDelegate* delegate() { return delegate_.get(); }

  void SetOffTheRecordProfile() {
    delegate_ = std::make_unique<ChromePageInfoUiDelegate>(
        profile()->GetPrimaryOTRProfile(), GURL("http://www.google.com"));
  }

  void SetPageInfoUiDelegate() {
    delegate_ = std::make_unique<ChromePageInfoUiDelegate>(
        profile(), GURL("http://www.google.com"));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<PageInfoUiDelegate> delegate_;
};

}  // namespace

TEST_F(PermissionMenuModelTest, TestDefault) {
  TestCallback callback;
  PageInfo::PermissionInfo permission;
  permission.type = ContentSettingsType::COOKIES;
  permission.setting = CONTENT_SETTING_ALLOW;
  permission.default_setting = CONTENT_SETTING_ALLOW;
  permission.source = content_settings::SETTING_SOURCE_USER;

  PermissionMenuModel model(delegate(), permission, callback.callback());
  EXPECT_EQ(3, model.GetItemCount());
}

TEST_F(PermissionMenuModelTest, TestDefaultMediaHttp) {
  for (int i = 0; i < 2; ++i) {
    ContentSettingsType type = i ? ContentSettingsType::MEDIASTREAM_MIC
                                 : ContentSettingsType::MEDIASTREAM_CAMERA;
    TestCallback callback;
    PageInfo::PermissionInfo permission;
    permission.type = type;
    permission.setting = CONTENT_SETTING_ALLOW;
    permission.default_setting = CONTENT_SETTING_ALLOW;
    permission.source = content_settings::SETTING_SOURCE_USER;
    PermissionMenuModel model(delegate(), permission, callback.callback());
    EXPECT_EQ(2, model.GetItemCount());
  }
}

TEST_F(PermissionMenuModelTest, TestIncognitoNotifications) {
  TestCallback callback;
  PageInfo::PermissionInfo permission;
  permission.type = ContentSettingsType::NOTIFICATIONS;
  permission.setting = CONTENT_SETTING_ASK;
  permission.default_setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;

  PermissionMenuModel regular_model(delegate(), permission,
                                    callback.callback());
  EXPECT_EQ(3, regular_model.GetItemCount());

  SetOffTheRecordProfile();
  PermissionMenuModel incognito_model(delegate(), permission,
                                      callback.callback());
  EXPECT_EQ(2, incognito_model.GetItemCount());
  SetPageInfoUiDelegate();
}

TEST_F(PermissionMenuModelTest, TestUsbGuard) {
  TestCallback callback;
  PageInfo::PermissionInfo permission;
  permission.type = ContentSettingsType::USB_GUARD;
  permission.setting = CONTENT_SETTING_ASK;
  permission.default_setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;

  PermissionMenuModel model(delegate(), permission, callback.callback());
  EXPECT_EQ(3, model.GetItemCount());
}

TEST_F(PermissionMenuModelTest, TestSerialGuard) {
  TestCallback callback;
  PageInfo::PermissionInfo permission;
  permission.type = ContentSettingsType::SERIAL_GUARD;
  permission.setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;

  permission.default_setting = CONTENT_SETTING_ASK;
  PermissionMenuModel default_ask_model(delegate(), permission,
                                        callback.callback());
  ASSERT_EQ(3, default_ask_model.GetItemCount());
  EXPECT_EQ(u"Ask (default)", default_ask_model.GetLabelAt(0));
  EXPECT_EQ(u"Block", default_ask_model.GetLabelAt(1));
  EXPECT_EQ(u"Ask", default_ask_model.GetLabelAt(2));

  permission.default_setting = CONTENT_SETTING_BLOCK;
  PermissionMenuModel default_block_model(delegate(), permission,
                                          callback.callback());
  ASSERT_EQ(3, default_block_model.GetItemCount());
  EXPECT_EQ(u"Block (default)", default_block_model.GetLabelAt(0));
  EXPECT_EQ(u"Block", default_block_model.GetLabelAt(1));
  EXPECT_EQ(u"Ask", default_block_model.GetLabelAt(2));
}

TEST_F(PermissionMenuModelTest, TestBluetoothScanning) {
  TestCallback callback;
  PageInfo::PermissionInfo permission;
  permission.type = ContentSettingsType::BLUETOOTH_SCANNING;
  permission.setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;

  permission.default_setting = CONTENT_SETTING_ASK;
  PermissionMenuModel default_ask_model(delegate(), permission,
                                        callback.callback());
  ASSERT_EQ(3, default_ask_model.GetItemCount());
  EXPECT_EQ(u"Ask (default)", default_ask_model.GetLabelAt(0));
  EXPECT_EQ(u"Block", default_ask_model.GetLabelAt(1));
  EXPECT_EQ(u"Ask", default_ask_model.GetLabelAt(2));

  permission.default_setting = CONTENT_SETTING_BLOCK;
  PermissionMenuModel default_block_model(delegate(), permission,
                                          callback.callback());
  ASSERT_EQ(3, default_block_model.GetItemCount());
  EXPECT_EQ(u"Block (default)", default_block_model.GetLabelAt(0));
  EXPECT_EQ(u"Block", default_block_model.GetLabelAt(1));
  EXPECT_EQ(u"Ask", default_block_model.GetLabelAt(2));
}

TEST_F(PermissionMenuModelTest, TestHidGuard) {
  TestCallback callback;
  PageInfo::PermissionInfo permission;
  permission.type = ContentSettingsType::HID_GUARD;
  permission.setting = CONTENT_SETTING_ASK;
  permission.source = content_settings::SETTING_SOURCE_USER;

  permission.default_setting = CONTENT_SETTING_ASK;
  PermissionMenuModel default_ask_model(delegate(), permission,
                                        callback.callback());
  ASSERT_EQ(3, default_ask_model.GetItemCount());
  EXPECT_EQ(u"Ask (default)", default_ask_model.GetLabelAt(0));
  EXPECT_EQ(u"Block", default_ask_model.GetLabelAt(1));
  EXPECT_EQ(u"Ask", default_ask_model.GetLabelAt(2));

  permission.default_setting = CONTENT_SETTING_BLOCK;
  PermissionMenuModel default_block_model(delegate(), permission,
                                          callback.callback());
  ASSERT_EQ(3, default_block_model.GetItemCount());
  EXPECT_EQ(u"Block (default)", default_block_model.GetLabelAt(0));
  EXPECT_EQ(u"Block", default_block_model.GetLabelAt(1));
  EXPECT_EQ(u"Ask", default_block_model.GetLabelAt(2));
}
