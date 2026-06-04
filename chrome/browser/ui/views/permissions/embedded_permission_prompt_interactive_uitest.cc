// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <queue>
#include <string>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_denied_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_granted_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_show_system_prompt_view.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_system_settings_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "url/origin.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kPEPCVisibleEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kDoneVisibleEvent);

using UkmEntry = ukm::builders::Permissions_EmbeddedPromptAction;

constexpr int kMinWindowHeight = 400;
}  // namespace

class EmbeddedPermissionPromptInteractiveTest
    : public InteractiveBrowserTest,
      public content::TestDevToolsProtocolClient,
      public testing::WithParamInterface<float> {
 public:
  EmbeddedPermissionPromptInteractiveTest() {
    // Force the scale factor to test that the prompt is positioned correctly
    // for all device scale factors.
    display::Display::SetForceDeviceScaleFactor(GetParam());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    feature_list_.InitWithFeatures(
        {blink::features::kGeolocationElement,
         blink::features::kUserMediaElement,
         blink::features::kBypassPepcSecurityForTesting},
        {});
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
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    // Force the window to be large enough.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {10, 10, 800, 800});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTestMixin::SetUpCommandLine(command_line);
    // Disables the disregarding of potentially unintended input events.
    command_line->AppendSwitch(
        views::switches::kDisableInputEventActivationProtectionForTesting);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  GURL GetOrigin() { return url::Origin::Create(GetURL()).GetURL(); }

  GURL GetURL() {
    return https_server()->GetURL("a.test",
                                  "/permissions/permission_element.html");
  }

  auto ClickOnPEPCElement(const std::string& element_id) {
    StateChange pepc_visible;
    pepc_visible.where = DeepQuery{"#" + element_id};
    pepc_visible.type = StateChange::Type::kExists;
    pepc_visible.event = kPEPCVisibleEvent;
    return Steps(
        WaitForStateChange(kWebContentsElementId, pepc_visible),
        ExecuteJsAt(kWebContentsElementId, pepc_visible.where, "click"));
  }

  auto PushPEPCPromptButton(ui::ElementIdentifier button_identifier,
                            bool wait_for_prompt_resolution = true) {
    if (wait_for_prompt_resolution) {
      return InAnyContext(
          WaitForShow(button_identifier), PressButton(button_identifier),
          WaitForHide(EmbeddedPermissionPromptBaseView::kMainViewId));
    } else {
      return InAnyContext(WaitForShow(button_identifier),
                          PressButton(button_identifier));
    }
  }

  auto WaitForChipText(int id_string) {
    DEFINE_LOCAL_POLLING_VIEW_PROPERTY_STATE_IDENTIFIER(
        views::LabelButton, GetText, kChipTextState);
    return InAnyContext(
        WaitForShow(PermissionChipView::kIndicatorChipElementId),
        PollViewProperty(kChipTextState,
                         PermissionChipView::kIndicatorChipElementId),
        WaitForState(kChipTextState, l10n_util::GetStringUTF16(id_string)),
        StopObservingState(kChipTextState));
  }

  // Checks that the next value in the queue matches the text in the label
  // element. If the queue is empty or the popped value is empty, checks that
  // the label is not present instead. Pops the front of the queue if the queues
  // is not empty.
  auto CheckLabel(ui::ElementIdentifier label_identifier,
                  std::vector<std::u16string>& expected_labels,
                  size_t expected_label_index) {
    if (expected_label_index < expected_labels.size()) {
      return InAnyContext(
          CheckViewProperty(label_identifier, &views::Label::GetText,
                            expected_labels[expected_label_index]));
    } else {
      return InAnyContext(EnsureNotPresent(label_identifier));
    }
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

  auto CheckHistogram(const base::HistogramTester& tester,
                      const std::string& view_name,
                      permissions::RequestTypeForUma request_type,
                      int count) {
    return Steps(Do([=, &tester]() {
      tester.ExpectBucketCount(
          view_name, static_cast<base::HistogramBase::Sample32>(request_type),
          count);
    }));
  }

  auto CheckLastSampleAndResetTester(
      std::unique_ptr<base::HistogramTester>& tester,
      const std::string& view_name,
      base::HistogramBase::Sample32 sample) {
    return Steps(Do([=, &tester]() {
      tester->ExpectUniqueSample(view_name, sample, 1);
      tester = std::make_unique<base::HistogramTester>();
    }));
  }

  auto CheckNoUkmEntriesSinceLastCheck() {
    return Steps(Check([this]() {
      size_t entry_count =
          ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName).size();
      ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
      return entry_count == 0U;
    }));
  }

  auto CheckEntrySinceLastCheck(
      permissions::RequestTypeForUma permission,
      permissions::RequestTypeForUma screen_permission,
      permissions::ElementAnchoredBubbleAction action,
      permissions::ElementAnchoredBubbleVariant variant,
      int screen_counter) {
    return Steps(Do([=, this] {
      auto entries = ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
      CHECK_EQ(entries.size(), 1U);

      ukm_recorder_->ExpectEntryMetric(entries[0],
                                       UkmEntry::kPermissionTypeName,
                                       static_cast<int64_t>(permission));
      ukm_recorder_->ExpectEntryMetric(entries[0],
                                       UkmEntry::kScreenPermissionTypeName,
                                       static_cast<int64_t>(screen_permission));
      ukm_recorder_->ExpectEntryMetric(entries[0], UkmEntry::kActionName,
                                       static_cast<int64_t>(action));
      ukm_recorder_->ExpectEntryMetric(entries[0], UkmEntry::kVariantName,
                                       static_cast<int64_t>(variant));
      ukm_recorder_->ExpectEntryMetric(entries[0],
                                       UkmEntry::kPreviousScreensName,
                                       static_cast<int64_t>(screen_counter));
      ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    }));
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
      std::vector<std::u16string> expected_titles =
          std::vector<std::u16string>(),
      std::vector<std::u16string> expected_labels1 =
          std::vector<std::u16string>(),
      std::vector<std::u16string> expected_labels2 =
          std::vector<std::u16string>()) {
    RunTestSequence(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),

        // Initially the Ask view is displayed.
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        CheckLabel(EmbeddedPermissionPromptBaseView::kTitleViewId,
                   expected_titles, /*expected_label_index=*/0),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                   expected_labels1, /*expected_label_index=*/0),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId2,
                   expected_labels2, /*expected_label_index=*/0),

        // After allowing, the content setting is updated accordingly.
        PushPEPCPromptButton(EmbeddedPermissionPromptAskView::kAllowId),
        CheckContentSettingsValue(content_settings_types,
                                  CONTENT_SETTING_ALLOW),

        // Reset the permission to BLOCK to test the block flow.
        Do([&, this]() {
          for (const auto& type : content_settings_types) {
            SetContentSetting(type, CONTENT_SETTING_BLOCK);
          }
        }),

        // The PreviouslyBlocked view is displayed since the permission is
        // blocked.
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        CheckLabel(EmbeddedPermissionPromptBaseView::kTitleViewId,
                   expected_titles, /*expected_label_index=*/2),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                   expected_labels1, /*expected_label_index=*/2),
        CheckLabel(EmbeddedPermissionPromptBaseView::kLabelViewId2,
                   expected_labels2, /*expected_label_index=*/2),

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
        // This has to be immediate, because otherwise closing the browser will
        // detach the profile.
        WithoutDelay(CheckContentSettingsValue(content_settings_types,
                                               CONTENT_SETTING_ASK)));
  }

  void TestAllowThisTimeFlow(
      const std::string& element_id,
      const std::vector<ContentSettingsType>& content_settings_types) {
    RunTestSequence(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),

        // Initially the Ask view is displayed.
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),

        // After allowing this time, the content setting is updated accordingly.
        PushPEPCPromptButton(EmbeddedPermissionPromptAskView::kAllowThisTimeId),
        CheckContentSettingsValue(content_settings_types,
                                  CONTENT_SETTING_ALLOW),

        // After the last tab is closed, since the last grant was one-time,
        // ensure the content setting is reset.
        Do([this]() {
          browser()->tab_strip_model()->GetActiveWebContents()->Close();
        }),
        // This has to be immediate, because otherwise closing the browser will
        // detach the profile.
        WithoutDelay(CheckContentSettingsValue(content_settings_types,
                                               CONTENT_SETTING_ASK)));
  }

  void TestPromptElementText(
      ContentSetting camera_setting,
      ContentSetting mic_setting,
      std::map<ui::ElementIdentifier, const std::u16string>& expected_labels,
      bool check_buttons) {
    auto steps = Steps(
        // Set the initial settings values.
        Do([&, this]() {
          SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                            CONTENT_SETTING_ASK);
          SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                            CONTENT_SETTING_ASK);
        }),
        NavigateWebContents(kWebContentsElementId, GURL("about:blank")),
        NavigateWebContents(kWebContentsElementId, GetURL()),
        Do([&, this]() {
          SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                            camera_setting);
          SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC, mic_setting);
        }),

        // Trigger a camera+mic prompt and check that the label has the expected
        // text.
        ClickOnPEPCElement("camera-microphone"),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)));

    for (const auto& expectation : expected_labels) {
      if (check_buttons) {
        AddStep(steps, Steps(InAnyContext(CheckViewProperty(
                           expectation.first, &views::MdTextButton::GetText,
                           expectation.second))));
      } else {
        AddStep(steps, Steps(InAnyContext(CheckViewProperty(
                           expectation.first, &views::Label::GetText,
                           expectation.second))));
      }
    }

    AddStep(steps,
            Steps(
                // Dismiss the prompt.
                Do([this]() {
                  auto* manager =
                      permissions::PermissionRequestManager::FromWebContents(
                          browser()->tab_strip_model()->GetActiveWebContents());
                  manager->Dismiss(/*prompt_options=*/std::monostate());
                  manager->FinalizeCurrentRequests();
                })));

    RunTestSequence(std::move(steps));
  }

  void TestPromptDismissViaXButton(const std::string& request_type_string,
                                   const std::string& element_id) {
    base::HistogramTester tester;
    RunTestSequence(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        InAnyContext(
            PressButton(views::BubbleFrameView::kCloseButtonElementId)),
        WaitForHide(EmbeddedPermissionPromptBaseView::kMainViewId), Do([&]() {
          tester.ExpectUniqueSample(
              base::StrCat({"Permissions.Prompt.", request_type_string,
                            ".ElementAnchoredBubble.DismissedReason"}),
              permissions::DismissedReason::kDismissedXButton, 1);
        }));
  }

  auto TestPromptDismissViaScrim(const std::string& request_type_string,
                                 const std::string& element_id) {
    base::HistogramTester tester;
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        "EmbeddedPermissionPromptContentScrimWidget");
    RunTestSequence(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        Do([&]() {
          auto* scrim_view =
              static_cast<EmbeddedPermissionPromptContentScrimView*>(
                  waiter.WaitIfNeededAndGet()->GetContentsView());
          scrim_view->OnMousePressed(ui::MouseEvent(
              ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
              ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
        }),
        WaitForHide(EmbeddedPermissionPromptBaseView::kMainViewId), Do([&]() {
          tester.ExpectUniqueSample(
              base::StrCat({"Permissions.Prompt.", request_type_string,
                            ".ElementAnchoredBubble.DismissedReason"}),
              permissions::DismissedReason::kDismissedScrim, 1);
        }));
  }

  auto DismissPromptByClickingCloseButton(
      permissions::RequestType request_type) {
    return Steps();
  }

  void TestPartialPermissionsLabel(ContentSetting camera_setting,
                                   ContentSetting mic_setting,
                                   ui::ElementIdentifier id,
                                   const std::u16string expected_label1) {
    std::map<ui::ElementIdentifier, const std::u16string> expected_labels = {
        {id, expected_label1}};
    TestPromptElementText(camera_setting, mic_setting, expected_labels,
                          /*check_buttons=*/false);
  }

  auto DoPromptAndCheckHistograms(const std::string& element_id,
                                  ui::ElementIdentifier prompt_button,
                                  const base::HistogramTester& tester,
                                  permissions::RequestTypeForUma type,
                                  int accepted_count,
                                  int accepted_once_count) {
    return Steps(
        ClickOnPEPCElement(element_id),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        PushPEPCPromptButton(prompt_button),
        CheckHistogram(
            tester, permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
            type, accepted_count),
        CheckHistogram(
            tester,
            permissions::PermissionUmaUtil::kPermissionsPromptAcceptedOnce,
            type, accepted_once_count),
        CheckHistogram(tester,
                       permissions::PermissionUmaUtil::kPermissionsPromptDenied,
                       type, 0),
        CheckHistogram(
            tester, permissions::PermissionUmaUtil::kPermissionsPromptDismissed,
            type, 0));
  }

  auto ShowTabModalUI() {
    return Do([this]() {
      scoped_tab_modal_ui_ = browser()->GetActiveTabInterface()->ShowModalUI();
    });
  }

  auto HideTabModalUI() {
    return Do([this]() { scoped_tab_modal_ui_.reset(); });
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  // |ukm_recorder_| needs to be reset after every check so that further check
  // functions will only check the new data.
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;
};

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       BasicFlowMicrophone) {
  TestAskBlockAllowFlow(
      "microphone", {ContentSettingsType::MEDIASTREAM_MIC},
      std::vector<std::u16string>(
          {u"a.test:" + base::UTF8ToUTF16(GetOrigin().GetPort()) + u" wants to",
           u"You have allowed microphone for this site",
           u"You previously didn't allow microphone for this site"}),
      std::vector<std::u16string>({u"Use your microphones"}));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       BasicFlowCamera) {
  TestAskBlockAllowFlow(
      "camera", {ContentSettingsType::MEDIASTREAM_CAMERA},
      std::vector<std::u16string>(
          {u"a.test:" + base::UTF8ToUTF16(GetOrigin().GetPort()) + u" wants to",
           u"You have allowed camera for this site",
           u"You previously didn't allow camera for this site"}),
      std::vector<std::u16string>({u"Use your cameras"}));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       BasicFlowCameraMicrophone) {
  TestAskBlockAllowFlow(
      "camera-microphone",
      {ContentSettingsType::MEDIASTREAM_CAMERA,
       ContentSettingsType::MEDIASTREAM_MIC},
      std::vector<std::u16string>(
          {u"a.test:" + base::UTF8ToUTF16(GetOrigin().GetPort()) + u" wants to",
           u"You have allowed camera and microphone for this site",
           u"You previously didn't allow camera and microphone for this site"}),
      std::vector<std::u16string>({u"Use your cameras"}),
      std::vector<std::u16string>({u"Use your microphones"}));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestAllowThisTimeFlowMicrophone) {
  TestAllowThisTimeFlow("microphone", {ContentSettingsType::MEDIASTREAM_MIC});
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestAllowThisTimeFlowCamera) {
  TestAllowThisTimeFlow("camera", {ContentSettingsType::MEDIASTREAM_CAMERA});
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestAllowThisTimeFlowCameraMicrophone) {
  TestAllowThisTimeFlow("camera-microphone",
                        {ContentSettingsType::MEDIASTREAM_CAMERA,
                         ContentSettingsType::MEDIASTREAM_MIC});
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestPartialPermissionsLabels) {
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()));

  TestPartialPermissionsLabel(CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
                              EmbeddedPermissionPromptBaseView::kLabelViewId1,
                              u"Use your microphones");
  TestPartialPermissionsLabel(CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW,
                              EmbeddedPermissionPromptBaseView::kLabelViewId1,
                              u"Use your cameras");

  TestPartialPermissionsLabel(
      CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK,
      EmbeddedPermissionPromptBaseView::kTitleViewId,
      u"You previously didn't allow camera and microphone for this site");
  TestPartialPermissionsLabel(
      CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK,
      EmbeddedPermissionPromptBaseView::kTitleViewId,
      u"You previously didn't allow camera and microphone for this site");

  TestPartialPermissionsLabel(
      CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
      EmbeddedPermissionPromptBaseView::kTitleViewId,
      u"You previously didn't allow camera for this site");
  TestPartialPermissionsLabel(
      CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
      EmbeddedPermissionPromptBaseView::kTitleViewId,
      u"You previously didn't allow microphone for this site");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestButtonsLabel) {
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()));

  std::map<ui::ElementIdentifier, const std::u16string> expected_ask_labels = {
      {EmbeddedPermissionPromptAskView::kAllowId,
       u"Allow while visiting the site"},
      {EmbeddedPermissionPromptAskView::kAllowThisTimeId, u"Allow this time"}};

  TestPromptElementText(CONTENT_SETTING_ASK, CONTENT_SETTING_ASK,
                        expected_ask_labels, /*check_buttons=*/true);
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestOsPromptButtonsLabel) {
  base::AutoReset<bool> mock_system_settings =
      system_permission_settings::MockShowSystemSettingsForTesting();

  std::u16string open_settings_label;

#if BUILDFLAG(IS_MAC)
  open_settings_label = u"MacOS settings";
#elif BUILDFLAG(IS_WIN)
  open_settings_label = u"Windows settings";
#elif BUILDFLAG(IS_CHROMEOS)
  open_settings_label = u"ChromeOS settings";
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera"),
      InAnyContext(
          WaitForShow(EmbeddedPermissionPromptSystemSettingsView::kMainViewId)),
      InAnyContext(CheckViewProperty(
          EmbeddedPermissionPromptSystemSettingsView::kOpenSettingsId,
          &views::MdTextButton::GetText, open_settings_label)));
#endif
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestPepcHistograms) {
  base::HistogramTester tester;
  std::unique_ptr<base::HistogramTester> variant_tester =
      std::make_unique<base::HistogramTester>();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),

      // Initially the "ask" view is displayed.
      DoPromptAndCheckHistograms(
          "camera", EmbeddedPermissionPromptAskView::kAllowId, tester,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA,
          /*accepted_count=*/1, /*accepted_once_count=*/0),
      WaitForChipText(IDS_CAMERA_IN_USE),

      CheckLastSampleAndResetTester(
          variant_tester,
          "Permissions.Prompt.VideoCapture.ElementAnchoredBubble.Variant",
          static_cast<base::HistogramBase::Sample32>(
              permissions::ElementAnchoredBubbleVariant::kAsk)),

      Do([&, this]() {
        SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                          CONTENT_SETTING_DEFAULT);
      }),
      // Other permissions are not affected, check that the microphone
      // permission has no histograms.
      CheckHistogram(tester,
                     permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
                     permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC,
                     /*count=*/0),
      CheckHistogram(
          tester,
          permissions::PermissionUmaUtil::kPermissionsPromptAcceptedOnce,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC,
          /*count=*/0),

      // Trigger and check a microphone "ask" prompt with allow-once.
      DoPromptAndCheckHistograms(
          "microphone", EmbeddedPermissionPromptAskView::kAllowThisTimeId,
          tester, permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC,
          /*accepted_count=*/0,
          /*accepted_once_count=*/1),
      WaitForChipText(IDS_MICROPHONE_IN_USE),

      CheckLastSampleAndResetTester(
          variant_tester,
          "Permissions.Prompt.AudioCapture.ElementAnchoredBubble.Variant",
          static_cast<base::HistogramBase::Sample32>(
              permissions::ElementAnchoredBubbleVariant::kAsk)),

      Do([&, this]() {
        SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                          CONTENT_SETTING_BLOCK);
        SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                          CONTENT_SETTING_BLOCK);
      }),
      // Showing a combined prompt at this point will result in a "previously
      // blocked" screen which won't record new histograms.
      DoPromptAndCheckHistograms(
          "camera-microphone",
          EmbeddedPermissionPromptPreviouslyDeniedView::kAllowThisTimeId,
          tester,
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          /*accepted_count=*/0,
          /*accepted_once_count=*/0),

      // Wait for gUM request to complete before resetting content settings.
      WaitForChipText(IDS_MICROPHONE_CAMERA_IN_USE),
      CheckLastSampleAndResetTester(
          variant_tester,
          "Permissions.Prompt.AudioAndVideoCapture.ElementAnchoredBubble."
          "Variant",
          static_cast<base::HistogramBase::Sample32>(
              permissions::ElementAnchoredBubbleVariant::kPreviouslyDenied)),

      CheckHistogram(
          tester,
          permissions::PermissionUmaUtil::kPermissionsPromptAcceptedOnce,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA,
          /*count=*/0),
      CheckHistogram(
          tester,
          permissions::PermissionUmaUtil::kPermissionsPromptAcceptedOnce,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC,
          /*count=*/1),

      // Reset permissions and show the combined prompt, now in "ask" mode.
      // First check the allow action, then the allow-once action.
      Do([&, this]() {
        SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                          CONTENT_SETTING_DEFAULT);
        SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                          CONTENT_SETTING_DEFAULT);
      }),
      // Reload the page to stop any active getUserMedia request.
      NavigateWebContents(kWebContentsElementId, GURL("about:blank")),
      NavigateWebContents(kWebContentsElementId, GetURL()),

      DoPromptAndCheckHistograms(
          "camera-microphone", EmbeddedPermissionPromptAskView::kAllowId,
          tester,
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          /*accepted_count=*/1,
          /*accepted_once_count=*/0),

      CheckLastSampleAndResetTester(
          variant_tester,
          "Permissions.Prompt.AudioAndVideoCapture.ElementAnchoredBubble."
          "Variant",
          static_cast<base::HistogramBase::Sample32>(
              permissions::ElementAnchoredBubbleVariant::kAsk)),

      WaitForChipText(IDS_MICROPHONE_CAMERA_IN_USE), Do([&, this]() {
        SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                          CONTENT_SETTING_DEFAULT);
        SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                          CONTENT_SETTING_DEFAULT);
      }),
      // Reload the page to stop any active getUserMedia request.
      NavigateWebContents(kWebContentsElementId, GURL("about:blank")),
      NavigateWebContents(kWebContentsElementId, GetURL()),

      DoPromptAndCheckHistograms(
          "camera-microphone",
          EmbeddedPermissionPromptAskView::kAllowThisTimeId, tester,
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          /*accepted_count=*/1,
          /*accepted_once_count=*/1),

      CheckLastSampleAndResetTester(
          variant_tester,
          "Permissions.Prompt.AudioAndVideoCapture.ElementAnchoredBubble."
          "Variant",
          static_cast<base::HistogramBase::Sample32>(
              permissions::ElementAnchoredBubbleVariant::kAsk)),

      // Check that all other histograms are unmodified.
      CheckHistogram(
          tester, permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA,
          /*count=*/1),
      CheckHistogram(
          tester,
          permissions::PermissionUmaUtil::kPermissionsPromptAcceptedOnce,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA,
          /*count=*/0),
      CheckHistogram(tester,
                     permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
                     permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC,
                     /*count=*/0),
      CheckHistogram(
          tester,
          permissions::PermissionUmaUtil::kPermissionsPromptAcceptedOnce,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC,
          /*count=*/1));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       FocusableViaTabKey) {
  StateChange pepc_visible;
  pepc_visible.where = DeepQuery{"#geolocation"};
  pepc_visible.type = StateChange::Type::kExists;
  pepc_visible.event = kPEPCVisibleEvent;

  StateChange done_visible;
  done_visible.where = DeepQuery{"#done"};
  done_visible.type = StateChange::Type::kExists;
  done_visible.event = kDoneVisibleEvent;

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // Setup an event listener for focus changes. When all expected element
      // ids are matched. A 'done' element is appended to the body.
      ExecuteJs(kWebContentsElementId,
                R"JS(
        () => {
          var expected_focused_ids = [
            "geolocation",
            "microphone",
            "camera",
            "camera-microphone"
          ];

          document.addEventListener('focus', (event) => {
            if (event.target.id === expected_focused_ids[0]) {
              expected_focused_ids.shift();
              if (expected_focused_ids.length == 0) {
                const newElement = document.createElement('div');
                newElement.id = 'done';
                document.body.appendChild(newElement);
              }
            }
          }, true);
        })JS"),
      WaitForStateChange(kWebContentsElementId, pepc_visible), Do([this]() {
        // The exact number of "tab" presses needed to pass through all elements
        // differs by platform. Here we do it 10 times to be sure.
        for (int i = 0; i < 10; i++) {
          ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
              browser(), ui::VKEY_TAB, false, false, false, false));
        }
      }),
      // Make sure all elements have been focused as expected.
      WaitForStateChange(kWebContentsElementId, done_visible));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest, TestPepcUkm) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      "EmbeddedPermissionPromptContentScrimWidget");
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera-microphone"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
      CheckNoUkmEntriesSinceLastCheck(),
      PushPEPCPromptButton(EmbeddedPermissionPromptAskView::kAllowId),
      CheckEntrySinceLastCheck(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          permissions::ElementAnchoredBubbleAction::kGranted,
          permissions::ElementAnchoredBubbleVariant::kAsk, 0),

      Do([&, this]() {
        SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                          CONTENT_SETTING_BLOCK);
      }),

      // Mic is granted, camera is blocked. Triggering the double permission
      // prompt will show the screen that is only for camera, while the prompt
      // is for both.
      ClickOnPEPCElement("camera-microphone"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
      PushPEPCPromptButton(
          EmbeddedPermissionPromptPreviouslyDeniedView::kAllowThisTimeId),
      CheckEntrySinceLastCheck(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          permissions::RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA,
          permissions::ElementAnchoredBubbleAction::kGrantedOnce,
          permissions::ElementAnchoredBubbleVariant::kPreviouslyDenied, 0),

      Do([&, this]() {
        SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                          CONTENT_SETTING_BLOCK);
        SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                          CONTENT_SETTING_BLOCK);
      }),
      // Both permissions are blocked. Dismiss the prompt via clicking on the
      // scrim.
      ClickOnPEPCElement("camera-microphone"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
      Do([&]() {
        auto* scrim_view =
            static_cast<EmbeddedPermissionPromptContentScrimView*>(
                waiter.WaitIfNeededAndGet()->GetContentsView());
        scrim_view->OnMousePressed(ui::MouseEvent(
            ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
      }),
      CheckEntrySinceLastCheck(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE,
          permissions::ElementAnchoredBubbleAction::kDismissedScrim,
          permissions::ElementAnchoredBubbleVariant::kPreviouslyDenied, 0));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedXButtonUmaCamera) {
  TestPromptDismissViaXButton(
      permissions::PermissionUmaUtil::GetRequestTypeString(
          permissions::RequestType::kCameraStream),
      "camera");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedXButtonUmaMicrophone) {
  TestPromptDismissViaXButton(
      permissions::PermissionUmaUtil::GetRequestTypeString(
          permissions::RequestType::kMicStream),
      "microphone");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedXButtonUmaGeolocation) {
  TestPromptDismissViaXButton(
      permissions::PermissionUmaUtil::GetRequestTypeString(
          permissions::RequestType::kGeolocation),
      "geolocation");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedXButtonUmaCameraMicrophone) {
  TestPromptDismissViaXButton("AudioAndVideoCapture", "camera-microphone");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedScrimUmaCamera) {
  TestPromptDismissViaScrim(
      permissions::PermissionUmaUtil::GetRequestTypeString(
          permissions::RequestType::kCameraStream),
      "camera");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedScrimUmaMicrophone) {
  TestPromptDismissViaScrim(
      permissions::PermissionUmaUtil::GetRequestTypeString(
          permissions::RequestType::kMicStream),
      "microphone");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedScrimUmaGeolocation) {
  TestPromptDismissViaScrim(
      permissions::PermissionUmaUtil::GetRequestTypeString(
          permissions::RequestType::kGeolocation),
      "geolocation");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDismissedScrimUmaCameraMicrophone) {
  TestPromptDismissViaScrim("AudioAndVideoCapture", "camera-microphone");
}
IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestOsSystemPromptTransition) {
  base::AutoReset<bool> mock_system_prompt =
      system_permission_settings::MockSystemPromptForTesting();

  views::NamedWidgetShownWaiter original_waiter(
      views::test::AnyWidgetTestPasskey{},
      "EmbeddedPermissionPromptContentScrimWidget");
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      "EmbeddedPermissionPromptContentScrimWidget");

  EmbeddedPermissionPromptContentScrimView* original_scrim_view = nullptr;
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
      Do([&]() {
        // Save the reference to the scrim.
        original_scrim_view =
            static_cast<EmbeddedPermissionPromptContentScrimView*>(
                original_waiter.WaitIfNeededAndGet()->GetContentsView());
      }),
      PushPEPCPromptButton(EmbeddedPermissionPromptAskView::kAllowId),
      InAnyContext(WaitForShow(
          EmbeddedPermissionPromptShowSystemPromptView::kMainViewId)),
      Do([&]() {
        auto* scrim_view =
            static_cast<EmbeddedPermissionPromptContentScrimView*>(
                waiter.WaitIfNeededAndGet()->GetContentsView());
        // Verify that the scrim view is the same one that was opened at the
        // beginning, and wasn't closed and reopened during the transition.
        EXPECT_EQ(scrim_view, original_scrim_view);
        scrim_view->OnMousePressed(ui::MouseEvent(
            ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
      }));
}

// Linux wayland does not support window activation.
#if (BUILDFLAG(IS_LINUX) && BUILDFLAG(SUPPORTS_OZONE_WAYLAND))
#define MAYBE_TestOsSystemAutoResolves DISABLED_TestOsSystemAutoResolves
#else
#define MAYBE_TestOsSystemAutoResolves TestOsSystemAutoResolves
#endif
IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       MAYBE_TestOsSystemAutoResolves) {
  std::unique_ptr<system_permission_settings::ScopedSettingsForTesting>
      scoped_system_permission_camera = std::make_unique<
          system_permission_settings::ScopedSettingsForTesting>(
          ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/true);
  std::unique_ptr<system_permission_settings::ScopedSettingsForTesting>
      scoped_system_permission_mic = std::make_unique<
          system_permission_settings::ScopedSettingsForTesting>(
          ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/true);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera-microphone"),
      InAnyContext(
          WaitForShow(EmbeddedPermissionPromptSystemSettingsView::kMainViewId)),
      Do([&]() {
        scoped_system_permission_camera.reset();
        scoped_system_permission_mic.reset();
        scoped_system_permission_camera = std::make_unique<
            system_permission_settings::ScopedSettingsForTesting>(
            ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/false);
        scoped_system_permission_mic = std::make_unique<
            system_permission_settings::ScopedSettingsForTesting>(
            ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/false);

        // Simulate another window becoming active, and then the current window
        // again.
        Browser* focused_window = CreateBrowser(browser()->profile());
        ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(focused_window));
        ASSERT_FALSE(browser()->window()->IsActive());

        ui_test_utils::BrowserActivationWaiter waiter(browser());
        browser()->window()->Activate();
        waiter.WaitForActivation();
      }),

      // Now that both system permissions changed to allowed, the PEPC prompt
      // advances to the next screen.
      InAnyContext(WaitForShow(EmbeddedPermissionPromptAskView::kAllowId)));
}

// This test relies on the presence of the "Go to [OS] setting" button which is
// not implemented for the linux version of the prompt.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_TestOsSystemAutoResolvesOnButton \
  DISABLED_TestOsSystemAutoResolvesOnButton
#else
#define MAYBE_TestOsSystemAutoResolvesOnButton TestOsSystemAutoResolvesOnButton
#endif
IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       MAYBE_TestOsSystemAutoResolvesOnButton) {
  std::unique_ptr<system_permission_settings::ScopedSettingsForTesting>
      scoped_system_permission_camera = std::make_unique<
          system_permission_settings::ScopedSettingsForTesting>(
          ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/true);
  std::unique_ptr<system_permission_settings::ScopedSettingsForTesting>
      scoped_system_permission_mic = std::make_unique<
          system_permission_settings::ScopedSettingsForTesting>(
          ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/true);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera-microphone"),
      InAnyContext(
          WaitForShow(EmbeddedPermissionPromptSystemSettingsView::kMainViewId)),
      Do([&]() {
        scoped_system_permission_camera.reset();
        scoped_system_permission_mic.reset();
        scoped_system_permission_camera = std::make_unique<
            system_permission_settings::ScopedSettingsForTesting>(
            ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/false);
        scoped_system_permission_mic = std::make_unique<
            system_permission_settings::ScopedSettingsForTesting>(
            ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/false);
      }),

      PushPEPCPromptButton(
          EmbeddedPermissionPromptSystemSettingsView::kOpenSettingsId,
          /*wait_for_prompt_resolution=*/false),

      // Now that both system permissions changed to allowed, clicking the "open
      // settings" button means the prompt progresses to the next screen.
      InAnyContext(WaitForShow(EmbeddedPermissionPromptAskView::kAllowId)));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       CrossOriginZoomAffectsValidation) {
  StateChange done_visible;
  done_visible.where = DeepQuery{"#done"};
  done_visible.type = StateChange::Type::kExists;
  done_visible.event = kDoneVisibleEvent;

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(
          kWebContentsElementId,
          https_server()->GetURL(
              "b.test", "/permissions/permission_element_embedder.html")),
      ExecuteJs(kWebContentsElementId,
                content::JsReplace("() => { insertIframe($1, $2); }", GetURL(),
                                   "zoom5")),
      WaitForStateChange(kWebContentsElementId, done_visible), Do([&]() {
        // Need to attach the devtools client to the cross-site child frame to
        // be able to notice the font size issue.
        AttachToFrameTreeHost(ChildFrameAt(browser()
                                               ->tab_strip_model()
                                               ->GetActiveWebContents()
                                               ->GetPrimaryMainFrame(),
                                           0));
        SendCommandSync("Audits.enable");

        // Wait until getting the message that the permission element's font
        // is too large.
        WaitForMatchingNotification(
            "Audits.issueAdded",
            base::BindRepeating([](const base::DictValue& params) {
              const std::string* code =
                  params.FindStringByDottedPath("issue.code");
              if (!code) {
                return false;
              }
              const std::string* issue_type = params.FindStringByDottedPath(
                  "issue.details.permissionElementIssueDetails.issueType");
              if (!issue_type) {
                return false;
              }
              return *code == "PermissionElementIssue" &&
                     *issue_type == "FontSizeTooLarge";
            }));

        DetachProtocolClient();
      }));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       BrowserZoomDoesNotAffectValidation) {
  zoom::ZoomController* zoom_controller = zoom::ZoomController::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  zoom_controller->SetZoomLevel(10);
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera-microphone"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)));
}

// Checks that the prompt is not shown if the tab is displaying another modal
// UI and that it is shown when the other modal UI is closed.
IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestSamePromptInteractionsWithModalUILock) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()), ShowTabModalUI(),
      ClickOnPEPCElement("camera"), HideTabModalUI(),
      ClickOnPEPCElement("camera"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
      InAnyContext(
          CheckViewProperty(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                            &views::Label::GetText, u"Use your cameras")),
      Do([&]() {
        auto* manager = permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
        ASSERT_FALSE(manager->has_pending_requests());

        // Need to close the permission prompt before the test shuts down.
        manager->Dismiss(/*prompt_options=*/std::monostate());
        manager->FinalizeCurrentRequests();
      }));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       TestDifferentPromptInteractionsWithModalUILock) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()), ShowTabModalUI(),
      ClickOnPEPCElement("camera"), HideTabModalUI(),
      ClickOnPEPCElement("geolocation"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
      InAnyContext(
          CheckViewProperty(EmbeddedPermissionPromptBaseView::kLabelViewId1,
                            &views::Label::GetText, u"Know your location")),
      Do([&]() {
        auto* manager = permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
        ASSERT_FALSE(manager->has_pending_requests());

        // Need to close the permission prompt before the test shuts down.
        manager->Dismiss(/*prompt_options=*/std::monostate());
        manager->FinalizeCurrentRequests();
      }));
}

class EmbeddedPermissionPromptPositioningInteractiveTest
    : public EmbeddedPermissionPromptInteractiveTest {
 public:
  EmbeddedPermissionPromptPositioningInteractiveTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {
            {blink::features::kGeolocationElement, {}},
            {blink::features::kUserMediaElement, {}},
            {permissions::features::kPermissionElementPromptPositioning,
             {{"PermissionElementPromptPositioningParam", "near_element"}}},
            {blink::features::kBypassPepcSecurityForTesting, {}},
        },
        {});
  }
};

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptPositioningInteractiveTest,
                       TestPermissionElementDialogPositioning) {
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()),
                  // Set the font size to 'small' to ensure all elements have
                  // enough room in a line as this test depends on it.
                  ExecuteJs(kWebContentsElementId, "setFontSizeSmall"));

  // Click on multiple elements in order from left to right, and ensure that
  // dialog moves with each click
  gfx::Point current_origin;
  struct ElementAction {
    std::string element_name;
    ui::ElementIdentifier button_identifier;
  };
  std::vector<ElementAction> element_actions = {
      {"geolocation", EmbeddedPermissionPromptAskView::kAllowId},
      {"microphone", EmbeddedPermissionPromptAskView::kAllowId},
      {"camera", EmbeddedPermissionPromptAskView::kAllowId},
  };

  for (const auto& element_action : element_actions) {
    RunTestSequence(
        ClickOnPEPCElement(element_action.element_name),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),

        InAnyContext(CheckView(EmbeddedPermissionPromptBaseView::kMainViewId,
                               [&current_origin](views::View* view) {
                                 gfx::Point previous_origin = current_origin;
                                 current_origin =
                                     view->GetBoundsInScreen().origin();
                                 return previous_origin < current_origin;
                               })),
        PushPEPCPromptButton(element_action.button_identifier));
  }
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptPositioningInteractiveTest,
                       TestPositionUsingZoom) {
  auto* widget = BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  if (widget->GetWindowBoundsInScreen().height() < kMinWindowHeight) {
    // Skip the test if the actual window size is too small to fit the prompt
    // even after forcing the window size in the test suite.
    return;
  }

  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()),
                  ExecuteJs(kWebContentsElementId, "setFontSizeSmall"));

  double zoom_level = 0;
  int previous_x = 0;

  int loops = 5;
  while (loops--) {
    RunTestSequence(
        ClickOnPEPCElement("microphone"),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),

        InAnyContext(CheckView(
            EmbeddedPermissionPromptBaseView::kMainViewId,
            [&previous_x](views::View* view) {
              gfx::Rect bounds = view->GetBoundsInScreen();
              previous_x = bounds.x();
              return bounds.x();
            },
            testing::Gt(previous_x))),

        Do([this, &zoom_level]() {
          auto* manager =
              permissions::PermissionRequestManager::FromWebContents(
                  browser()->tab_strip_model()->GetActiveWebContents());
          manager->Dismiss(/*prompt_options=*/std::monostate());
          manager->FinalizeCurrentRequests();

          zoom::ZoomController* zoom_controller =
              zoom::ZoomController::FromWebContents(
                  browser()->tab_strip_model()->GetActiveWebContents());
          zoom_level += 0.2;
          zoom_controller->SetZoomLevel(zoom_level);
        }));
  }

  zoom::ZoomController* zoom_controller = zoom::ZoomController::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  zoom_controller->SetZoomLevel(zoom_controller->GetDefaultZoomLevel());
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptPositioningInteractiveTest,
                       TestPositionInsideCrossOriginFrame) {
  auto* widget = BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  if (widget->GetWindowBoundsInScreen().height() < kMinWindowHeight) {
    // Skip the test if the actual window size is too small to fit the prompt
    // even after forcing the window size in the test suite.
    return;
  }

  StateChange done_visible;
  done_visible.where = DeepQuery{"#done"};
  done_visible.type = StateChange::Type::kExists;
  done_visible.event = kDoneVisibleEvent;

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(
          kWebContentsElementId,
          https_server()->GetURL(
              "b.test", "/permissions/permission_element_embedder.html")),
      ExecuteJs(kWebContentsElementId,
                content::JsReplace("() => { insertIframe($1); }", GetURL())),
      WaitForStateChange(kWebContentsElementId, done_visible));

  int loops = 5;
  int previous_y = 0;
  while (loops--) {
    RunTestSequence(
        ExecuteJs(
            kWebContentsElementId,
            content::JsReplace("() => { clickInIframe($1); }", "microphone")),
        InAnyContext(
            WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),
        InAnyContext(CheckView(
            EmbeddedPermissionPromptBaseView::kMainViewId,
            [&previous_y](views::View* view) {
              gfx::Rect bounds = view->GetBoundsInScreen();
              previous_y = bounds.y();
              return bounds.y();
            },
            testing::Gt(previous_y))),
        ExecuteJs(kWebContentsElementId, "expandDiv"), Do([this]() {
          auto* manager =
              permissions::PermissionRequestManager::FromWebContents(
                  browser()->tab_strip_model()->GetActiveWebContents());
          manager->Dismiss(/*prompt_options=*/std::monostate());
          manager->FinalizeCurrentRequests();
        }));
  }
}

// A test suite for running policy-related interactive tests. This test suite
// is parameterized to match its base class, but the parameter is not used in
// the tests.
class EmbeddedPermissionPromptPolicyInteractiveTest
    : public EmbeddedPermissionPromptInteractiveTest {
 public:
  EmbeddedPermissionPromptPolicyInteractiveTest() = default;
  ~EmbeddedPermissionPromptPolicyInteractiveTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    EmbeddedPermissionPromptInteractiveTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);
  }

  void UpdateProviderPolicy(const policy::PolicyMap& policies) {
    policy_provider_.UpdateChromePolicy(policies);
  }

  void TestPolicy(const policy::PolicyMap& policies,
                  const std::string& element_id,
                  const ui::ElementIdentifier& expected_view_id,
                  const std::u16string& expected_title) {
    UpdateProviderPolicy(policies);

    RunTestSequence(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),
        ClickOnPEPCElement(element_id),
        InAnyContext(WaitForShow(expected_view_id)),
        InAnyContext(
            CheckViewProperty(EmbeddedPermissionPromptBaseView::kTitleViewId,
                              &views::Label::GetText, expected_title)),
        PushPEPCPromptButton(EmbeddedPermissionPromptBaseView::kOkButtonId));
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptPolicyInteractiveTest,
                       CameraPolicyBlock) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kVideoCaptureAllowed,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  TestPolicy(policies, "camera",
             EmbeddedPermissionPromptPolicyView::kMainViewId,
             u"Your administrator doesn't allow camera for this site");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptPolicyInteractiveTest,
                       MicrophonePolicyBlock) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kAudioCaptureAllowed,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  TestPolicy(policies, "microphone",
             EmbeddedPermissionPromptPolicyView::kMainViewId,
             u"Your administrator doesn't allow microphone for this site");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptPolicyInteractiveTest,
                       CameraAndMicrophonePolicyBlock) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kVideoCaptureAllowed,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policies.Set(policy::key::kAudioCaptureAllowed,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  TestPolicy(
      policies, "camera-microphone",
      EmbeddedPermissionPromptPolicyView::kMainViewId,
      u"Your administrator doesn't allow camera and microphone for this site");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptPolicyInteractiveTest,
                       GeolocationPolicyBlock) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kDefaultGeolocationSetting,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(CONTENT_SETTING_BLOCK),
               nullptr);
  TestPolicy(policies, "geolocation",
             EmbeddedPermissionPromptPolicyView::kMainViewId,
             u"Your administrator doesn't allow location for this site");
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       ScrimSnapsToWebContentsBounds) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      "EmbeddedPermissionPromptContentScrimWidget");

  RunTestSequence(
      // Setup and trigger the permission prompt.
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),

      Do([&]() {
        views::Widget* scrim_widget = waiter.WaitIfNeededAndGet();
        ASSERT_TRUE(scrim_widget);

        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();

        // Change scrim's bounds to be different from web contents so it
        // is out of sync.
        gfx::Rect wrong_bounds(0, 0, 10, 10);
        scrim_widget->SetBounds(wrong_bounds);
        EXPECT_EQ(scrim_widget->GetWindowBoundsInScreen(), wrong_bounds);

        auto* scrim_view =
            static_cast<EmbeddedPermissionPromptContentScrimView*>(
                scrim_widget->GetContentsView());

        // Simulate bounds event change. Pass empty rect bounds since
        // code does not utilize bounds parameter and uses web contents bounds
        // instead.
        scrim_view->OnWidgetBoundsChanged(scrim_widget, gfx::Rect());

        // The scrim should update its size to match the web contents bounds.
        EXPECT_EQ(scrim_widget->GetWindowBoundsInScreen(),
                  web_contents->GetContainerBounds());
      }),

      // Clean up.
      Do([this]() {
        auto* manager = permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
        manager->Dismiss(/*prompt_options=*/std::monostate());
        manager->FinalizeCurrentRequests();
      }));
}

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       ScrimSnapsToBoundsOnFrameSizeChanged) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      "EmbeddedPermissionPromptContentScrimWidget");

  RunTestSequence(
      // Setup and trigger the permission prompt.
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      ClickOnPEPCElement("camera"),
      InAnyContext(WaitForShow(EmbeddedPermissionPromptBaseView::kMainViewId)),

      Do([&]() {
        views::Widget* scrim_widget = waiter.WaitIfNeededAndGet();
        ASSERT_TRUE(scrim_widget);

        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();

        // Change the scrim's bounds so it is out of sync with the window size.
        gfx::Rect wrong_bounds(0, 0, 10, 10);
        scrim_widget->SetBounds(wrong_bounds);
        EXPECT_EQ(scrim_widget->GetWindowBoundsInScreen(), wrong_bounds);

        auto* scrim_view =
            static_cast<EmbeddedPermissionPromptContentScrimView*>(
                scrim_widget->GetContentsView());
        // Simulate the window changing size by calling `FrameSizeChanged` on a
        // misc iframe. This way, `FrameSizeChanged` does not change the size of
        // the scrim.
        scrim_view->FrameSizeChanged(nullptr, gfx::Size());
        EXPECT_NE(scrim_widget->GetWindowBoundsInScreen(),
                  web_contents->GetContainerBounds());

        // Simulate the window changing size by calling `FrameSizeChanged` on
        // the main frame. This way, `FrameSizeChanged` updates the size of the
        // scrim.
        scrim_view->FrameSizeChanged(web_contents->GetPrimaryMainFrame(),
                                     gfx::Size());

        // The scrim must have instantly snapped back to match the size of the
        // window.
        EXPECT_EQ(scrim_widget->GetWindowBoundsInScreen(),
                  web_contents->GetContainerBounds());
      }),

      // Cleanup.
      Do([this]() {
        auto* manager = permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
        manager->Dismiss(/*prompt_options=*/std::monostate());
        manager->FinalizeCurrentRequests();
      }));
}

namespace {
class DummyOmniboxPopupWebUIContent : public OmniboxPopupWebUIBaseContent {
  METADATA_HEADER(DummyOmniboxPopupWebUIContent, OmniboxPopupWebUIBaseContent)
 public:
  explicit DummyOmniboxPopupWebUIContent(LocationBar* location_bar)
      : OmniboxPopupWebUIBaseContent(nullptr,
                                     location_bar,
                                     nullptr,
                                     /*top_rounded_corners=*/true) {}

  void Clear() override {}
  std::string_view GetMetricPrefix() const override { return "Dummy"; }

 protected:
  void OnContextMenuClosed() override {}
};

BEGIN_METADATA(DummyOmniboxPopupWebUIContent)
END_METADATA

class TestScrimDelegate
    : public EmbeddedPermissionPromptContentScrimView::Delegate {
 public:
  TestScrimDelegate() = default;
  void DismissScrim() override {}
  base::WeakPtr<permissions::PermissionPrompt::Delegate>
  GetPermissionPromptDelegate() const override {
    return nullptr;
  }
  base::WeakPtr<EmbeddedPermissionPromptContentScrimView::Delegate>
  GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<EmbeddedPermissionPromptContentScrimView::Delegate>
      weak_factory_{this};
};
}  // namespace

IN_PROC_BROWSER_TEST_P(EmbeddedPermissionPromptInteractiveTest,
                       ScrimRoundedCornersMatchOmniboxPopup) {
  // Construct a dummy widget and view hierarchy to host the mock Omnibox popup.
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.context = browser()->GetWindow()->GetNativeWindow();
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetBounds(gfx::Rect(0, 0, 800, 600));

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* location_bar = browser_view->toolbar()->location_bar_view();

  auto container = std::make_unique<views::View>();
  auto* omnibox_content = container->AddChildView(
      std::make_unique<DummyOmniboxPopupWebUIContent>(location_bar));

  // Make omnibox the client of the PEPC scrim.
  auto rounded_frame = std::make_unique<RoundedOmniboxResultsFrame>(
      container.release(), location_bar, /*forward_mouse_events=*/false);

  widget->SetContentsView(std::move(rounded_frame));
  widget->Show();

  // Create a new WebContents instead of using the browser's active WebContents.
  std::unique_ptr<content::WebContents> test_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  // Navigate the web contents to ensure its render widget host view is created
  // and GetContentNativeView() is non-null.
  ASSERT_TRUE(
      content::NavigateToURL(test_web_contents.get(), GURL("about:blank")));

  omnibox_content->SetWebContents(test_web_contents.get());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return views::Widget::GetTopLevelWidgetForNativeView(
               test_web_contents->GetContentNativeView()) == widget.get();
  }));

  views::Widget* web_contents_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          test_web_contents->GetContentNativeView());
  EXPECT_NE(web_contents_widget, nullptr);
  EXPECT_EQ(web_contents_widget, widget.get());

  // Get frame and validate it exists.
  auto* rounded_frame_actual = views::AsViewClass<RoundedOmniboxResultsFrame>(
      web_contents_widget->GetClientContentsView());
  EXPECT_NE(rounded_frame_actual, nullptr);

  // Get omnibox popup and confirm it exists and equal to the omnibox
  // popup of interest.
  auto* omnibox_content_actual =
      rounded_frame_actual->GetOmniboxPopupWebUIBaseContent();
  EXPECT_NE(omnibox_content_actual, nullptr);
  EXPECT_EQ(omnibox_content_actual, omnibox_content);

  TestScrimDelegate delegate;
  auto scrim_view = std::make_unique<EmbeddedPermissionPromptContentScrimView>(
      delegate.GetWeakPtr(), test_web_contents.get(),
      /*should_dismiss_on_click=*/true);

  // The scrim's layer rounded corner radius should match the radii of the
  // omnibox popup content wrapper.
  EXPECT_NE(scrim_view->layer(), nullptr);
  EXPECT_FALSE(scrim_view->layer()->fills_bounds_opaquely());
  EXPECT_EQ(scrim_view->layer()->rounded_corner_radii(),
            omnibox_content->GetRoundedCornerRadii());

  omnibox_content->SetWebContents(nullptr);
  views::WidgetDeletionObserver deletion_observer(widget.get());
  widget.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !deletion_observer.IsWidgetAlive(); }));
}

// Setting up to run all tests with two screen scale factors.
INSTANTIATE_TEST_SUITE_P(,
                         EmbeddedPermissionPromptInteractiveTest,
                         testing::Values(1.0, 2.0));
INSTANTIATE_TEST_SUITE_P(,
                         EmbeddedPermissionPromptPolicyInteractiveTest,
                         testing::Values(1.0));
INSTANTIATE_TEST_SUITE_P(,
                         EmbeddedPermissionPromptPositioningInteractiveTest,
                         testing::Values(1.0, 2.0));
