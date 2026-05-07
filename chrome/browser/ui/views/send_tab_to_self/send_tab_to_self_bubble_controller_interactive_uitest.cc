// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string_view>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace send_tab_to_self {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7819337";

class SendTabToSelfInteractiveUiTest : public InteractiveBrowserTest {
 public:
  SendTabToSelfInteractiveUiTest() {
    feature_list_.InitWithFeatures({kSendTabToSelfPostSendToast},
                                   {kSendTabToSelfPropagateScrollPosition});
  }

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
            browser()->profile());
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
      SendTabToSelfBubbleController::CreateOrGetFromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->ShowBubble();
    });
  }

  auto StopToastTimer() {
    return Do([this]() {
      browser()
          ->browser_window_features()
          ->toast_controller()
          ->GetToastCloseTimerForTesting()
          ->Stop();
    });
  }

  auto ShowToast(ToastParams params) {
    return Do(base::BindOnce(
        [](ToastController* toast_controller, ToastParams toast_params) {
          toast_controller->MaybeShowToast(std::move(toast_params));
          toast_controller->GetToastCloseTimerForTesting()->Stop();
        },
        browser()->browser_window_features()->toast_controller(),
        std::move(params)));
  }

  SendTabToSelfDevicePickerBubbleView* GetBubbleView() {
    return static_cast<SendTabToSelfDevicePickerBubbleView*>(
        SendTabToSelfBubbleController::CreateOrGetFromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->send_tab_to_self_bubble_view());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

// Tests that sending a tab to a device shows the device picker bubble and then
// a success toast after a device is selected.
IN_PROC_BROWSER_TEST_F(SendTabToSelfInteractiveUiTest,
                       SendTabShowsBubbleAndToast) {
  const GURL test_url("chrome://flags/");
  RunTestSequence(
      InstrumentTab(kPrimaryTabId),
      NavigateWebContents(kPrimaryTabId, test_url),
      FocusWebContents(kPrimaryTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Should only run in pixel_tests."),
      Do([this]() {
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@example.com",
                                          signin::ConsentLevel::kSignin);
        StubSendTabToSelfSyncService* sync_service =
            static_cast<StubSendTabToSelfSyncService*>(
                SendTabToSelfSyncServiceFactory::GetForProfile(
                    browser()->profile()));
        sync_service->GetFakeSendTabToSelfModel()
            ->SetTargetDeviceInfoSortedList({TargetDeviceInfo(
                "device_1", "device_1",
                syncer::DeviceInfo::FormFactor::kDesktop, base::Time::Now())});
      }),
      ShowBubble(),
      WaitForShow(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId),
      Do([this]() {
        SendTabToSelfBubbleController::CreateOrGetFromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->OnDeviceSelected("device_1", "device_1");
      }),
      WaitForShow(toasts::ToastView::kToastViewId), StopToastTimer(),
      Screenshot(toasts::ToastView::kToastViewId,
                 /*screenshot_name=*/"SendTabToSelfSuccessToast",
                 /*baseline_cl=*/kScreenshotBaselineCL),
      Do([this]() {
        SendTabToSelfBubbleController::CreateOrGetFromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->HideBubble();
      }),
      WaitForHide(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId));
}

class SendTabToSelfDeviceSelectionInteractiveUiTest
    : public SendTabToSelfInteractiveUiTest {
 public:
  SendTabToSelfDeviceSelectionInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(kSendTabToSelfEnhancedDesktopUI);
  }
  ~SendTabToSelfDeviceSelectionInteractiveUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfDeviceSelectionInteractiveUiTest,
                       SendTabShowsBubbleAndToastDeviceSelection) {
  const GURL test_url("chrome://flags/");
  RunTestSequence(
      InstrumentTab(kPrimaryTabId),
      NavigateWebContents(kPrimaryTabId, test_url),
      FocusWebContents(kPrimaryTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Should only run in pixel_tests."),
      Do([this]() {
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@example.com",
                                          signin::ConsentLevel::kSignin);
        StubSendTabToSelfSyncService* sync_service =
            static_cast<StubSendTabToSelfSyncService*>(
                SendTabToSelfSyncServiceFactory::GetForProfile(
                    browser()->profile()));
        sync_service->GetFakeSendTabToSelfModel()
            ->SetTargetDeviceInfoSortedList({TargetDeviceInfo(
                "device_1", "device_1",
                syncer::DeviceInfo::FormFactor::kDesktop, base::Time::Now())});
      }),
      ShowBubble(),
      WaitForShow(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId),
      // Capture a screenshot of the modernized bubble for Gold pixel
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
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForShow(toasts::ToastView::kToastViewId), StopToastTimer(),
      Do([this]() {
        SendTabToSelfBubbleController::CreateOrGetFromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->HideBubble();
      }),
      WaitForHide(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfDeviceSelectionInteractiveUiTest,
                       SendTabMultipleDevicesDeviceSelection) {
  const GURL test_url("chrome://flags/");
  RunTestSequence(
      InstrumentTab(kPrimaryTabId),
      NavigateWebContents(kPrimaryTabId, test_url),
      FocusWebContents(kPrimaryTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Should only run in pixel_tests."),
      Do([this]() {
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@example.com",
                                          signin::ConsentLevel::kSignin);
        StubSendTabToSelfSyncService* sync_service =
            static_cast<StubSendTabToSelfSyncService*>(
                SendTabToSelfSyncServiceFactory::GetForProfile(
                    browser()->profile()));
        sync_service->GetFakeSendTabToSelfModel()
            ->SetTargetDeviceInfoSortedList(
                {TargetDeviceInfo("device_1", "device_1",
                                  syncer::DeviceInfo::FormFactor::kDesktop,
                                  base::Time::Now()),
                 TargetDeviceInfo("device_2", "device_2",
                                  syncer::DeviceInfo::FormFactor::kPhone,
                                  base::Time::Now())});
      }),
      ShowBubble(),
      WaitForShow(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId),
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
        SendTabToSelfBubbleController::CreateOrGetFromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->HideBubble();
      }),
      WaitForHide(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfDeviceSelectionInteractiveUiTest,
                       CancelClosesBubbleWithoutSending) {
  const GURL test_url("chrome://flags/");
  RunTestSequence(
      InstrumentTab(kPrimaryTabId),
      NavigateWebContents(kPrimaryTabId, test_url), Do([this]() {
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@example.com",
                                          signin::ConsentLevel::kSignin);
        StubSendTabToSelfSyncService* sync_service =
            static_cast<StubSendTabToSelfSyncService*>(
                SendTabToSelfSyncServiceFactory::GetForProfile(
                    browser()->profile()));
        sync_service->GetFakeSendTabToSelfModel()
            ->SetTargetDeviceInfoSortedList({TargetDeviceInfo(
                "device_1", "device_1",
                syncer::DeviceInfo::FormFactor::kDesktop, base::Time::Now())});
      }),
      ShowBubble(),
      WaitForShow(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId),
      // Click the Cancel button
      PressButton(views::DialogClientView::kCancelButtonElementId),
      // The bubble should hide, and NO toast should be shown.
      WaitForHide(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId),
      EnsureNotPresent(toasts::ToastView::kToastViewId));
}

}  // namespace

}  // namespace send_tab_to_self
