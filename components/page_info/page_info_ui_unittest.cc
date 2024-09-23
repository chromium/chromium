// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/page_info_ui.h"

#include "build/buildflag.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class MockPageInfoUiDelegate : public PageInfoUiDelegate {
 public:
#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(bool, IsBlockAutoPlayEnabled, (), (override));
  MOCK_METHOD(bool, IsMultipleTabsOpen, (), (override));
  MOCK_METHOD(void, OpenSiteSettingsFileSystem, (), (override));
#endif
  MOCK_METHOD(bool, IsTrackingProtection3pcdEnabled, (), (override));
  MOCK_METHOD(content::PermissionResult,
              GetPermissionResult,
              (blink::PermissionType permission),
              (override));
  MOCK_METHOD(std::optional<content::PermissionResult>,
              GetEmbargoResult,
              (ContentSettingsType type),
              (override));
};

}  // namespace

TEST(PageInfoUITest, PermissionStateToUIString) {
  MockPageInfoUiDelegate delegate;
  PageInfo::PermissionInfo permission_info;
  permission_info.setting = CONTENT_SETTING_ASK;

  permission_info.type = ContentSettingsType::KEYBOARD_LOCK;
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_STATE_TEXT_KEYBOARD_LOCK_ASK),
      PageInfoUI::PermissionStateToUIString(&delegate, permission_info));

  permission_info.type = ContentSettingsType::POINTER_LOCK;
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_STATE_TEXT_POINTER_LOCK_ASK),
      PageInfoUI::PermissionStateToUIString(&delegate, permission_info));
}
