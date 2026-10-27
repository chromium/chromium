// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"

#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/sync_device_info/remote_device_info.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/window/dialog_client_view.h"

namespace send_tab_to_self {

namespace {

class SendTabToSelfInteractiveUiTest;

using FormFactor = syncer::DeviceInfo::FormFactor;
using OsType = syncer::DeviceInfo::OsType;
// Test suite for interactive UI tests that are not part of the
// `SendTabToSelfInteractiveUiTest.*` filter in `pixel_tests.filter`.
using SendTabToSelfDeviceSelectionInteractiveUiTest =
    SendTabToSelfInteractiveUiTest;
using syncer::RemoteDeviceInfo;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "8667798";

class SendTabToSelfInteractiveUiTest : public InteractiveBrowserTest {
 public:
  SendTabToSelfInteractiveUiTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&SendTabToSelfInteractiveUiTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>();
        }));
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  auto ShowBubble() {
    return Do([this]() {
      SendTabToSelfBubbleController::GetOrCreateForWebContents(
          browser()->GetTabStripModel()->GetActiveWebContents())
          ->ShowBubble(ShareEntryPoint::kToolbarIcon);
    });
  }

  auto StopToastTimer() {
    return Do([this]() {
      ToastController::From(browser())->GetToastCloseTimerForTesting()->Stop();
    });
  }

  auto SetUpTargetDevicesAndShowBubble(
      std::vector<RemoteDeviceInfo> target_devices) {
    const GURL test_url = embedded_test_server()->GetURL("/empty.html");
    return Steps(
        InstrumentTab(kPrimaryTabId),
        NavigateWebContents(kPrimaryTabId, test_url),
        FocusWebContents(kPrimaryTabId),
        SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                "Should only run in pixel_tests."),
        Do([this, devices = std::move(target_devices)]() {
          identity_test_env_adaptor_->identity_test_env()
              ->MakePrimaryAccountAvailable("user@example.com",
                                            signin::ConsentLevel::kSignin);
          StubSendTabToSelfSyncService* sync_service =
              static_cast<StubSendTabToSelfSyncService*>(
                  SendTabToSelfSyncServiceFactory::GetForProfile(
                      browser()->GetProfile()));
          sync_service->GetFakeSendTabToSelfModel()
              ->SetTargetDeviceInfoSortedList(devices);
        }),
        ShowBubble(),
        WaitForShow(SendTabToSelfDevicePickerBubbleView::
                        kSendTabToSelfDevicePickerBubbleId));
  }

  auto SetUpSingleDeviceAndShowBubble() {
    return SetUpTargetDevicesAndShowBubble(
        {RemoteDeviceInfo("device_1", "device_1", FormFactor::kDesktop,
                          OsType::kLinux, base::Time::Now(),
                          /*has_high_precision_timestamp=*/false)});
  }

 protected:
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

// Tests that sending a tab to a device shows the device picker bubble and then
// a success toast after a device is selected.
IN_PROC_BROWSER_TEST_F(SendTabToSelfInteractiveUiTest,
                       SendTabShowsBubbleAndToast) {
  RunTestSequence(SetUpSingleDeviceAndShowBubble(), Do([this]() {
                    SendTabToSelfBubbleController::GetOrCreateForWebContents(
                        browser()->GetTabStripModel()->GetActiveWebContents())
                        ->OnDeviceSelected("device_1", "device_1");
                  }),
                  WaitForShow(toasts::ToastView::kToastViewId),
                  StopToastTimer(),
                  Screenshot(toasts::ToastView::kToastViewId,
                             /*screenshot_name=*/"SendTabToSelfSuccessToast",
                             /*baseline_cl=*/kScreenshotBaselineCL),
                  Do([this]() {
                    SendTabToSelfBubbleController::GetOrCreateForWebContents(
                        browser()->GetTabStripModel()->GetActiveWebContents())
                        ->HideBubble();
                  }),
                  WaitForHide(SendTabToSelfDevicePickerBubbleView::
                                  kSendTabToSelfDevicePickerBubbleId));
}

// Verifies that the device selection bubble selects and focuses the first
// device by default and sends the tab when the Send button is pressed.
IN_PROC_BROWSER_TEST_F(SendTabToSelfDeviceSelectionInteractiveUiTest,
                       SendTabShowsBubbleAndToastDeviceSelection) {
  RunTestSequence(
      SetUpSingleDeviceAndShowBubble(),
      // Capture a screenshot of the target picker bubble for Gold pixel
      // verification.
      Screenshot(SendTabToSelfDevicePickerBubbleView::
                     kSendTabToSelfDevicePickerBubbleId,
                 /*screenshot_name=*/"SendTabToSelfDevicePickerDeviceSelection",
                 /*baseline_cl=*/kScreenshotBaselineCL),
      NameDescendantViewByType<SendTabToSelfBubbleDeviceButton>(
          SendTabToSelfDevicePickerBubbleView::
              kSendTabToSelfDevicePickerBubbleId,
          "device_button_1", 0),
      CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                        &views::View::GetEnabled, true),
      CheckViewProperty("device_button_1",
                        &SendTabToSelfBubbleDeviceButton::IsSelected, true),
      // Verify that initial focus lands on the selected device button so screen
      // reader users hear the currently selected device in the list of options
      // first.
      CheckViewProperty("device_button_1", &views::View::HasFocus, true),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForShow(toasts::ToastView::kToastViewId), StopToastTimer(),
      Do([this]() {
        SendTabToSelfBubbleController::GetOrCreateForWebContents(
            browser()->GetTabStripModel()->GetActiveWebContents())
            ->HideBubble();
      }),
      WaitForHide(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfDeviceSelectionInteractiveUiTest,
                       SendTabMultipleDevicesDeviceSelection) {
  RunTestSequence(
      SetUpTargetDevicesAndShowBubble(
          {RemoteDeviceInfo("device_1", "device_1", FormFactor::kDesktop,
                            OsType::kLinux, base::Time::Now(),
                            /*has_high_precision_timestamp=*/false),
           RemoteDeviceInfo("device_2", "device_2", FormFactor::kPhone,
                            OsType::kAndroid, base::Time::Now(),
                            /*has_high_precision_timestamp=*/false)}),
      NameDescendantViewByType<SendTabToSelfBubbleDeviceButton>(
          SendTabToSelfDevicePickerBubbleView::
              kSendTabToSelfDevicePickerBubbleId,
          "device_button_1", 0),
      NameDescendantViewByType<SendTabToSelfBubbleDeviceButton>(
          SendTabToSelfDevicePickerBubbleView::
              kSendTabToSelfDevicePickerBubbleId,
          "device_button_2", 1),
      CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                        &views::View::GetEnabled, true),
      CheckViewProperty("device_button_1",
                        &SendTabToSelfBubbleDeviceButton::IsSelected, true),
      // Verify that initial focus lands on the selected device button so screen
      // reader users hear the currently selected device in the list of options
      // first.
      CheckViewProperty("device_button_1", &views::View::HasFocus, true),
      CheckViewProperty("device_button_2",
                        &SendTabToSelfBubbleDeviceButton::IsSelected, false),
      // Clicking the already selected device should keep it selected.
      PressButton("device_button_1"),
      CheckViewProperty("device_button_1",
                        &SendTabToSelfBubbleDeviceButton::IsSelected, true),
      PressButton("device_button_1"),
      CheckViewProperty("device_button_1",
                        &SendTabToSelfBubbleDeviceButton::IsSelected, true),
      PressButton("device_button_2"),
      CheckViewProperty("device_button_1",
                        &SendTabToSelfBubbleDeviceButton::IsSelected, false),
      CheckViewProperty("device_button_2",
                        &SendTabToSelfBubbleDeviceButton::IsSelected, true),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForShow(toasts::ToastView::kToastViewId), StopToastTimer(),
      Do([this]() {
        SendTabToSelfBubbleController::GetOrCreateForWebContents(
            browser()->GetTabStripModel()->GetActiveWebContents())
            ->HideBubble();
      }),
      WaitForHide(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfDeviceSelectionInteractiveUiTest,
                       CancelClosesBubbleWithoutSending) {
  RunTestSequence(SetUpSingleDeviceAndShowBubble(),
                  // Click the Cancel button
                  PressButton(views::DialogClientView::kCancelButtonElementId),
                  // The bubble should hide, and NO toast should be shown.
                  WaitForHide(SendTabToSelfDevicePickerBubbleView::
                                  kSendTabToSelfDevicePickerBubbleId),
                  EnsureNotPresent(toasts::ToastView::kToastViewId));
}

// Tests that clicking the "Manage devices" link in the device picker bubble
// navigates to the Google Account device activity page in a new foreground tab.
IN_PROC_BROWSER_TEST_F(SendTabToSelfDeviceSelectionInteractiveUiTest,
                       ClickManageDevicesLinkOpensPageInNewTab) {
  RunTestSequence(
      SetUpSingleDeviceAndShowBubble(),
      // Clicking the "Manage devices" link opens the account devices URL in a
      // new foreground tab and closes the bubble.
      DoDefaultAction(
          SendTabToSelfDevicePickerBubbleView::kManageDevicesLinkElementId),
      WaitForHide(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId),
      CheckResult([this]() { return browser()->GetTabStripModel()->count(); },
                  2),
      CheckResult(
          [this]() {
            return browser()
                ->GetTabStripModel()
                ->GetActiveWebContents()
                ->GetVisibleURL();
          },
          GURL(chrome::kGoogleAccountDeviceActivityURL)));
}

}  // namespace

}  // namespace send_tab_to_self
