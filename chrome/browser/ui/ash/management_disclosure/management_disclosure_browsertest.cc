// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/quick_settings_header.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ui/ash/management_disclosure/management_disclosure_client_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class ManagementDisclosureTest : public LoginManagerTest {
 public:
  ManagementDisclosureTest() { login_manager_.AppendRegularUsers(1); }

  ManagementDisclosureTest(const ManagementDisclosureTest&) = delete;
  ManagementDisclosureTest& operator=(const ManagementDisclosureTest&) = delete;

  ~ManagementDisclosureTest() override = default;

  void SetUpOnMainThread() override { LoginManagerTest::SetUpOnMainThread(); }

 protected:
  LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ManagementDisclosureTest, CheckContents) {
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});
  const AccountId test_account_id = login_manager_.users().front().account_id;
  LoginUser(test_account_id);

  ScreenLockerTester locker_tester;
  locker_tester.Lock();
  EXPECT_EQ(1, LoginScreenTestApi::GetUsersCount());

  LockScreen::TestApi lock_screen_test_api(LockScreen::Get());
  LockContentsViewTestApi contents_view_test_api(
      lock_screen_test_api.contents_view());

  QuickSettingsHeader::ShowEnterpriseInfo(
      nullptr, /*showManagementDisclosureDialog*/ true,
      /*IsUserSessionBlocked*/ true, /*hasEnterpriseDomainManager*/ true);
  auto dialog = contents_view_test_api.management_disclosure_dialog();
  ASSERT_TRUE(dialog);

  EXPECT_FALSE(dialog->ShouldShowWindowTitle());
  EXPECT_EQ(ui::mojom::ModalType::kSystem, dialog->GetModalType());

  // Buttons.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
            dialog->GetOkButton()->GetText());
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk), dialog->buttons());
  EXPECT_FALSE(dialog->ShouldShowCloseButton());

  // Labels.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE),
      static_cast<views::Label*>(
          dialog->GetViewByID(IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE))
          ->GetText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_CONFIGURATION),
            static_cast<views::Label*>(
                dialog->GetViewByID(IDS_MANAGEMENT_DEVICE_CONFIGURATION))
                ->GetText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_OPEN_CHROME_MANAGEMENT),
            static_cast<views::Label*>(
                dialog->GetViewByID(IDS_MANAGEMENT_OPEN_CHROME_MANAGEMENT))
                ->GetText());
}

}  // namespace ash
