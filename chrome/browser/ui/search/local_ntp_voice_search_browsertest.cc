// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using LocalNTPVoiceSearchSmokeTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceSearchSmokeTest,
                       GoogleNTPWithVoiceLoadsWithoutError) {
  // Open a new blank tab.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ASSERT_FALSE(search::IsInstantNTP(active_tab));

  content::WebContentsConsoleObserver console_observer(active_tab);

  // Navigate to the local NTP.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  // Make sure the microphone icon in the fakebox is present and visible.
  bool microphone_is_visible = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "!!document.getElementById('realbox-microphone') && "
      "!document.getElementById('realbox-microphone').hidden",
      &microphone_is_visible));
  EXPECT_TRUE(microphone_is_visible);

  // We shouldn't have gotten any console error messages.
  EXPECT_TRUE(console_observer.messages().empty())
      << console_observer.GetMessageAt(0u);
}

// Test is flaky: crbug.com/790963.
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_MicrophonePermission DISABLED_MicrophonePermission
#else
#define MAYBE_MicrophonePermission MicrophonePermission
#endif
IN_PROC_BROWSER_TEST_F(LocalNTPVoiceSearchSmokeTest,
                       MAYBE_MicrophonePermission) {
  // Open a new local NTP.
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeSearchLocalNtpUrl));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            active_tab->GetController().GetVisibleEntry()->GetURL());

  permissions::PermissionRequestManager* request_manager =
      permissions::PermissionRequestManager::FromWebContents(active_tab);
  permissions::MockPermissionPromptFactory prompt_factory(request_manager);

  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(browser()->profile());

  // Make sure microphone permission for the NTP isn't set yet.
  const permissions::PermissionResult mic_permission_before =
      permission_manager->GetPermissionStatusForFrame(
          ContentSettingsType::MEDIASTREAM_MIC, active_tab->GetMainFrame(),
          GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin());
  ASSERT_EQ(CONTENT_SETTING_ASK, mic_permission_before.content_setting);
  ASSERT_EQ(permissions::PermissionStatusSource::UNSPECIFIED,
            mic_permission_before.source);

  ASSERT_EQ(0, prompt_factory.TotalRequestCount());

  // Auto-approve the permissions bubble as soon as it shows up.
  prompt_factory.set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Click on the microphone button, which will trigger a permission request.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('realbox-microphone').click();"));

  // Make sure the request arrived.
  prompt_factory.WaitForPermissionBubble();
  EXPECT_EQ(1, prompt_factory.show_count());
  EXPECT_EQ(1, prompt_factory.request_count());
  EXPECT_EQ(1, prompt_factory.TotalRequestCount());
  EXPECT_TRUE(prompt_factory.RequestTypeSeen(
      permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));
  // ...and that it showed the local NTP URL.
  EXPECT_FALSE(prompt_factory.RequestOriginSeen(
      GURL(chrome::kChromeUINewTabURL).GetOrigin()));
  EXPECT_TRUE(prompt_factory.RequestOriginSeen(
      GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin()));

  // Now microphone permission for the NTP should be set.
  const permissions::PermissionResult mic_permission_after =
      permission_manager->GetPermissionStatusForFrame(
          ContentSettingsType::MEDIASTREAM_MIC, active_tab->GetMainFrame(),
          GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, mic_permission_after.content_setting);
}
