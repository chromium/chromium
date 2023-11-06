// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/test/mock_media_dialog_delegate.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/test_crosapi_environment.h"
#endif

using global_media_controls::test::MockDevicePickerProvider;
using testing::_;
using testing::AtLeast;
using testing::NiceMock;

class PresentationRequestNotificationProducerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PresentationRequestNotificationProducerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::MainThreadType::UI) {}
  ~PresentationRequestNotificationProducerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&media_router::MockMediaRouter::Create));

    device_picker_host_ =
        std::make_unique<PresentationRequestNotificationProducer>(
            base::BindRepeating(&PresentationRequestNotificationProducerTest::
                                    HasActiveNotificationsForWebContents,
                                base::Unretained(this)),
            base::UnguessableToken::Create());

    presentation_manager_ =
        std::make_unique<NiceMock<MockWebContentsPresentationManager>>();
    device_picker_host_->SetTestPresentationManager(
        presentation_manager_->GetWeakPtr());

    device_picker_host_->BindProvider(item_provider_.PassRemote());
  }

  void TearDown() override {
    media_router::WebContentsPresentationManager::SetTestInstance(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::PresentationRequest CreatePresentationRequest() {
    return content::PresentationRequest(
        main_rfh()->GetGlobalId(),
        {GURL("http://example.com"), GURL("http://example2.com")},
        url::Origin::Create(GURL("http://google.com")));
  }

  void SimulateStartPresentationContextCreated() {
    auto context = std::make_unique<media_router::StartPresentationContext>(
        CreatePresentationRequest(), base::DoNothing(), base::DoNothing());
    device_picker_host_->OnStartPresentationContextCreated(std::move(context));
  }

  void SimulatePresentationsChanged(bool has_presentation) {
    device_picker_host_->OnPresentationsChanged(has_presentation);
  }

  content::RenderFrameHost* CreateChildFrame() {
    NavigateAndCommit(GURL("about:blank"));

    content::RenderFrameHost* child_frame = main_rfh();
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(child_frame);
    child_frame = rfh_tester->AppendChild("childframe");
    content::MockNavigationHandle handle(GURL(), child_frame);
    handle.set_has_committed(true);
    return child_frame;
  }

  bool HasActiveNotificationsForWebContents(
      content::WebContents* web_contents) {
    return true;
  }

 protected:
  std::unique_ptr<PresentationRequestNotificationProducer> device_picker_host_;
  std::unique_ptr<MockWebContentsPresentationManager> presentation_manager_;
  MockDevicePickerProvider item_provider_;
};

TEST_F(PresentationRequestNotificationProducerTest,
       HideItemOnMediaRoutesChanged) {
  SimulateStartPresentationContextCreated();
  SimulatePresentationsChanged(true);
  EXPECT_CALL(item_provider_, DeleteItem());
  task_environment()->RunUntilIdle();
}

TEST_F(PresentationRequestNotificationProducerTest, DismissNotification) {
  SimulateStartPresentationContextCreated();
  auto item = device_picker_host_->GetNotificationItem();
  ASSERT_TRUE(item);

  device_picker_host_->OnPickerDismissed();
  EXPECT_FALSE(device_picker_host_->GetNotificationItem());
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnMediaDialogOpenedWithoutPresentationRequest) {
  // Open the dialog on a page without a default presentation request. A dummy
  // notification should not be created.
  device_picker_host_->OnMediaUIOpened();
  EXPECT_FALSE(device_picker_host_->GetNotificationItem());
  device_picker_host_->OnMediaUIClosed();
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnMediaDialogOpenedWithPresentationRequest) {
  // Open the dialog on a page with default presentation request and there does
  // not exist a notification for non-default presentation request. A dummy
  // notification should be created.
  presentation_manager_->SetDefaultPresentationRequest(
      CreatePresentationRequest());
  device_picker_host_->OnMediaUIOpened();
  EXPECT_TRUE(device_picker_host_->GetNotificationItem());
  device_picker_host_->OnMediaUIClosed();
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnMediaDialogOpenedWithExistingItem) {
  // Open the dialog on a page with default presentation request and there
  // exists a notification for non-default presentation request. The existing
  // notification should not be replaced.
  SimulateStartPresentationContextCreated();
  presentation_manager_->SetDefaultPresentationRequest(
      CreatePresentationRequest());
  device_picker_host_->OnMediaUIOpened();
  EXPECT_TRUE(device_picker_host_->GetNotificationItem());
  device_picker_host_->OnMediaUIClosed();
}

TEST_F(PresentationRequestNotificationProducerTest, DeleteItem) {
  content::RenderFrameHost* child_frame = CreateChildFrame();
  device_picker_host_->OnMediaUIOpened();
  // Simulate a PresentationRequest from `child_frame`.
  device_picker_host_->OnStartPresentationContextCreated(
      std::make_unique<media_router::StartPresentationContext>(
          content::PresentationRequest(child_frame->GetGlobalId(),
                                       {GURL(), GURL()},
                                       url::Origin::Create(GURL())),
          base::DoNothing(), base::DoNothing()));

  // Detach `child_frame`.
  content::RenderFrameHostTester::For(child_frame)->Detach();

  device_picker_host_->OnMediaUIClosed();
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnPresentationRequestWebContentsNavigated) {
  // Navigating to another page should delete the notification.
  SimulateStartPresentationContextCreated();
  device_picker_host_->OnMediaUIOpened();
  EXPECT_CALL(item_provider_, DeleteItem());
  NavigateAndCommit(GURL("https://www.google.com/"));
  EXPECT_FALSE(device_picker_host_->GetNotificationItem());
  device_picker_host_->OnMediaUIClosed();
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnPresentationRequestWebContentsDestroyed) {
  // Removing the WebContents should delete the notification.
  SimulateStartPresentationContextCreated();
  device_picker_host_->OnMediaUIOpened();
  EXPECT_CALL(item_provider_, DeleteItem());
  DeleteContents();
  EXPECT_FALSE(device_picker_host_->GetNotificationItem());
  device_picker_host_->OnMediaUIClosed();
}

TEST_F(PresentationRequestNotificationProducerTest,
       InvokeCallbackOnDialogClosed) {
  base::MockCallback<content::PresentationConnectionErrorCallback>
      mock_error_cb;
  auto context = std::make_unique<media_router::StartPresentationContext>(
      CreatePresentationRequest(), base::DoNothing(), mock_error_cb.Get());
  device_picker_host_->OnStartPresentationContextCreated(std::move(context));
  device_picker_host_->OnMediaUIOpened();

  // Closing the media UI should invoke the presentation error callback.
  EXPECT_CALL(mock_error_cb, Run);
  device_picker_host_->OnMediaUIClosed();
}
