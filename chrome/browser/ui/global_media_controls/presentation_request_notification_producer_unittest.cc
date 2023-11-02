// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/test/mock_media_dialog_delegate.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    feature_list_.InitAndEnableFeature(
        media_router::kGlobalMediaControlsCastStartStop);
    ChromeRenderViewHostTestHarness::SetUp();

    media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&media_router::MockMediaRouter::Create));
    notification_service_ =
        std::make_unique<MediaNotificationService>(profile(), false);
    notification_producer_ =
        notification_service_->presentation_request_notification_producer_
            .get();

    presentation_manager_ =
        std::make_unique<NiceMock<MockWebContentsPresentationManager>>();
    notification_producer_->SetTestPresentationManager(
        presentation_manager_->GetWeakPtr());
  }

  void TearDown() override {
    notification_service_.reset();
    media_router::WebContentsPresentationManager::SetTestInstance(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SimulateDialogOpenedAndWait(
      global_media_controls::test::MockMediaDialogDelegate* delegate) {
    notification_service_->media_item_manager()->SetDialogDelegate(delegate);
    task_environment()->RunUntilIdle();
  }

  void SimulateDialogClosedAndWait(
      global_media_controls::test::MockMediaDialogDelegate* delegate) {
    notification_service_->media_item_manager()->SetDialogDelegate(nullptr);
    task_environment()->RunUntilIdle();
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
    notification_producer_->OnStartPresentationContextCreated(
        std::move(context));
  }

  void SimulatePresentationsChanged(bool has_presentation) {
    notification_producer_->OnPresentationsChanged(has_presentation);
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

 protected:
  std::unique_ptr<MediaNotificationService> notification_service_;
  raw_ptr<PresentationRequestNotificationProducer> notification_producer_ =
      nullptr;
  std::unique_ptr<MockWebContentsPresentationManager> presentation_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PresentationRequestNotificationProducerTest,
       HideItemOnMediaRoutesChanged) {
  SimulateStartPresentationContextCreated();
  SimulatePresentationsChanged(true);
  EXPECT_FALSE(notification_service_->media_item_manager()->HasOpenDialog());
  task_environment()->RunUntilIdle();
}

TEST_F(PresentationRequestNotificationProducerTest, DismissNotification) {
  SimulateStartPresentationContextCreated();
  auto item = notification_producer_->GetNotificationItem();
  ASSERT_TRUE(item);

  notification_producer_->OnMediaItemUIDismissed(item->id());
  EXPECT_FALSE(notification_producer_->GetNotificationItem());
}

TEST_F(PresentationRequestNotificationProducerTest, OnMediaDialogOpened) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate> delegate;
  // Open the dialog on a page without a default presentation request.
  SimulateDialogOpenedAndWait(&delegate);
  EXPECT_FALSE(notification_producer_->GetNotificationItem());
  SimulateDialogClosedAndWait(&delegate);

  // Open the dialog on a page with default presentation request and there does
  // not exist a notification for non-default presentation request. A dummy
  // notification should be created.
  presentation_manager_->SetDefaultPresentationRequest(
      CreatePresentationRequest());
  SimulateDialogOpenedAndWait(&delegate);
  EXPECT_TRUE(notification_producer_->GetNotificationItem());
  SimulateDialogClosedAndWait(&delegate);
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnMediaDialogOpenedWithExistingItem) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate> delegate;

  // Open the dialog on a page with default presentation request and there
  // exists a notification for non-default presentation request. The existing
  // notification should not be replaced.
  SimulateStartPresentationContextCreated();
  auto id = notification_producer_->GetNotificationItem()->id();
  presentation_manager_->SetDefaultPresentationRequest(
      CreatePresentationRequest());
  SimulateDialogOpenedAndWait(&delegate);
  EXPECT_TRUE(notification_producer_->GetNotificationItem());
  EXPECT_EQ(id, notification_producer_->GetNotificationItem()->id());
  SimulateDialogClosedAndWait(&delegate);
}

TEST_F(PresentationRequestNotificationProducerTest, DeleteItem) {
  content::RenderFrameHost* child_frame = CreateChildFrame();
  NiceMock<global_media_controls::test::MockMediaDialogDelegate> delegate;
  SimulateDialogOpenedAndWait(&delegate);
  // Simulate a PresentationRequest from |child_frame|.
  notification_producer_->OnStartPresentationContextCreated(
      std::make_unique<media_router::StartPresentationContext>(
          content::PresentationRequest(child_frame->GetGlobalId(),
                                       {GURL(), GURL()},
                                       url::Origin::Create(GURL())),
          base::DoNothing(), base::DoNothing()));

  // Detach |child_frame|.
  content::RenderFrameHostTester::For(child_frame)->Detach();

  SimulateDialogClosedAndWait(&delegate);
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnPresentationRequestWebContentsNavigated) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate> delegate;

  // Navigating to another page should delete the notification.
  SimulateStartPresentationContextCreated();
  SimulateDialogOpenedAndWait(&delegate);
  EXPECT_CALL(
      delegate,
      HideMediaItem(notification_producer_->GetNotificationItem()->id()))
      .Times(AtLeast(1));
  NavigateAndCommit(GURL("https://www.google.com/"));
  EXPECT_FALSE(notification_producer_->GetNotificationItem());
  SimulateDialogClosedAndWait(&delegate);
}

TEST_F(PresentationRequestNotificationProducerTest,
       OnPresentationRequestWebContentsDestroyed) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate> delegate;

  // Removing the WebContents should delete the notification.
  SimulateStartPresentationContextCreated();
  SimulateDialogOpenedAndWait(&delegate);
  EXPECT_CALL(
      delegate,
      HideMediaItem(notification_producer_->GetNotificationItem()->id()))
      .Times(AtLeast(1));
  DeleteContents();
  EXPECT_FALSE(notification_producer_->GetNotificationItem());
  SimulateDialogClosedAndWait(&delegate);
}

TEST_F(PresentationRequestNotificationProducerTest,
       InvokeCallbackOnDialogClosed) {
  NiceMock<global_media_controls::test::MockMediaDialogDelegate> delegate;

  // PRNP should invoke |mock_error_cb| after the media dialog is closed.
  base::MockCallback<content::PresentationConnectionErrorCallback>
      mock_error_cb;
  EXPECT_CALL(mock_error_cb, Run);
  auto context = std::make_unique<media_router::StartPresentationContext>(
      CreatePresentationRequest(), base::DoNothing(), mock_error_cb.Get());
  notification_service_->OnStartPresentationContextCreated(std::move(context));
  SimulateDialogOpenedAndWait(&delegate);
  SimulateDialogClosedAndWait(&delegate);
}
