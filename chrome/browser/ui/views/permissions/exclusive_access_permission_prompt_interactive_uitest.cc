// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

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
    feature_list_.InitAndEnableFeature(features::kKeyboardAndPointerLockPrompt);
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
    RunTestSequence(ShowPrompt(test_content_settings),
                    PressPromptButton(GetButtonViewId(expected_value)),
                    CheckOutcome(test_content_settings, expected_value));
  }

  MultiStep ShowPrompt(TestContentSettings test_content_settings) {
    return Steps(InstrumentTab(kWebContentsElementId),
                 NavigateWebContents(kWebContentsElementId, GetURL()),
                 FocusOnPage(kWebContentsElementId),
                 ExecuteJsAt(kWebContentsElementId,
                             DeepQuery{GetHtmlElementId(test_content_settings)},
                             "click"));
  }

  MultiStep PressPromptButton(ui::ElementIdentifier button_identifier) {
    return InAnyContext(
        Steps(WaitForShow(button_identifier), FlushEvents(),
              PressButton(button_identifier),
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

  StepBuilder FocusOnPage(ui::ElementIdentifier webcontents_id) {
    StepBuilder builder;
    builder.SetElementID(webcontents_id);
    builder.SetDescription("FocusOnPage()");
    builder.SetStartCallback(base::BindLambdaForTesting(
        [](ui::InteractionSequence* seq, ui::TrackedElement* el) {
          auto* const tracked_el = AsInstrumentedWebContents(el);
          if (!tracked_el) {
            LOG(ERROR) << "Element is not an instrumented WebContents.";
            seq->FailForTesting();
            return;
          }
          auto* const contents = tracked_el->web_contents();
          if (!contents || !contents->GetRenderWidgetHostView()) {
            LOG(ERROR)
                << "WebContents not present or no render widget host view.";
            seq->FailForTesting();
            return;
          }
          contents->GetRenderWidgetHostView()->Focus();
        }));
    return builder;
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
        return "#keybard-and-pointer-lock";
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
