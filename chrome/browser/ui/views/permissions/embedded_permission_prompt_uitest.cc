// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"

class EmbeddedPermissionPromptUiTest : public DialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    set_baseline("5591772");
  }

  void ShowUi(const std::string& name) override {
    // The test name matches the element id to click.
    std::string element_id;
    base::ReplaceChars(name, "_", "-", &element_id);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_https_test_server().GetURL(
                       "example.com", "/permissions/permission_element.html")));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    permissions::PermissionRequestObserver observer(web_contents);

    ASSERT_TRUE(content::ExecJs(
        web_contents, content::JsReplace("clickById($1)", element_id)));

    observer.Wait();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class DefaultParamEmbeddedPermissionPromptUiTest
    : public EmbeddedPermissionPromptUiTest {
 public:
  DefaultParamEmbeddedPermissionPromptUiTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{permissions::features::kOneTimePermission, {}},
         {blink::features::kPermissionElement, {}},
         {blink::features::kBypassPepcSecurityForTesting, {}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(DefaultParamEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41131));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(DefaultParamEmbeddedPermissionPromptUiTest,
                       InvokeUi_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41132));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(DefaultParamEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41133));
  ShowAndVerifyUi();
}

class WindowMiddleEmbeddedPermissionPromptUiTest
    : public EmbeddedPermissionPromptUiTest {
 public:
  WindowMiddleEmbeddedPermissionPromptUiTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {permissions::features::kOneTimePermission, {}},
            {blink::features::kPermissionElement, {}},
            {blink::features::kBypassPepcSecurityForTesting, {}},
            {permissions::features::kPermissionElementPromptPositioning,
             {{"PermissionElementPromptPositioningParam", "window_middle"}}},
        },
        {});
  }
};

IN_PROC_BROWSER_TEST_F(WindowMiddleEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41134));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(WindowMiddleEmbeddedPermissionPromptUiTest,
                       InvokeUi_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41135));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(WindowMiddleEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41136));
  ShowAndVerifyUi();
}

class NearElementEmbeddedPermissionPromptUiTest
    : public EmbeddedPermissionPromptUiTest {
 public:
  NearElementEmbeddedPermissionPromptUiTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {permissions::features::kOneTimePermission, {}},
            {blink::features::kPermissionElement, {}},
            {blink::features::kBypassPepcSecurityForTesting, {}},
            {permissions::features::kPermissionElementPromptPositioning,
             {{"PermissionElementPromptPositioningParam", "near_element"}}},
        },
        {});
  }
};

IN_PROC_BROWSER_TEST_F(NearElementEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41137));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(NearElementEmbeddedPermissionPromptUiTest,
                       InvokeUi_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41138));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(NearElementEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41139));
  ShowAndVerifyUi();
}

class LegacyPromptEmbeddedPermissionPromptUiTest
    : public EmbeddedPermissionPromptUiTest {
 public:
  LegacyPromptEmbeddedPermissionPromptUiTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {permissions::features::kOneTimePermission, {}},
            {blink::features::kPermissionElement, {}},
            {blink::features::kBypassPepcSecurityForTesting, {}},
            {permissions::features::kPermissionElementPromptPositioning,
             {{"PermissionElementPromptPositioningParam", "legacy_prompt"}}},
        },
        {});
  }
};

IN_PROC_BROWSER_TEST_F(LegacyPromptEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41140));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(LegacyPromptEmbeddedPermissionPromptUiTest,
                       // TODO(crbug.com/365077551): Re-enable this test
                       DISABLED_InvokeUi_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41141));
  ShowAndVerifyUi();
}
IN_PROC_BROWSER_TEST_F(LegacyPromptEmbeddedPermissionPromptUiTest,
                       InvokeUi_camera_microphone) {
  /* A static port is needed because it is part of the shown dialog UI which
   * is used for gold pixel texts.*/
  ASSERT_TRUE(embedded_https_test_server().Start(41142));
  ShowAndVerifyUi();
}
