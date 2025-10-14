// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/to_string.h"
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
#include "ui/views/views_switches.h"
#include "url/url_constants.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents);

enum class TestContentSettings {
  kKeyboardLock,
  kPointerLock,
  kKeyboardAndPointerLock,
};

}  // namespace

class ExclusiveAccessPermissionPromptInteractiveTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExclusiveAccessPermissionPromptInteractiveTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        permissions::features::kKeyboardLockPrompt,
        {{"use_pepc_ui", base::ToString(GetParam())}});
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTestMixin::SetUpCommandLine(command_line);
    // Disables the disregarding of potentially unintended input events.
    command_line->AppendSwitch(
        views::switches::kDisableInputEventActivationProtectionForTesting);
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
                 ClickOnElement(test_content_settings));
  }

  MultiStep ClickOnElement(TestContentSettings test_content_settings) {
    return Steps(ExecuteJsAt(kWebContents,
                             DeepQuery{GetHtmlElementId(test_content_settings)},
                             "click"));
  }

  MultiStep PressPromptButton(ui::ElementIdentifier button_identifier) {
    return InAnyContext(
        WaitForShow(button_identifier), PressButton(button_identifier),
        WaitForHide(ExclusiveAccessPermissionPromptView::kMainViewId));
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
        if (permissions::feature_params::kKeyboardLockPromptUIStyle.Get()) {
          return ExclusiveAccessPermissionPromptView::kAlwaysAllowId;
        } else {
          return PermissionPromptBubbleBaseView::kAllowButtonElementId;
        }
      case CONTENT_SETTING_BLOCK:
        if (permissions::feature_params::kKeyboardLockPromptUIStyle.Get()) {
          return ExclusiveAccessPermissionPromptView::kNeverAllowId;
        } else {
          return PermissionPromptBubbleBaseView::kBlockButtonElementId;
        }
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

  auto ShowTabModalUI() {
    return Do([this]() {
      scoped_tab_modal_ui_ = browser()->GetActiveTabInterface()->ShowModalUI();
    });
  }

  auto HideTabModalUI() {
    return Do([this]() { scoped_tab_modal_ui_.reset(); });
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ExclusiveAccessPermissionPromptInteractiveTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowKeyboardLock) {
  TestPermissionPrompt(TestContentSettings::kKeyboardLock,
                       CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_P(ExclusiveAccessPermissionPromptInteractiveTest,
                       BlockKeyboardLock) {
  TestPermissionPrompt(TestContentSettings::kKeyboardLock,
                       CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_P(ExclusiveAccessPermissionPromptInteractiveTest,
                       TestPromptInteractionWithModalUILock) {
  RunTestSequence(
      ShowTabModalUI(), ShowPrompt(TestContentSettings::kKeyboardLock),
      HideTabModalUI(), ClickOnElement(TestContentSettings::kKeyboardLock),
      PressPromptButton(GetButtonViewId(CONTENT_SETTING_ALLOW)), Do([&]() {
        auto* manager = permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
        ASSERT_FALSE(manager->has_pending_requests());
      }));
}
