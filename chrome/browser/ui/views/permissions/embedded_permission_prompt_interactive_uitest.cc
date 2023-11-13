// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_denied_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_granted_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/origin.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);
const WebContentsInteractionTestUtil::DeepQuery kReadyElementQuery = {"#ready"};
}  // namespace

class EmbeddedPermissionPromptInteractiveTest : public InteractiveBrowserTest {
 public:
  EmbeddedPermissionPromptInteractiveTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    feature_list_.InitWithFeatures({permissions::features::kPermissionElement,
                                    permissions::features::kOneTimePermission},
                                   {});
    ready_element_visible_.where = kReadyElementQuery;
    ready_element_visible_.type = StateChange::Type::kExists;
    ready_element_visible_.event = kElementReadyEvent;
  }

  ~EmbeddedPermissionPromptInteractiveTest() override = default;
  EmbeddedPermissionPromptInteractiveTest(
      const EmbeddedPermissionPromptInteractiveTest&) = delete;
  void operator=(const EmbeddedPermissionPromptInteractiveTest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  GURL GetOrigin() { return url::Origin::Create(GetURL()).GetURL(); }

  GURL GetURL() {
    return https_server()->GetURL("a.test",
                                  "/permissions/permission_element.html");
  }

  auto ClickOnPEPCElement(const std::string& element_id) {
    return Steps(
        WaitForStateChange(kWebContentsElementId, ready_element_visible_),
        EnsurePresent(kWebContentsElementId, DeepQuery{"#" + element_id}),
        MoveMouseTo(kWebContentsElementId, DeepQuery{"#" + element_id}),
        ClickMouse());
  }

  auto PushPEPCPromptButton(ui::ElementIdentifier button_identifier) {
    return Steps(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId),
                 WaitForShow(button_identifier), FlushEvents(),
                 PressButton(button_identifier),
                 WaitForHide(EmbeddedPermissionPromptBaseView::kMainViewId));
  }

  void ExpectContentSettingsHaveValue(
      const std::vector<ContentSettingsType>& content_settings_types,
      ContentSetting expected_value) {
    HostContentSettingsMap* hcsm =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    for (const auto& type : content_settings_types) {
      EXPECT_EQ(expected_value,
                hcsm->GetContentSetting(GetOrigin(), GetOrigin(), type));
    }
  }

  // Tests
  void TestAskBlockAllowFlow(
      const std::string& element_id,
      const std::vector<ContentSettingsType>& content_settings_types) {
    RunTestSequence(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),
        ClickOnPEPCElement(element_id),
        PushPEPCPromptButton(EmbeddedPermissionPromptAskView::kAllowId));

    ExpectContentSettingsHaveValue(content_settings_types,
                                   CONTENT_SETTING_ALLOW);

    RunTestSequence(
        ClickOnPEPCElement(element_id),
        PushPEPCPromptButton(
            EmbeddedPermissionPromptPreviouslyGrantedView::kStopAllowingId));

    ExpectContentSettingsHaveValue(content_settings_types,
                                   CONTENT_SETTING_BLOCK);

    // TODO(crbug.com/5020816): Also test with `kOneTimePermission` disabled
    // when the kAllowId button is present instead.
    RunTestSequence(
        ClickOnPEPCElement(element_id),
        PushPEPCPromptButton(
            EmbeddedPermissionPromptPreviouslyDeniedView::kAllowThisTimeId));

    ExpectContentSettingsHaveValue(content_settings_types,
                                   CONTENT_SETTING_ALLOW);

    browser()->tab_strip_model()->GetActiveWebContents()->Close();

    ExpectContentSettingsHaveValue(content_settings_types, CONTENT_SETTING_ASK);
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  StateChange ready_element_visible_;
};

IN_PROC_BROWSER_TEST_F(EmbeddedPermissionPromptInteractiveTest,
                       BasicFlowMicrophone) {
  TestAskBlockAllowFlow("microphone", {ContentSettingsType::MEDIASTREAM_MIC});
}

IN_PROC_BROWSER_TEST_F(EmbeddedPermissionPromptInteractiveTest,
                       BasicFlowCamera) {
  TestAskBlockAllowFlow("camera", {ContentSettingsType::MEDIASTREAM_CAMERA});
}

IN_PROC_BROWSER_TEST_F(EmbeddedPermissionPromptInteractiveTest,
                       BasicFlowCameraMicrophone) {
  TestAskBlockAllowFlow("camera-microphone",
                        {ContentSettingsType::MEDIASTREAM_CAMERA,
                         ContentSettingsType::MEDIASTREAM_MIC});
}
