// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/browser/ui/views/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7765290";

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
        context, base::BindRepeating(&BuildStubSyncService));
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
        sync_service->GetModelFake()->SetTargetDeviceInfoSortedList(
            {TargetDeviceInfo("device_1", "device_1",
                              syncer::DeviceInfo::FormFactor::kDesktop,
                              base::Time::Now())});
      }),
      ShowBubble(),
      WaitForShow(SendTabToSelfDevicePickerBubbleView::
                      kSendTabToSelfDevicePickerBubbleId),
      Do([this]() {
        SendTabToSelfBubbleController::CreateOrGetFromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->OnDeviceSelected("device_1");
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

}  // namespace

}  // namespace send_tab_to_self
