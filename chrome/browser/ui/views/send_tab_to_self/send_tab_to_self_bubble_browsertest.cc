// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace send_tab_to_self {

namespace {

class TestSendTabToSelfBubbleController : public SendTabToSelfBubbleController {
 public:
  explicit TestSendTabToSelfBubbleController(content::WebContents* web_contents)
      : SendTabToSelfBubbleController(web_contents) {}
  ~TestSendTabToSelfBubbleController() override = default;

  std::vector<TargetDeviceInfo> GetValidDevices() override {
    const auto now = base::Time::Now();
    return {{"Device_1", "Device_1", "device_guid_1",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(0)},
            {"Device_2", "Device_2", "device_guid_2",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(0)},
            {"Device_3", "Device_3", "device_guid_3",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(1)},
            {"Device_4", "Device_4", "device_guid_4",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(1)},
            {"Device_5", "Device_5", "device_guid_5",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(5)},
            {"Device_6", "Device_6", "device_guid_6",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(5)}};
  }

  AccountInfo GetSharingAccountInfo() override {
    AccountInfo info;
    info.email = "user@host.com";
    info.account_image = gfx::Image(gfx::test::CreateImageSkia(96, 96));
    return info;
  }
};

}  // namespace

class SendTabToSelfBubbleTest : public DialogBrowserTest {
 public:
  SendTabToSelfBubbleTest() = default;

  SendTabToSelfBubbleTest(const SendTabToSelfBubbleTest&) = delete;
  SendTabToSelfBubbleTest& operator=(const SendTabToSelfBubbleTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->SetUserData(
        TestSendTabToSelfBubbleController::UserDataKey(),
        std::make_unique<TestSendTabToSelfBubbleController>(web_contents));

    if (name == "ShowDeviceList") {
      BrowserView::GetBrowserViewForBrowser(browser())
          ->ShowSendTabToSelfDevicePickerBubble(web_contents);
    } else if (name == "ShowSigninPromo") {
      BrowserView::GetBrowserViewForBrowser(browser())
          ->ShowSendTabToSelfPromoBubble(web_contents,
                                         /*show_signin_button=*/true);
    } else if (name == "ShowNoTargetDevicePromo") {
      BrowserView::GetBrowserViewForBrowser(browser())
          ->ShowSendTabToSelfPromoBubble(web_contents,
                                         /*show_signin_button=*/false);
    } else {
      NOTREACHED();
    }
  }
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest, InvokeUi_ShowDeviceList) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest, InvokeUi_ShowSigninPromo) {
  // Last updated in crrev.com/c/3776623.
  set_baseline("3776623");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest,
                       InvokeUi_ShowNoTargetDevicePromo) {
  // Last updated in crrev.com/c/3832669.
  set_baseline("3832669");
  ShowAndVerifyUi();
}

}  // namespace send_tab_to_self
