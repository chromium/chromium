// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
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
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/label.h"
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
    feature_list_.InitWithFeatures({features::kPermissionElement,
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
    return InAnyContext(
        Steps(WaitForShow(button_identifier), FlushEvents(),
              PressButton(button_identifier),
              WaitForHide(EmbeddedPermissionPromptBaseView::kMainViewId)));
  }

  // Checks that the next value in the queue matches the text in the label
  // element. If the queue is empty or the popped value is empty, checks that
  // the label is not present instead. Pops the front of the queue if the queues
  // is not empty.
  auto CheckLabel(ui::ElementIdentifier label_identifier,
                  std::queue<std::u16string>& expected_labels) {
    std::u16string expected(u"");

    if (!expected_labels.empty()) {
      expected = expected_labels.front();
      expected_labels.pop();
    }

    if (expected.empty()) {
      return InAnyContext(Steps(EnsureNotPresent(label_identifier)));
    }

    return InAnyContext(Steps(
        CheckViewProperty(label_identifier, &views::Label::GetText, expected)));
  }

  auto CheckContentSettingsValue(
      const std::vector<ContentSettingsType>& content_settings_types,
      const ContentSetting& expected_value) {
    return Steps(CheckResult(
        [&, this]() {
          return DoContentSettingsHaveValue(content_settings_types,
                                            expected_value);
        },
        true));
  }

  bool DoContentSettingsHaveValue(
      const std::vector<ContentSettingsType>& content_settings_types,
      ContentSetting expected_value) {
    HostContentSettingsMap* hcsm =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    for (const auto& type : content_settings_types) {
      if (expected_value !=
          hcsm->GetContentSetting(GetOrigin(), GetOrigin(), type)) {
        return false;
      }
    }

    return true;
  }

  void SetContentSetting(ContentSettingsType type, ContentSetting setting) {
    HostContentSettingsMap* hcsm =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    hcsm->SetContentSettingDefaultScope(GetOrigin(), GetOrigin(), type,
                                        setting);
  }

  // Tests
  void TestAskBlockAllowFlow(
      const std::string& element_id,
      const std::vector<ContentSettingsType>& content_settings_types,
      // Deliberately passing through value to make a locally modifiable copy.
      std::queue<std::u16string> expected_labels1 =
          std::queue<std::u16string>(),
      std::queue<std::u16string> expected_labels2 =
          std::queue<std::u16string>()) {
    RunTestSequence(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),

        // Initially the Ask view is displayed.
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                   expected_labels1),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId2,
                   expected_labels2),

        // After allowing, the content setting is updated accordingly.
        PushPEPCPromptButton(EmbeddedPermissionPromptAskView::kAllowId),
        CheckContentSettingsValue(content_settings_types,
                                  CONTENT_SETTING_ALLOW),

        // The PreviouslyGranted view is displayed since the permission is
        // granted.
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                   expected_labels1),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId2,
                   expected_labels2),

        // Click on "Stop Allowing" and observe the content setting change.
        PushPEPCPromptButton(
            EmbeddedPermissionPromptPreviouslyGrantedView::kStopAllowingId),
        CheckContentSettingsValue(content_settings_types,
                                  CONTENT_SETTING_BLOCK),

        // TODO(crbug.com/5020816): Also test with `kOneTimePermission` disabled
        // when the kAllowId button is present instead.
        // The PreviouslyBlocked view is displayed since the permission is
        // blocked.
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                   expected_labels1),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId2,
                   expected_labels2),

        // Click on "Allow this time" and observe the content setting change.
        PushPEPCPromptButton(
            EmbeddedPermissionPromptPreviouslyDeniedView::kAllowThisTimeId),
        CheckContentSettingsValue(content_settings_types,
                                  CONTENT_SETTING_ALLOW),

        // After the last tab is closed, since the last grant was one-time,
        // ensure the content setting is reset.
        Do([this]() {
          browser()->tab_strip_model()->GetActiveWebContents()->Close();
        }),
        CheckContentSettingsValue(content_settings_types, CONTENT_SETTING_ASK));
  }

  void TestPartialPermissionsLabel(ContentSetting camera_setting,
                                   ContentSetting mic_setting,
                                   const std::u16string expected_label1) {
    RunTestSequence(
        // Set the initial settings values.
        Do([&, this]() {
          SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                            camera_setting);
          SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC, mic_setting);
        }),

        // Trigger a camera+mic prompt and check that the label has the expected
        // text.
        ClickOnPEPCElement("camera-microphone"),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        InAnyContext(
            CheckViewProperty(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                              &views::Label::GetText, expected_label1)),

        // Dismiss the prompt.
        FlushEvents(), Do([this]() {
          auto* manager =
              permissions::PermissionRequestManager::FromWebContents(
                  browser()->tab_strip_model()->GetActiveWebContents());
          manager->Dismiss();
          manager->FinalizeCurrentRequests();
        }));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  StateChange ready_element_visible_;
};

// Failing on Windows, though manual testing of the same flow does not reproduce
// the issue. TODO(andypaicu, crbug.com/1462930): Investigate and fix failure.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BasicFlowMicrophone DISABLED_BasicFlowMicrophone
#define MAYBE_BasicFlowCamera DISABLED_BasicFlowCamera
#define MAYBE_BasicFlowCameraMicrophone DISABLED_BasicFlowCameraMicrophone
#define MAYBE_TestPartialPermissionsLabels DISABLED_TestPartialPermissionsLabels
#else
#define MAYBE_BasicFlowMicrophone BasicFlowMicrophone
#define MAYBE_BasicFlowCamera BasicFlowCamera
#define MAYBE_BasicFlowCameraMicrophone BasicFlowCameraMicrophone
#define MAYBE_TestPartialPermissionsLabels TestPartialPermissionsLabels
#endif
IN_PROC_BROWSER_TEST_F(EmbeddedPermissionPromptInteractiveTest,
                       MAYBE_BasicFlowMicrophone) {
  TestAskBlockAllowFlow(
      "microphone", {ContentSettingsType::MEDIASTREAM_MIC},
      std::queue<std::u16string>(
          {u"Use your microphone",
           u"You have allowed microphone on a.test:" +
               base::UTF8ToUTF16(GetOrigin().port()),
           u"You previously didn't allow microphone on a.test:" +
               base::UTF8ToUTF16(GetOrigin().port())}));
}

IN_PROC_BROWSER_TEST_F(EmbeddedPermissionPromptInteractiveTest,
                       MAYBE_BasicFlowCamera) {
  TestAskBlockAllowFlow("camera", {ContentSettingsType::MEDIASTREAM_CAMERA},
                        std::queue<std::u16string>(
                            {u"Use your camera",
                             u"You have allowed camera on a.test:" +
                                 base::UTF8ToUTF16(GetOrigin().port()),
                             u"You previously didn't allow camera on a.test:" +
                                 base::UTF8ToUTF16(GetOrigin().port())}));
}

IN_PROC_BROWSER_TEST_F(EmbeddedPermissionPromptInteractiveTest,
                       MAYBE_BasicFlowCameraMicrophone) {
  TestAskBlockAllowFlow(
      "camera-microphone",
      {ContentSettingsType::MEDIASTREAM_CAMERA,
       ContentSettingsType::MEDIASTREAM_MIC},
      std::queue<std::u16string>(
          {u"Use your camera",
           u"You have allowed camera and microphone on a.test:" +
               base::UTF8ToUTF16(GetOrigin().port()),
           u"You previously didn't allow camera and microphone on a.test:" +
               base::UTF8ToUTF16(GetOrigin().port())}),
      std::queue<std::u16string>({u"Use your microphone"}));
}

IN_PROC_BROWSER_TEST_F(EmbeddedPermissionPromptInteractiveTest,
                       MAYBE_TestPartialPermissionsLabels) {
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()));

  TestPartialPermissionsLabel(CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
                              u"Use your microphone");
  TestPartialPermissionsLabel(CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW,
                              u"Use your camera");

  TestPartialPermissionsLabel(
      CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK,
      u"You previously didn't allow camera and microphone on a.test:" +
          base::UTF8ToUTF16(GetOrigin().port()));
  TestPartialPermissionsLabel(
      CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK,
      u"You previously didn't allow camera and microphone on a.test:" +
          base::UTF8ToUTF16(GetOrigin().port()));

  TestPartialPermissionsLabel(CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
                              u"You previously didn't allow camera on a.test:" +
                                  base::UTF8ToUTF16(GetOrigin().port()));
  TestPartialPermissionsLabel(
      CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
      u"You previously didn't allow microphone on a.test:" +
          base::UTF8ToUTF16(GetOrigin().port()));
}
