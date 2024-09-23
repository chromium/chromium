
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"

#include "base/command_line.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AlertIndicatorButtonBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<std::string> {
 public:
  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
    web_contents_ = OpenTestPageInNewTab("/video_conference_demo.html");

    // Set permissions for camera and microphone.
    SetPermission(web_contents_, ContentSettingsType::MEDIASTREAM_CAMERA,
                  CONTENT_SETTING_ALLOW);
    SetPermission(web_contents_, ContentSettingsType::MEDIASTREAM_MIC,
                  CONTENT_SETTING_ALLOW);

    // Assign the alert_indicator_button_.
    auto* tabstrip = static_cast<BrowserView*>(browser()->window())->tabstrip();
    alert_indicator_button_ =
        tabstrip->tab_at(tabstrip->GetActiveIndex().value())
            ->alert_indicator_button_for_testing();
  }

  content::WebContents* web_contents() { return web_contents_; }

  AlertIndicatorButton* alert_indicator_button() {
    return alert_indicator_button_;
  }

 private:
  // Allows or disallow permissions for `web_contents`.
  void SetPermission(content::WebContents* web_contents,
                     ContentSettingsType type,
                     ContentSetting result) {
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetContentSettingDefaultScope(web_contents->GetURL(), GURL(), type,
                                        result);
  }

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
  raw_ptr<AlertIndicatorButton, DanglingUntriaged> alert_indicator_button_ =
      nullptr;
};

// ToDo(b/323446918): this test fails for startScreenSharing on win10.
INSTANTIATE_TEST_SUITE_P(,  // Empty to simplify gtest output
                         AlertIndicatorButtonBrowserTest,
                         testing::ValuesIn(std::vector<std::string>{
                             "startVideo();", "startAudio();"}));

IN_PROC_BROWSER_TEST_P(AlertIndicatorButtonBrowserTest,
                       CapturingShouldShowAlertIndicatorButton) {
  const std::string action = GetParam();

  // alert_indicator_button_ should be invisible in the beginning.
  EXPECT_FALSE(alert_indicator_button()->GetVisible());

  // Prepare for the visibility change notification.
  base::RunLoop run_loop;
  auto callback_subscription =
      alert_indicator_button()->AddVisibleChangedCallback(
          run_loop.QuitClosure());

  // Start capturing.
  EXPECT_TRUE(content::ExecJs(web_contents(), action));

  // Wait for the visibility change to trigger.
  run_loop.Run();

  // alert_indicator_button_ should be visible now.
  EXPECT_TRUE(alert_indicator_button()->GetVisible());
}

}  // namespace
