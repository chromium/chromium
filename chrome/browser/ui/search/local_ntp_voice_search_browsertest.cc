// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class LocalNTPVoiceSearchSmokeTest : public InProcessBrowserTest {
 public:
  LocalNTPVoiceSearchSmokeTest() {}

 private:
  void SetUpCommandLine(base::CommandLine* cmdline) override {
    // Requesting microphone permission doesn't work unless there's a device
    // available.
    cmdline->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceSearchSmokeTest,
                       GoogleNTPWithVoiceLoadsWithoutError) {
  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  // Attach a console observer, listening for any message ("*" pattern).
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);

  // Navigate to the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  // Make sure the microphone icon in the fakebox is present and visible.
  bool fakebox_microphone_is_visible = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "!!document.getElementById('fakebox-microphone') && "
      "!document.getElementById('fakebox-microphone').hidden",
      &fakebox_microphone_is_visible));
  EXPECT_TRUE(fakebox_microphone_is_visible);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceSearchSmokeTest, MicrophonePermission) {
  // Open a new NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  PermissionRequestManager* request_manager =
      PermissionRequestManager::FromWebContents(active_tab);
  MockPermissionPromptFactory prompt_factory(request_manager);

  PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(browser()->profile());

  // Make sure microphone permission for the NTP isn't set yet.
  const PermissionResult mic_permission_before =
      permission_manager->GetPermissionStatusForFrame(
          ContentSettingsType::MEDIASTREAM_MIC, active_tab->GetMainFrame(),
          GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin());
  ASSERT_EQ(CONTENT_SETTING_ASK, mic_permission_before.content_setting);
  ASSERT_EQ(PermissionStatusSource::UNSPECIFIED, mic_permission_before.source);

  ASSERT_EQ(0, prompt_factory.TotalRequestCount());

  // Auto-approve the permissions bubble as soon as it shows up.
  prompt_factory.set_response_type(PermissionRequestManager::ACCEPT_ALL);

  // Click on the microphone button, which will trigger a permission request.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('fakebox-microphone').click();"));

  // Make sure the request arrived.
  prompt_factory.WaitForPermissionBubble();
  EXPECT_EQ(1, prompt_factory.show_count());
  EXPECT_EQ(1, prompt_factory.request_count());
  EXPECT_EQ(1, prompt_factory.TotalRequestCount());
  EXPECT_TRUE(prompt_factory.RequestTypeSeen(
      PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));
  // ...and that it showed the Google base URL, not the NTP URL.
  const GURL google_base_url(UIThreadSearchTermsData().GoogleBaseURLValue());
  EXPECT_TRUE(prompt_factory.RequestOriginSeen(google_base_url.GetOrigin()));
  EXPECT_FALSE(prompt_factory.RequestOriginSeen(
      GURL(chrome::kChromeUINewTabURL).GetOrigin()));
  EXPECT_FALSE(prompt_factory.RequestOriginSeen(
      GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin()));

  // Now microphone permission for the NTP should be set.
  const PermissionResult mic_permission_after =
      permission_manager->GetPermissionStatusForFrame(
          ContentSettingsType::MEDIASTREAM_MIC, active_tab->GetMainFrame(),
          GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, mic_permission_after.content_setting);
}
