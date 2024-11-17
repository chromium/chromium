// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_constants.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondWebContents);

enum class TestContentSettings {
  kKeyboardLock,
  kPointerLock,
  kKeyboardAndPointerLock,
};

}  // namespace

class ExclusiveAccessPermissionPromptInteractiveTest
    : public InteractiveBrowserTest {
 public:
  ExclusiveAccessPermissionPromptInteractiveTest() {
    feature_list_.InitAndEnableFeature(
        permissions::features::kKeyboardAndPointerLockPrompt);
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(&https_server_);
    https_server_.StartAcceptingConnections();
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InteractiveBrowserTest::TearDownOnMainThread();
    EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
  }

 protected:
  void TestPermissionPrompt(TestContentSettings test_content_settings,
                            ContentSetting expected_value) {
    if (test_content_settings == TestContentSettings::kPointerLock) {
      RunTestSequence(CheckPointerLockPrompt(/*displayed=*/false),
                      ShowPrompt(test_content_settings),
                      CheckPointerLockPrompt(/*displayed=*/true),
                      PressPromptButton(GetButtonViewId(expected_value)),
                      CheckOutcome(test_content_settings, expected_value));
    } else {
      RunTestSequence(ShowPrompt(test_content_settings),
                      PressPromptButton(GetButtonViewId(expected_value)),
                      CheckOutcome(test_content_settings, expected_value));
    }
  }

  MultiStep ShowPrompt(TestContentSettings test_content_settings) {
    return Steps(InstrumentTab(kWebContents),
                 NavigateWebContents(kWebContents, GetURL()),
                 FocusWebContents(kWebContents),
                 ExecuteJsAt(kWebContents,
                             DeepQuery{GetHtmlElementId(test_content_settings)},
                             "click"));
  }

  MultiStep PressPromptButton(ui::ElementIdentifier button_identifier) {
    return InAnyContext(
        Steps(WaitForShow(button_identifier), PressButton(button_identifier),
              WaitForHide(ExclusiveAccessPermissionPromptView::kMainViewId)));
  }

  MultiStep CheckOutcome(TestContentSettings test_content_settings,
                         ContentSetting expected_value) {
    return Steps(CheckResult(
        [=, this]() {
          HostContentSettingsMap* hcsm =
              HostContentSettingsMapFactory::GetForProfile(
                  browser()->profile());
          for (const auto& type : GetContentSettings(test_content_settings)) {
            if (hcsm->GetContentSetting(GetOrigin(), GetOrigin(), type) !=
                expected_value) {
              return false;
            }
          }
          return true;
        },
        true));
  }

  // Adds a Step to validate that the pointer lock prompt is displayed.
  MultiStep CheckPointerLockPrompt(bool displayed) {
    return Steps(CheckResult(
        [=, this]() {
          return static_cast<content::WebContentsDelegate*>(browser())
              ->IsWaitingForPointerLockPrompt(
                  browser()->tab_strip_model()->GetActiveWebContents());
        },
        displayed));
  }

  ui::ElementIdentifier GetButtonViewId(ContentSetting expected_value) {
    switch (expected_value) {
      case CONTENT_SETTING_ALLOW:
        return ExclusiveAccessPermissionPromptView::kAlwaysAllowId;
      case CONTENT_SETTING_BLOCK:
        return ExclusiveAccessPermissionPromptView::kNeverAllowId;
      default:
        NOTREACHED();
    }
  }

  GURL GetOrigin() { return url::Origin::Create(GetURL()).GetURL(); }

  GURL GetURL() {
    return https_server_.GetURL(
        "a.test", "/permissions/exclusive_access_permissions.html");
  }

  std::string GetHtmlElementId(TestContentSettings test_content_settings) {
    switch (test_content_settings) {
      case TestContentSettings::kKeyboardLock:
        return "#keyboard-lock";
      case TestContentSettings::kPointerLock:
        return "#pointer-lock";
      case TestContentSettings::kKeyboardAndPointerLock:
        return "#keyboard-and-pointer-lock";
    }
  }

  std::vector<ContentSettingsType> GetContentSettings(
      TestContentSettings test_content_settings) {
    switch (test_content_settings) {
      case TestContentSettings::kKeyboardLock:
        return {ContentSettingsType::KEYBOARD_LOCK};
      case TestContentSettings::kPointerLock:
        return {ContentSettingsType::POINTER_LOCK};
      case TestContentSettings::kKeyboardAndPointerLock:
        return {ContentSettingsType::KEYBOARD_LOCK,
                ContentSettingsType::POINTER_LOCK};
    }
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowKeyboardLock) {
  TestPermissionPrompt(TestContentSettings::kKeyboardLock,
                       CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       BlockKeyboardLock) {
  TestPermissionPrompt(TestContentSettings::kKeyboardLock,
                       CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowPointerLock) {
  TestPermissionPrompt(TestContentSettings::kPointerLock,
                       CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       BlockPointerLock) {
  TestPermissionPrompt(TestContentSettings::kPointerLock,
                       CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowKeyboardLockAndPointerLock) {
  TestPermissionPrompt(TestContentSettings::kKeyboardAndPointerLock,
                       CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       BlockKeyboardLockAndPointerLock) {
  TestPermissionPrompt(TestContentSettings::kKeyboardAndPointerLock,
                       CONTENT_SETTING_BLOCK);
}

// Validates that the pointer lock request which is initiated from the
// RenderWidgetHostImpl instance is isolated to the instance which
// initiated the pointer lock request. For context on a page with iframes
// each iframe (security context) would have its own RWHI instance.
// Please note that this requires the iframes to exist in different site
// instances.
//
// TODO(crbug.com/371112529): Add similar tests for pointer lock widget in
// content.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       PointerLockIsPerRenderWidgetHost) {
  // Step 1: Navigate to the custom page with two sandboxed iframes
  GURL main_url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("pointer_lock")),
      base::FilePath(FILE_PATH_LITERAL("page_with_two_iframes.html")));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* frame1 = nullptr;
  content::RenderFrameHost* frame2 = nullptr;

  // Step 2: Use ForEachRenderFrameHost to find both iframes
  // TODO: Use FrameTree to find the iframes we are interested in.
  web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* frame) {
    std::string frame_name = frame->GetFrameName();
    if (frame_name == "frame1") {
      frame1 = frame;
    } else if (frame_name == "frame2") {
      frame2 = frame;
    }
  });

  // Ensure that both iframes are found
  ASSERT_TRUE(frame1);
  ASSERT_TRUE(frame2);

  RunTestSequence(
      InstrumentTab(kWebContents), FocusWebContents(kWebContents),
      ExecuteJsAt(kWebContents, DeepQuery{"#pointer-lock"}, "click"),
      // Step 5: Verify that the pointer lock prompt is displayed for frame1.
      CheckPointerLockPrompt(/*displayed=*/true),
      // Step 6: Interact with the pointer lock prompt (simulate clicking
      // "Allow").
      PressPromptButton(GetButtonViewId(CONTENT_SETTING_ALLOW)));

  // Step 6: Ensure that the pointer lock is granted for frame1 and not for
  // frame2.
  EXPECT_TRUE(frame1->GetRenderWidgetHost()->GetView()->IsPointerLocked());
  EXPECT_FALSE(frame2->GetRenderWidgetHost()->GetView()->IsPointerLocked());
}

// Verifies that we can select the permission prompt after losing focus
// to another tab and getting focus back.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowPointerLockAfterLosingFocus) {
  TestContentSettings test_content_settings = TestContentSettings::kPointerLock;

  RunTestSequence(
      // Step 1: Verify the pointer lock prompt is not displayed initially.
      CheckPointerLockPrompt(/*displayed=*/false),
      // Step 2: Navigate and trigger the pointer lock prompt.
      ShowPrompt(test_content_settings),
      // Step 3: Verify that the pointer lock prompt is now displayed.
      CheckPointerLockPrompt(/*displayed=*/true),
      // Step 4: Create a new tab and simulate losing focus by switching to it.
      AddInstrumentedTab(kSecondWebContents, GURL(url::kAboutBlankURL)),
      // Step 5: Focus the new tab.
      FocusWebContents(kSecondWebContents),
      // Step 6: Switch back to the original tab to regain focus.
      SelectTab(kTabStripElementId, 0), WaitForShow(kWebContents),
      // Step 7: Interact with the pointer lock prompt (simulate clicking
      // "Allow").
      PressPromptButton(GetButtonViewId(CONTENT_SETTING_ALLOW)),
      // Step 8: Verify the expected outcome (pointer lock should be granted).
      CheckOutcome(test_content_settings, CONTENT_SETTING_ALLOW));
}
