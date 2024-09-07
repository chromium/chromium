// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/sharing/sharing_browsertest.h"
#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/pref_names.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

namespace {
const char kTelUrl[] = "tel:+9876543210";
const char kNonTelUrl[] = "https://google.com";
const char kLinkText[] = "Google";
const char kTextWithPhoneNumber[] = "call 9876543210 now";
const char kTextWithoutPhoneNumber[] = "abcde";

const char kTestPageURL[] = "/sharing/tel.html";

enum class ClickToCallPolicy {
  kNotConfigured,
  kFalse,
  kTrue,
};

}  // namespace

// Browser tests for the Click To Call feature.
class ClickToCallBrowserTest : public SharingBrowserTest {
 public:
  ~ClickToCallBrowserTest() override = default;

  std::string GetTestPageURL() const override {
    return std::string(kTestPageURL);
  }

  void CheckLastSharingMessageSent(
      const std::string& expected_phone_number) const {
    components_sharing_message::SharingMessage sharing_message =
        GetLastSharingMessageSent();
    ASSERT_TRUE(sharing_message.has_click_to_call_message());
    EXPECT_EQ(expected_phone_number,
              sharing_message.click_to_call_message().phone_number());
  }

 protected:
  std::string HistogramName(const char* suffix) {
    return base::StrCat({"Sharing.ClickToCall", suffix});
  }

  base::HistogramTester::CountsMap GetTotalHistogramCounts(
      const base::HistogramTester& histograms) {
    return histograms.GetTotalCountsForPrefix(HistogramName(""));
  }

 private:
  base::test::ScopedFeatureList features_{kClickToCall};
};

// TODO(himanshujaju): Add UI checks.
IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_TelLink_SingleDeviceAvailable) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(1u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kTelUrl), kLinkText, kTextWithoutPhoneNumber);

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);
  CheckLastReceiver(devices[0]);
  CheckLastSharingMessageSent(GURL(kTelUrl).GetContent());
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_NoDevicesAvailable) {
  Init(sync_pb::SharingSpecificFields::UNKNOWN,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(0u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kTelUrl), kLinkText, kTextWithoutPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_UnsafeTelLink) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(1u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu = InitContextMenu(
      GURL("tel:%23*999%23"), kLinkText, kTextWithoutPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_EscapedCharacters) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(1u, devices.size());

  GURL phone_number("tel:%2B44%20123");
  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(phone_number, kLinkText, kTextWithoutPhoneNumber);
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);
  CheckLastReceiver(devices[0]);
  CheckLastSharingMessageSent(phone_number.GetContent());
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_TelLink_MultipleDevicesAvailable) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kTelUrl), kLinkText, kTextWithoutPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  raw_ptr<ui::MenuModel> sub_menu_model = nullptr;
  size_t device_id = 0;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2u, sub_menu_model->GetItemCount());
  EXPECT_EQ(0u, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + static_cast<int>(device_id),
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    CheckLastReceiver(device);
    CheckLastSharingMessageSent(GURL(kTelUrl).GetContent());
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_HighlightedText_MultipleDevicesAvailable) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kNonTelUrl), kLinkText, kTextWithPhoneNumber);
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  raw_ptr<ui::MenuModel> sub_menu_model = nullptr;
  size_t device_id = 0;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2u, sub_menu_model->GetItemCount());
  EXPECT_EQ(0u, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + static_cast<int>(device_id),
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    CheckLastReceiver(device);
    std::optional<std::string> expected_number =
        ExtractPhoneNumberForClickToCall(GetProfile(0), kTextWithPhoneNumber);
    ASSERT_TRUE(expected_number.has_value());
    CheckLastSharingMessageSent(expected_number.value());
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_TelLink_Histograms) {
  base::HistogramTester histograms;
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);

  // Trigger a context menu for a link with 8 digits and 9 characters.
  std::unique_ptr<TestRenderViewContextMenu> menu = InitContextMenu(
      GURL("tel:1234-5678"), kLinkText, kTextWithoutPhoneNumber);

  base::HistogramTester::CountsMap expected_counts = {
      {HistogramName("DevicesToShow"), 1},
      {HistogramName("DevicesToShow.ContextMenu"), 1},
  };

  EXPECT_THAT(GetTotalHistogramCounts(histograms),
              testing::ContainerEq(expected_counts));

  // Send the number to the device in the context menu.
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  expected_counts.insert({
      {HistogramName("SelectedDeviceIndex"), 1},
      {HistogramName("SelectedDeviceIndex.ContextMenu"), 1},
  });

  EXPECT_THAT(GetTotalHistogramCounts(histograms),
              testing::ContainerEq(expected_counts));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_HighlightedText_Histograms) {
  base::HistogramTester histograms;
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);

  // Trigger a context menu for a selection with 8 digits and 9 characters.
  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kNonTelUrl), kLinkText, "1234-5678");

  base::HistogramTester::CountsMap expected_counts = {
      {HistogramName("DevicesToShow"), 1},
      {HistogramName("DevicesToShow.ContextMenu"), 1},
  };

  EXPECT_THAT(GetTotalHistogramCounts(histograms),
              testing::ContainerEq(expected_counts));

  // Send the number to the device in the context menu.
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  expected_counts.insert({
      {HistogramName("SelectedDeviceIndex"), 1},
      {HistogramName("SelectedDeviceIndex.ContextMenu"), 1},
  });

  EXPECT_THAT(GetTotalHistogramCounts(histograms),
              testing::ContainerEq(expected_counts));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_UKM) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(1u, devices.size());

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::RunLoop run_loop;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::Sharing_ClickToCall::kEntryName, run_loop.QuitClosure());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(kNonTelUrl), kLinkText, kTextWithPhoneNumber);

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  // Send number to device
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  // Expect UKM metrics to be logged
  run_loop.Run();
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      ukm_entries = ukm_recorder.GetEntriesByName(
          ukm::builders::Sharing_ClickToCall::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());

  const int64_t* entry_point = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kEntryPointName);
  const int64_t* has_apps = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasAppsName);
  const int64_t* has_devices = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasDevicesName);
  const int64_t* selection = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kSelectionName);

  ASSERT_TRUE(entry_point);
  ASSERT_TRUE(has_apps);
  ASSERT_TRUE(has_devices);
  ASSERT_TRUE(selection);

  EXPECT_EQ(
      static_cast<int64_t>(SharingClickToCallEntryPoint::kRightClickSelection),
      *entry_point);
  EXPECT_EQ(true, *has_devices);
  EXPECT_EQ(static_cast<int64_t>(SharingClickToCallSelection::kDevice),
            *selection);
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, CloseTabWithBubble) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(1u, devices.size());

  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  // Click on the tel link to trigger the bubble view.
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.querySelector('a').click();", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  // Wait until the bubble is visible.
  run_loop.Run();

  // Close the tab while the bubble is opened.
  // Regression test for http://crbug.com/1000934.
  sessions_helper::CloseTab(/*browser_index=*/0, /*tab_index=*/0);
}

// TODO(himanshujaju) - Add chromeos test for same flow.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, LeftClick_ChooseDevice) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  ASSERT_EQ(1u, devices.size());

  base::RunLoop run_loop;
  PageActionIconView* click_to_call_icon =
      GetPageActionIconView(PageActionIconType::kClickToCall);
  ASSERT_FALSE(click_to_call_icon->GetVisible());

  ClickToCallUiController* controller =
      ClickToCallUiController::GetOrCreateFromWebContents(web_contents());
  controller->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  // Click on the tel link to trigger the bubble view.
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.querySelector('a').click();", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  // Wait until the bubble is visible.
  run_loop.Run();

  ASSERT_TRUE(click_to_call_icon->GetVisible());

  SharingDialogView* dialog =
      static_cast<SharingDialogView*>(controller->dialog());
  EXPECT_EQ(SharingDialogType::kDialogWithDevicesMaybeApps,
            dialog->GetDialogType());

  // Choose first device.
  const auto& buttons = dialog->button_list_for_testing()->children();
  ASSERT_GT(buttons.size(), 0u);
  views::test::ButtonTestApi(static_cast<views::Button*>(buttons[0]))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));

  CheckLastReceiver(devices[0]);
  // Defined in tel.html
  CheckLastSharingMessageSent("0123456789");
}
#endif

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, OpenNewTabAndShowBubble) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);

  // Open tab to different origin.
  sessions_helper::OpenTab(
      /*browser_index=*/0,
      embedded_test_server()->GetURL("mock2.http", GetTestPageURL()));

  // Expect dialog to be shown in context of active WebContents.
  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(
      GetBrowser(0)->tab_strip_model()->GetWebContentsAt(1))
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  // Navigate initial tab to a tel link.
  NavigateParams params(GetBrowser(0), GURL(kTelUrl), ui::PAGE_TRANSITION_LINK);
  params.source_contents = web_contents();
  params.initiator_origin = url::Origin::Create(GURL("mock.http"));
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  ui_test_utils::NavigateToURL(&params);

  // Wait until the bubble is visible.
  run_loop.Run();
  views::BubbleDialogDelegate* bubble =
      GetPageActionIconView(PageActionIconType::kClickToCall)->GetBubble();
  ASSERT_NE(nullptr, bubble);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Ensure that the dialog shows the origin in the footnote.
  EXPECT_NE(nullptr, bubble->GetFootnoteViewForTesting());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, NavigateDifferentOrigin) {
  Init(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);

  base::RunLoop run_loop;
  PageActionIconView* click_to_call_icon =
      GetPageActionIconView(PageActionIconType::kClickToCall);
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  // Click on the tel link to trigger the bubble view.
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.querySelector('a').click();", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  // Wait until the bubble is visible.
  run_loop.Run();
  EXPECT_NE(nullptr, click_to_call_icon->GetBubble());

  // Navigate to a different origin.
  sessions_helper::NavigateTab(/*browser_index=*/0, GURL("https://google.com"));

  // Ensure that the bubble is now closed.
  EXPECT_EQ(nullptr, click_to_call_icon->GetBubble());
}

class ClickToCallPolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<ClickToCallPolicy> {
 public:
  ClickToCallPolicyTest() = default;
  ~ClickToCallPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;

    const ClickToCallPolicy policy = GetParam();
    if (policy == ClickToCallPolicy::kFalse ||
        policy == ClickToCallPolicy::kTrue) {
      const bool policy_bool = (policy == ClickToCallPolicy::kTrue);
      policies.Set(policy::key::kClickToCallEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                   base::Value(policy_bool), nullptr);
    }

    provider_.UpdateChromePolicy(policies);
  }

 private:
  base::test::ScopedFeatureList features_{kClickToCall};
};

IN_PROC_BROWSER_TEST_P(ClickToCallPolicyTest, RunTest) {
  const char* kPhoneNumber = "+9876543210";
  const char* kPhoneLink = "tel:+9876543210";
  bool expected_enabled = GetParam() != ClickToCallPolicy::kFalse;
  bool expected_configured = GetParam() != ClickToCallPolicy::kNotConfigured;

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ(expected_enabled, prefs->GetBoolean(prefs::kClickToCallEnabled));
  EXPECT_EQ(expected_configured,
            prefs->IsManagedPreference(prefs::kClickToCallEnabled));

  EXPECT_EQ(expected_enabled, ShouldOfferClickToCallForURL(browser()->profile(),
                                                           GURL(kPhoneLink)));

  std::optional<std::string> extracted =
      ExtractPhoneNumberForClickToCall(browser()->profile(), kPhoneNumber);
  if (expected_enabled)
    EXPECT_EQ(kPhoneNumber, extracted.value());
  else
    EXPECT_FALSE(extracted.has_value());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClickToCallPolicyTest,
                         ::testing::Values(ClickToCallPolicy::kNotConfigured,
                                           ClickToCallPolicy::kFalse,
                                           ClickToCallPolicy::kTrue));
