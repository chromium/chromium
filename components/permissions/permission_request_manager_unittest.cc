// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace permissions {

namespace {
using QuietUiReason = PermissionUiSelector::QuietUiReason;
}

class PermissionRequestManagerTest : public content::RenderViewHostTestHarness {
 public:
  PermissionRequestManagerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        request1_(RequestType::kGeolocation,
                  PermissionRequestGestureType::GESTURE),
        request2_(RequestType::kMultipleDownloads,
                  PermissionRequestGestureType::NO_GESTURE),
        request_mic_(RequestType::kMicStream,
                     PermissionRequestGestureType::NO_GESTURE),
        request_camera_(RequestType::kCameraStream,
                        PermissionRequestGestureType::NO_GESTURE),
#if !BUILDFLAG(IS_ANDROID)
        request_ptz_(RequestType::kCameraPanTiltZoom,
                     PermissionRequestGestureType::NO_GESTURE),
#endif
        iframe_request_same_domain_(GURL("https://www.google.com/some/url"),
                                    RequestType::kMidiSysex),
        iframe_request_other_domain_(GURL("https://www.youtube.com"),
                                     RequestType::kClipboard),
        iframe_request_camera_other_domain_(GURL("https://www.youtube.com"),
                                            RequestType::kStorageAccess),
        iframe_request_mic_other_domain_(GURL("https://www.youtube.com"),
                                         RequestType::kMicStream) {
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(MockPermissionRequest::kDefaultOrigin));

    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    manager_->set_enabled_app_level_notification_permission_for_testing(true);
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  void Accept() {
    manager_->Accept();
    task_environment()->RunUntilIdle();
  }

  void AcceptThisTime() {
    manager_->AcceptThisTime();
    task_environment()->RunUntilIdle();
  }

  void Deny() {
    manager_->Deny();
    task_environment()->RunUntilIdle();
  }

  void Closing() {
    manager_->Dismiss();
    task_environment()->RunUntilIdle();
  }

  void OpenHelpCenterLink() {
#if !BUILDFLAG(IS_ANDROID)
    const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(), 0, 0);
#else  // BUILDFLAG(IS_ANDROID)
    const ui::TouchEvent event(
        ui::EventType::kTouchMoved, gfx::PointF(), gfx::PointF(),
        ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 1));
#endif
    manager_->OpenHelpCenterLink(event);
    task_environment()->RunUntilIdle();
  }

  void WaitForFrameLoad() {
    // PermissionRequestManager ignores all parameters. Yay?
    manager_->DOMContentLoaded(nullptr);
    task_environment()->RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInPrimaryMainFrame();
    task_environment()->RunUntilIdle();
  }

  void MockTabSwitchAway() {
    manager_->OnVisibilityChanged(content::Visibility::HIDDEN);
  }

  void MockTabSwitchBack() {
    manager_->OnVisibilityChanged(content::Visibility::VISIBLE);
  }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

  std::unique_ptr<MockPermissionRequest> CreateAndAddRequest(
      RequestType type,
      bool should_be_seen,
      int expected_request_count) {
    std::unique_ptr<MockPermissionRequest> request =
        std::make_unique<MockPermissionRequest>(
            type, PermissionRequestGestureType::GESTURE);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), request.get());
    WaitForBubbleToBeShown();
    if (should_be_seen) {
      EXPECT_TRUE(prompt_factory_->RequestTypeSeen(type));
    } else {
      EXPECT_FALSE(prompt_factory_->RequestTypeSeen(type));
    }
    EXPECT_EQ(prompt_factory_->TotalRequestCount(), expected_request_count);

    return request;
  }

  void WaitAndAcceptPromptForRequest(MockPermissionRequest* request) {
    WaitForBubbleToBeShown();

    EXPECT_FALSE(request->finished());
    EXPECT_TRUE(prompt_factory_->is_visible());
    ASSERT_EQ(prompt_factory_->request_count(), 1);

    Accept();
    EXPECT_TRUE(request->granted());
  }

 protected:
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  MockPermissionRequest request_mic_;
  MockPermissionRequest request_camera_;
#if !BUILDFLAG(IS_ANDROID)
  MockPermissionRequest request_ptz_;
#endif
  MockPermissionRequest iframe_request_same_domain_;
  MockPermissionRequest iframe_request_other_domain_;
  MockPermissionRequest iframe_request_camera_other_domain_;
  MockPermissionRequest iframe_request_mic_other_domain_;
  raw_ptr<PermissionRequestManager, DanglingUntriaged> manager_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  TestPermissionsClient client_;
  base::test::ScopedFeatureList feature_list_;
};

////////////////////////////////////////////////////////////////////////////////
// General
////////////////////////////////////////////////////////////////////////////////

TEST_F(PermissionRequestManagerTest, NoRequests) {
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, SingleRequest) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionRequestManagerTest, SequentialRequests) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  Accept();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request2_.granted());
}

TEST_F(PermissionRequestManagerTest, ForgetRequestsOnPageNavigation) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_other_domain_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForBubbleToBeShown();

  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, RequestsDontNeedUserGesture) {
  WaitForFrameLoad();
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_other_domain_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, RequestsNotSupported) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  Accept();
  EXPECT_TRUE(request1_.granted());

  manager_->set_web_contents_supports_permission_requests(false);

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  EXPECT_TRUE(request2_.cancelled());
}

////////////////////////////////////////////////////////////////////////////////
// Requests grouping
////////////////////////////////////////////////////////////////////////////////

// Android is the only platform that does not support the permission chip.
#if BUILDFLAG(IS_ANDROID)
// Most requests should never be grouped.
// Grouping for chip feature is tested in ThreeRequestsStackOrderChip.
TEST_F(PermissionRequestManagerTest, TwoRequestsUngrouped) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());

  ASSERT_EQ(prompt_factory_->show_count(), 2);
}

// Tests for non-Android platforms which support the permission chip.
#else   // BUILDFLAG(IS_ANDROID)
TEST_F(PermissionRequestManagerTest, ThreeRequestsStackOrderChip) {
  // Test new permissions order, requests shouldn't be grouped.
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Test new permissions order by adding requests one at a time.
TEST_F(PermissionRequestManagerTest, ThreeRequestsOneByOneStackOrderChip) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Only mic/camera requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, MicCameraGrouped) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
}

// If mic/camera requests come from different origins, they should not be
// grouped.
TEST_F(PermissionRequestManagerTest, MicCameraDifferentOrigins) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

#if !BUILDFLAG(IS_ANDROID)
// Only camera/ptz requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, CameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());
}

TEST_F(PermissionRequestManagerTest, CameraPtzDifferentOrigins) {
  // If camera/ptz requests come from different origins, they should not be
  // grouped.
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_camera_other_domain_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

// Only mic/camera/ptz requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, MicCameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 3);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());
}

// If mic/camera/ptz requests come from different origins, they should not be
// grouped.
TEST_F(PermissionRequestManagerTest, MicCameraPtzDifferentOrigins) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  // Requests should be split into two groups and each one will contain less
  // than 3 requests (1 request + 2 request for current logic and 2 requests + 1
  // request for chip).
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
  Accept();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests mix of grouped media requests and non-groupable request.
TEST_F(PermissionRequestManagerTest, MixOfMediaAndNotMediaRequests) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  // Requests should be split into two groups and each one will contain less
  // than 3 requests (1 request + 2 request for current logic and 2 requests + 1
  // request for chip).
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
  Accept();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
  Accept();
}

TEST_F(PermissionRequestManagerTest, OpenHelpCenterLink) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_camera_other_domain_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  OpenHelpCenterLink();
  SUCCEED();
}

TEST_F(PermissionRequestManagerTest, OpenHelpCenterLink_RequestNotSupported) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  EXPECT_DEATH_IF_SUPPORTED(OpenHelpCenterLink(), "");
}

////////////////////////////////////////////////////////////////////////////////
// Tab switching
////////////////////////////////////////////////////////////////////////////////

#if BUILDFLAG(IS_ANDROID)
TEST_F(PermissionRequestManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  MockTabSwitchAway();
  EXPECT_TRUE(prompt_factory_->is_visible());

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PermissionRequestManagerTest, PermissionRequestWhileTabSwitchedAway) {
  MockTabSwitchAway();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
}

////////////////////////////////////////////////////////////////////////////////
// Duplicated requests
////////////////////////////////////////////////////////////////////////////////

TEST_F(PermissionRequestManagerTest, SameRequestRejected) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, WeakDuplicateRequests) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  auto dupe_request_1 = request1_.CreateDuplicateRequest();
  auto dupe_request_2 = request1_.CreateDuplicateRequest();
  auto dupe_request_3 = request1_.CreateDuplicateRequest();
  auto dupe_request_4 = request1_.CreateDuplicateRequest();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_1.get());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_2.get());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_3.get());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_4.get());
  dupe_request_1.reset();
  dupe_request_3.reset();
  EXPECT_EQ(4ul, manager_->duplicate_requests_.front().size());
  EXPECT_EQ(1ul, manager_->duplicate_requests_.size());
  auto request_list = manager_->FindDuplicateRequestList(&request1_);
  EXPECT_NE(request_list, manager_->duplicate_requests_.end());
  EXPECT_EQ(3ul, manager_->duplicate_requests_.front().size());
  dupe_request_4.reset();
  manager_->VisitDuplicateRequests(
      base::BindRepeating(
          [](const base::WeakPtr<PermissionRequest>& weak_request) {}),
      &request1_);
  EXPECT_EQ(1ul, manager_->duplicate_requests_.front().size());
  EXPECT_EQ(1ul, manager_->duplicate_requests_.size());
  Accept();
  EXPECT_EQ(0ul, manager_->duplicate_requests_.size());
}

class QuicklyDeletedRequest : public PermissionRequest {
 public:
  QuicklyDeletedRequest(const GURL& requesting_origin,
                        RequestType request_type,
                        PermissionRequestGestureType gesture_type)
      : PermissionRequest(
            requesting_origin,
            request_type,
            gesture_type == PermissionRequestGestureType::GESTURE,
            base::BindLambdaForTesting(
                [](ContentSetting result,
                   bool is_one_time,
                   bool is_final_decision) { NOTREACHED_IN_MIGRATION(); }),
            base::NullCallback()) {}

  static std::unique_ptr<QuicklyDeletedRequest> CreateRequest(
      MockPermissionRequest* request) {
    return std::make_unique<QuicklyDeletedRequest>(request->requesting_origin(),
                                                   request->request_type(),
                                                   request->GetGestureType());
  }
};

TEST_F(PermissionRequestManagerTest, WeakDuplicateRequestsAccept) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  auto dupe_request_1 = QuicklyDeletedRequest::CreateRequest(&request1_);
  auto dupe_request_2 = request1_.CreateDuplicateRequest();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_1.get());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_2.get());
  auto dupe_request_3 = QuicklyDeletedRequest::CreateRequest(&request2_);
  auto dupe_request_4 = request2_.CreateDuplicateRequest();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_3.get());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request_4.get());
  dupe_request_1.reset();
  dupe_request_3.reset();
  EXPECT_EQ(2ul, manager_->duplicate_requests_.size());
  EXPECT_EQ(2ul, manager_->duplicate_requests_.front().size());
  EXPECT_EQ(2ul, manager_->duplicate_requests_.back().size());
  WaitForBubbleToBeShown();
  Accept();
  EXPECT_EQ(1ul, manager_->duplicate_requests_.size());
  WaitForBubbleToBeShown();
  Accept();
  EXPECT_EQ(0ul, manager_->duplicate_requests_.size());
}

TEST_F(PermissionRequestManagerTest, DuplicateRequest) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);

  auto dupe_request = request1_.CreateDuplicateRequest();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request.get());
  EXPECT_FALSE(dupe_request->finished());
  EXPECT_FALSE(request1_.finished());

  auto dupe_request2 = request2_.CreateDuplicateRequest();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       dupe_request2.get());
  EXPECT_FALSE(dupe_request2->finished());
  EXPECT_FALSE(request2_.finished());

  WaitForBubbleToBeShown();
  Accept();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(dupe_request2->finished());
    EXPECT_TRUE(request2_.finished());
  } else {
    EXPECT_TRUE(dupe_request->finished());
    EXPECT_TRUE(request1_.finished());
  }

  WaitForBubbleToBeShown();
  Accept();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(dupe_request->finished());
    EXPECT_TRUE(request1_.finished());
  } else {
    EXPECT_TRUE(dupe_request2->finished());
    EXPECT_TRUE(request2_.finished());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Requests from iframes
////////////////////////////////////////////////////////////////////////////////

TEST_F(PermissionRequestManagerTest, MainFrameNoRequestIFrameRequest) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_same_domain_);
  WaitForBubbleToBeShown();
  WaitForFrameLoad();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_same_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_same_domain_.finished());
  }

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());

  Closing();
  EXPECT_FALSE(prompt_factory_->is_visible());
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_.finished());
  } else {
    EXPECT_TRUE(iframe_request_same_domain_.finished());
  }
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_other_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_.finished());
  } else {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
  }
}

TEST_F(PermissionRequestManagerTest, IFrameRequestWhenMainRequestVisible) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_same_domain_);
  WaitForFrameLoad();
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_same_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_same_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_.finished());
  } else {
    EXPECT_TRUE(iframe_request_same_domain_.finished());
  }
}

TEST_F(PermissionRequestManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_other_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_.finished());
  } else {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
  }
}

////////////////////////////////////////////////////////////////////////////////
// UMA logging
////////////////////////////////////////////////////////////////////////////////

// This code path (calling Accept on a non-merged bubble, with no accepted
// permission) would never be used in actual Chrome, but its still tested for
// completeness.
TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedBubbleAlternatePath) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptDenied,
                                static_cast<base::HistogramBase::Sample>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);
}

TEST_F(PermissionRequestManagerTest, UMAForTabSwitching) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptShown,
                                static_cast<base::HistogramBase::Sample>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);

  MockTabSwitchAway();
  MockTabSwitchBack();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptShown,
                                static_cast<base::HistogramBase::Sample>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);
}

////////////////////////////////////////////////////////////////////////////////
// UI selectors
////////////////////////////////////////////////////////////////////////////////

// Simulate a PermissionUiSelector that simply returns a predefined |ui_to_use|
// every time.
class MockNotificationPermissionUiSelector : public PermissionUiSelector {
 public:
  explicit MockNotificationPermissionUiSelector(
      std::optional<QuietUiReason> quiet_ui_reason,
      std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood,
      std::optional<base::TimeDelta> async_delay)
      : quiet_ui_reason_(quiet_ui_reason),
        prediction_likelihood_(prediction_likelihood),
        async_delay_(async_delay) {}

  void SelectUiToUse(PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    selected_ui_to_use_ = true;
    Decision decision(quiet_ui_reason_, Decision::ShowNoWarning());
    if (async_delay_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(std::move(callback), decision),
          async_delay_.value());
    } else {
      std::move(callback).Run(decision);
    }
  }

  bool IsPermissionRequestSupported(RequestType request_type) override {
    return request_type == RequestType::kNotifications ||
           request_type == RequestType::kGeolocation;
  }

  std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
  PredictedGrantLikelihoodForUKM() override {
    return prediction_likelihood_;
  }

  static void CreateForManager(
      PermissionRequestManager* manager,
      std::optional<QuietUiReason> quiet_ui_reason,
      std::optional<base::TimeDelta> async_delay,
      std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood = std::nullopt) {
    manager->add_permission_ui_selector_for_testing(
        std::make_unique<MockNotificationPermissionUiSelector>(
            quiet_ui_reason, prediction_likelihood, async_delay));
  }

  bool selected_ui_to_use() const { return selected_ui_to_use_; }

 private:
  std::optional<QuietUiReason> quiet_ui_reason_;
  std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
      prediction_likelihood_;
  std::optional<base::TimeDelta> async_delay_;
  bool selected_ui_to_use_ = false;
};

// Same as the MockNotificationPermissionUiSelector but handling only the
// Camera stream request type
class MockCameraStreamPermissionUiSelector
    : public MockNotificationPermissionUiSelector {
 public:
  explicit MockCameraStreamPermissionUiSelector(
      std::optional<QuietUiReason> quiet_ui_reason,
      std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood,
      std::optional<base::TimeDelta> async_delay)
      : MockNotificationPermissionUiSelector(quiet_ui_reason,
                                             prediction_likelihood,
                                             async_delay) {}

  bool IsPermissionRequestSupported(RequestType request_type) override {
    return request_type == RequestType::kCameraStream;
  }

  static void CreateForManager(
      PermissionRequestManager* manager,
      std::optional<QuietUiReason> quiet_ui_reason,
      std::optional<base::TimeDelta> async_delay,
      std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood = std::nullopt) {
    manager->add_permission_ui_selector_for_testing(
        std::make_unique<MockCameraStreamPermissionUiSelector>(
            quiet_ui_reason, prediction_likelihood, async_delay));
  }
};

TEST_F(PermissionRequestManagerTest,
       UiSelectorNotUsedForPermissionsOtherThanNotification) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, PermissionUiSelector::QuietUiReason::kEnabledInPrefs,
      std::nullopt /* async_delay */);

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(prompt_factory_->RequestTypeSeen(request_camera_.request_type()));
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  EXPECT_TRUE(request_camera_.granted());
}

TEST_F(PermissionRequestManagerTest, UiSelectorUsedForNotifications) {
  const struct {
    std::optional<PermissionUiSelector::QuietUiReason> quiet_ui_reason;
    std::optional<base::TimeDelta> async_delay;
  } kTests[] = {
      {QuietUiReason::kEnabledInPrefs, std::make_optional<base::TimeDelta>()},
      {PermissionUiSelector::Decision::UseNormalUi(),
       std::make_optional<base::TimeDelta>()},
      {QuietUiReason::kEnabledInPrefs, std::nullopt},
      {PermissionUiSelector::Decision::UseNormalUi(), std::nullopt},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    MockNotificationPermissionUiSelector::CreateForManager(
        manager_, test.quiet_ui_reason, test.async_delay);

    MockPermissionRequest request(RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.request_type()));
    EXPECT_EQ(!!test.quiet_ui_reason,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();

    EXPECT_TRUE(request.granted());
  }
}

TEST_F(PermissionRequestManagerTest,
       UiSelectionHappensSeparatelyForEachRequest) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs,
      std::make_optional<base::TimeDelta>());
  MockPermissionRequest request1(RequestType::kNotifications,
                                 PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  MockPermissionRequest request2(RequestType::kNotifications,
                                 PermissionRequestGestureType::GESTURE);
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, PermissionUiSelector::Decision::UseNormalUi(),
      std::make_optional<base::TimeDelta>());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

TEST_F(PermissionRequestManagerTest, SkipNextUiSelector) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs,
      /* async_delay */ std::nullopt);
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, PermissionUiSelector::Decision::UseNormalUi(),
      /* async_delay */ std::nullopt);
  MockPermissionRequest request1(RequestType::kNotifications,
                                 PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1);
  WaitForBubbleToBeShown();
  auto* next_selector =
      manager_->get_permission_ui_selectors_for_testing().back().get();
  EXPECT_FALSE(static_cast<MockNotificationPermissionUiSelector*>(next_selector)
                   ->selected_ui_to_use());
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

TEST_F(PermissionRequestManagerTest, MultipleUiSelectors) {
  const struct {
    std::vector<std::optional<QuietUiReason>> quiet_ui_reasons;
    std::vector<bool> simulate_delayed_decision;
    std::optional<QuietUiReason> expected_reason;
  } kTests[] = {
      // Simple sync selectors, first one should take priority.
      {{QuietUiReason::kTriggeredByCrowdDeny, QuietUiReason::kEnabledInPrefs},
       {false, false},
       QuietUiReason::kTriggeredByCrowdDeny},
      {{QuietUiReason::kTriggeredDueToDisruptiveBehavior,
        QuietUiReason::kEnabledInPrefs},
       {false, false},
       QuietUiReason::kTriggeredDueToDisruptiveBehavior},
      {{QuietUiReason::kTriggeredDueToDisruptiveBehavior,
        QuietUiReason::kServicePredictedVeryUnlikelyGrant},
       {false, false},
       QuietUiReason::kTriggeredDueToDisruptiveBehavior},
      {{QuietUiReason::kTriggeredDueToDisruptiveBehavior,
        QuietUiReason::kTriggeredByCrowdDeny},
       {false, false},
       QuietUiReason::kTriggeredDueToDisruptiveBehavior},
      // First selector is async but should still take priority even if it
      // returns later.
      {{QuietUiReason::kTriggeredByCrowdDeny, QuietUiReason::kEnabledInPrefs},
       {true, false},
       QuietUiReason::kTriggeredByCrowdDeny},
      {{QuietUiReason::kTriggeredDueToDisruptiveBehavior,
        QuietUiReason::kEnabledInPrefs},
       {true, false},
       QuietUiReason::kTriggeredDueToDisruptiveBehavior},
      // The first selector that has a quiet ui decision should be used.
      {{std::nullopt, std::nullopt,
        QuietUiReason::kTriggeredDueToAbusiveContent,
        QuietUiReason::kEnabledInPrefs},
       {false, true, true, false},
       QuietUiReason::kTriggeredDueToAbusiveContent},
      // If all selectors return a normal ui, it should use a normal ui.
      {{std::nullopt, std::nullopt}, {false, true}, std::nullopt},

      // Use a bunch of selectors both async and sync.
      {{std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        QuietUiReason::kTriggeredDueToAbusiveRequests, std::nullopt,
        QuietUiReason::kEnabledInPrefs},
       {false, true, false, true, true, true, false, false},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      // Use a bunch of selectors all sync.
      {{std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        QuietUiReason::kTriggeredDueToAbusiveRequests, std::nullopt,
        QuietUiReason::kEnabledInPrefs},
       {false, false, false, false, false, false, false, false},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      // Use a bunch of selectors all async.
      {{std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        QuietUiReason::kTriggeredDueToAbusiveRequests, std::nullopt,
        QuietUiReason::kEnabledInPrefs},
       {true, true, true, true, true, true, true, true},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      // Use a bunch of selectors both async and sync.
      {{std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        QuietUiReason::kTriggeredDueToDisruptiveBehavior, std::nullopt,
        QuietUiReason::kEnabledInPrefs},
       {true, false, false, true, true, true, false, false},
       QuietUiReason::kTriggeredDueToDisruptiveBehavior},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.quiet_ui_reasons.size(); ++i) {
      MockNotificationPermissionUiSelector::CreateForManager(
          manager_, test.quiet_ui_reasons[i],
          test.simulate_delayed_decision[i]
              ? std::make_optional<base::TimeDelta>()
              : std::nullopt);
    }

    MockPermissionRequest request(RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.request_type()));
    if (test.expected_reason.has_value()) {
      EXPECT_EQ(test.expected_reason, manager_->ReasonForUsingQuietUi());
    } else {
      EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    }

    Accept();
    EXPECT_TRUE(request.granted());
  }
}

TEST_F(PermissionRequestManagerTest, SelectorsPredictionLikelihood) {
  using PredictionLikelihood = PermissionUmaUtil::PredictionGrantLikelihood;
  const auto VeryLikely = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY;
  const auto Neutral = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL;

  const struct {
    std::vector<bool> enable_quiet_uis;
    std::vector<std::optional<PredictionLikelihood>> prediction_likelihoods;
    std::optional<PredictionLikelihood> expected_prediction_likelihood;
  } kTests[] = {
      // Sanity check: prediction likelihood is populated correctly.
      {{true}, {VeryLikely}, VeryLikely},
      {{false}, {Neutral}, Neutral},

      // Prediction likelihood is populated only if the selector was considered.
      {{true, true}, {std::nullopt, VeryLikely}, std::nullopt},
      {{false, true}, {std::nullopt, VeryLikely}, VeryLikely},
      {{false, false}, {std::nullopt, VeryLikely}, VeryLikely},

      // First considered selector is preserved.
      {{true, true}, {Neutral, VeryLikely}, Neutral},
      {{false, true}, {Neutral, VeryLikely}, Neutral},
      {{false, false}, {Neutral, VeryLikely}, Neutral},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.enable_quiet_uis.size(); ++i) {
      MockNotificationPermissionUiSelector::CreateForManager(
          manager_,
          test.enable_quiet_uis[i]
              ? std::optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)
              : std::nullopt,
          std::nullopt /* async_delay */, test.prediction_likelihoods[i]);
    }

    MockPermissionRequest request(RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.request_type()));
    EXPECT_EQ(test.expected_prediction_likelihood,
              manager_->prediction_grant_likelihood_for_testing());

    Accept();
    EXPECT_TRUE(request.granted());
  }
}

TEST_F(PermissionRequestManagerTest, SelectorRequestTypes) {
  const struct {
    RequestType request_type;
    bool should_request_use_quiet_ui;
  } kTests[] = {
      {RequestType::kNotifications, true},
      {RequestType::kGeolocation, true},
      {RequestType::kCameraStream, false},
  };
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs,
      std::make_optional<base::TimeDelta>());
  for (const auto& test : kTests) {
    MockPermissionRequest request(test.request_type,
                                  PermissionRequestGestureType::GESTURE);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
    WaitForBubbleToBeShown();
    EXPECT_EQ(test.should_request_use_quiet_ui,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();
  }
  // Adding a mock PermissionUiSelector that handles Camera stream.
  MockCameraStreamPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs,
      std::make_optional<base::TimeDelta>());
  // Now the RequestType::kCameraStream should show a quiet UI as well
  MockPermissionRequest request2(RequestType::kCameraStream,
                                 PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

////////////////////////////////////////////////////////////////////////////////
// Quiet UI chip. Low priority for Notifications & Geolocation.
////////////////////////////////////////////////////////////////////////////////

TEST_F(PermissionRequestManagerTest, NotificationsSingleBubbleAndChipRequest) {
  MockPermissionRequest request(RequestType::kNotifications,
                                PermissionRequestGestureType::GESTURE);

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request.granted());
  EXPECT_EQ(prompt_factory_->show_count(), 1);
}

// Android is the only platform that does not support the permission chip.
#if BUILDFLAG(IS_ANDROID)
// Quiet UI feature is disabled. Chip is disabled. No low priority requests, the
// first request is always shown.
//
// Permissions requested in order:
// 1. Notification (non abusive)
// 2. Geolocation
// 3. Camera
//
// Prompt display order:
// 1. Notification request shown
// 2. Geolocation request shown
// 3. Camera request shown
TEST_F(PermissionRequestManagerTest,
       NotificationsGeolocationCameraBubbleRequest) {
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/false, 1);

  for (auto* kRequest : {request_notifications.get(), request_geolocation.get(),
                         request_camera.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 3);
}

// Tests for non-Android platforms which support the permission chip.
#else  // BUILDFLAG(IS_ANDROID)
// Quiet UI feature is disabled, no low priority requests, the last request is
// always shown.
//
// Permissions requested in order:
// 1. Camera
// 2. Clipboard
// 3. MIDI
//
// Prompt display order:
// 1. Camera request shown but is preempted
// 2. Clipboard request shown but is preempted
// 3. MIDI request shown
// 4. Clipboard request shown again
// 5. Camera request shown again
TEST_F(PermissionRequestManagerTest,
       CameraNotificationsGeolocationChipRequest) {
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);
  std::unique_ptr<MockPermissionRequest> request_clipboard =
      CreateAndAddRequest(RequestType::kClipboard, /*should_be_seen=*/true, 2);
  std::unique_ptr<MockPermissionRequest> request_midi =
      CreateAndAddRequest(RequestType::kMidiSysex, /*should_be_seen=*/true, 3);

  for (auto* kRequest :
       {request_midi.get(), request_clipboard.get(), request_camera.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

// Verifies order of simultaneous requests, with quiet chip enabled.
// Simultaneous new requests are coming while we are waiting for UI selector
// decisions.
//
// Permissions requested in order:
// 1. Geolocation, UI selector takes 2 seconds to decide.
// 2. Notification then mic. Notification will preempt geolocation
//
// Prompt display order:
// 1. Mic
// 2. Clipboard
// 3. Geolocation
TEST_F(PermissionRequestManagerTest, NewHighPriorityRequestDuringUIDecision) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kTriggeredDueToAbusiveRequests,
      std::make_optional<base::TimeDelta>(base::Seconds(2)));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);

  task_environment()->FastForwardBy(base::Seconds(1));

  MockPermissionRequest request(RequestType::kClipboard,
                                PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();
  manager_->clear_permission_ui_selector_for_testing();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Verifies that the quiet UI chip is not ignored if another request came in
// less than 8.5 seconds after.
// Permissions requested in order:
// 1. Notification (abusive)
// 2. After less than 8.5 seconds Geolocation
//
// Prompt display order:
// 1. Notifications request shown but is preempted because of quiet UI.
// 2. Geolocation request shown
// 3. Notifications request shown again
TEST_F(PermissionRequestManagerTest,
       AbusiveNotificationsGeolocationQuietUIChipRequest) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      std::nullopt /* async_delay */);

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);

  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          2);

  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

  EXPECT_EQ(prompt_factory_->show_count(), 3);
}

// Verifies that the quiet UI chip is ignored if another request came in more
// than 8.5 seconds after.
//
// Permissions requested in order:
// 1. Notification (abusive)
// 2. After more than 8.5 seconds Geolocation
//
// Prompt display order:
// 1. Notifications request shown but is preempted because of quiet UI.
// 2. Geolocation request shown
TEST_F(PermissionRequestManagerTest, AbusiveNotificationsShownLongEnough) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      std::nullopt /* async_delay */);

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);

  // More then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(9000));

  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          2);

  // The second permission was requested after 8.5 second window, the quiet UI
  // Notifiations request for an abusive origin is automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_TRUE(request_notifications->finished());

  WaitAndAcceptPromptForRequest(request_geolocation.get());

  EXPECT_EQ(prompt_factory_->show_count(), 2);
}

// Verifies that the quiet UI chip is not ignored if another request came in
// more than 8.5 seconds after. Verify different requests priority. Camera
// request is shown despite being requested last.
//
// Permissions requested in order:
// 1. Notification (abusive)
// 2. After less than 8.5 seconds Geolocation
// 3. Camera
//
// Prompt display order:
// 1. Notifications request shown but is preempted because of quiet UI.
// 2. Geolocation request shown but is preempted because of low priority.
// 3. Camera request shown
// 4. Geolocation request shown again
// 5. Notifications quiet UI request shown again
TEST_F(PermissionRequestManagerTest,
       AbusiveNotificationsShownLongEnoughCamera) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      std::nullopt /* async_delay */);

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          2);
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 3);

  // The second permission was requested in 8.5 second window, the quiet UI
  // Notifiations request for an abusive origin is not automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  for (auto* kRequest : {request_camera.get(), request_geolocation.get(),
                         request_notifications.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

// Verifies that the quiet UI chip is not ignored if another request came in
// more than 8.5 seconds after. Verify different requests priority. Camera
// request is not preemted.
//
// Permissions requested in order:
// 1. Camera
// 2. Notification (abusive)
// 3. After less than 8.5 seconds Geolocation
//
// Prompt display order:
// 1. Camera request shown
// 2. Geolocation request shown
// 3. Camera request shown
TEST_F(PermissionRequestManagerTest, CameraAbusiveNotificationsGeolocation) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      std::nullopt /* async_delay */);

  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/false,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  for (auto* kRequest : {request_camera.get(), request_geolocation.get(),
                         request_notifications.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 3);
}

// Verifies that the quiet UI chip is not ignored if another request came in
// more than 8.5 seconds after. Verify different requests priority. Camera
// request is not preemted.
//
// Permissions requested in order:
// 1. Camera
// 2. Notification (abusive)
// 3. After less than 8.5 seconds Geolocation
// 4. MIDI
//
// Prompt display order:
// 1. Camera request shown
// 2. MIDI request shown (or MIDI and then Camera, the order depends on
// `PermissionUtil::DoesPlatformSupportChip()`)
// 3. Geolocation request shown
// 4. Notifications request shown
// If Chip is enabled MIDI will replace Camera, hence 5 prompts will be
// shown. Otherwise 4.
TEST_F(PermissionRequestManagerTest,
       CameraAbusiveNotificationsGeolocationMIDI) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      std::nullopt /* async_delay */);

  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/false,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);

  std::unique_ptr<MockPermissionRequest> request_midi;

  // Since the chip is enabled, MIDI should be shown.
  request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                     /*should_be_seen=*/true, 2);

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  WaitAndAcceptPromptForRequest(request_midi.get());
  WaitAndAcceptPromptForRequest(request_camera.get());
  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

// Verifies that non abusive chip behaves similar to others when Quiet UI Chip
// is enabled.
//
// Permissions requested in order:
// 1. Camera
// 2. Notification (non abusive)
// 3. After less than 8.5 seconds Geolocation
// 4. MIDI
//
// Prompt display order:
// 1. Camera request shown
// 2. MIDI request shown (or MIDI and then Camera, the order depends on
// `PermissionUtil::DoesPlatformSupportChip()`)
// 3. Geolocation request shown
// 4. Notifications request shown
// If Chip is enabled MIDI will replace Camera, hence 5 prompts will be
// shown. Otherwise 4.
TEST_F(PermissionRequestManagerTest,
       CameraNonAbusiveNotificationsGeolocationMIDI) {
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/false,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);

  std::unique_ptr<MockPermissionRequest> request_midi;

  // If Chip is enabled, MIDI should be shown, otherwise MIDI should not be
  // shown.
    request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                       /*should_be_seen=*/true, 2);

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  WaitAndAcceptPromptForRequest(request_midi.get());
  WaitAndAcceptPromptForRequest(request_camera.get());
  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

// Verifies that a high-priority request cannot preempt a low-priority request
// if the high-priority request comes in as the result of a permission prompt
// being accepted.
// Permissions requested in order:
// 1. Gelocation (low)
// 2. Mic (high)
TEST_F(PermissionRequestManagerTest, ReentrantPermissionRequestAccept) {
  request1_.RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                             &request_mic_);
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request_mic_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
}

// Verifies that a high-priority request cannot preempt a low-priority request
// if the high-priority request comes in as the result of a permission prompt
// being accepted once.
// Permissions requested in order:
// 1. Gelocation (low)
// 2. Mic (high)
TEST_F(PermissionRequestManagerTest, ReentrantPermissionRequestAcceptOnce) {
  request1_.RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                             &request_mic_);
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  AcceptThisTime();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(request_mic_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
}

// Verifies that a high-priority request cannot preempt a low-priority request
// if the high-priority request comes in as the result of a permission prompt
// being denied.
// Permissions requested in order:
// 1. Gelocation (low)
// 2. Mic (high)
TEST_F(PermissionRequestManagerTest, ReentrantPermissionRequestDeny) {
  request1_.RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                             &request_mic_);
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Deny();
  EXPECT_FALSE(request1_.granted());
  EXPECT_FALSE(request_mic_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request1_.granted());
  EXPECT_TRUE(request_mic_.granted());
}

// Verifies that a high-priority request cannot preempt a low-priority request
// if the high-priority request comes in as the result of a permission prompt
// being dismissed.
// Permissions requested in order:
// 1. Gelocation (low)
// 2. Mic (high)
TEST_F(PermissionRequestManagerTest, ReentrantPermissionRequestCancelled) {
  request1_.RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                             &request_mic_);
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(request1_.cancelled());
  EXPECT_FALSE(request_mic_.cancelled());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request1_.granted());
  EXPECT_TRUE(request_mic_.granted());
}

// Verifies order of requests with mixed low-high priority requests input, with
// both chip and quiet chip enabled. New permissions are added and accepted one
// by one.
//
// Permissions requested in order:
// 1. Multiple Download (high)
// 2. Geolocation (low)
// 3. Mic (high)
//
// Prompt display order:
// 1. Mic
// 2. Multiple Download
// 3. Geolocation
TEST_F(PermissionRequestManagerTest, Mixed1Low2HighPriorityRequests) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Verifies order of requests with mixed low-high priority requests input, with
// both chip and quiet chip enabled. New permissions are added and accepted one
// by one.
//
// Permissions requested in order:
// 1. Geolocation (low)
// 2. Mic (high)
// 3. Notification (low)
//
// Prompt display order:
// 1. Mic
// 2. Notification
// 3. Geolocation
TEST_F(PermissionRequestManagerTest, Mixed2Low1HighRequests) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  MockPermissionRequest request(RequestType::kNotifications,
                                PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Verifies order of requests with mixed low-high priority requests input, added
// simultaneously, with both chip and quiet chip enabled.
//
// Permissions requested in order:
// 1. Geolocation (low)
// 2. Mic (high)
// 3. Notification (low)
//
// Prompt display order:
// 1. Mic
// 2. Notification
// 3. Geolocation
TEST_F(PermissionRequestManagerTest, MultipleSimultaneous2Low1HighRequests) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  MockPermissionRequest request(RequestType::kNotifications,
                                PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Verifies order of requests with mixed low-high priority requests input,
// added simultaneously, with both chip and quiet chip enabled.
//
// Permissions requested in order:
// 1. MIDI (high)
// 2. Geolocation (low)
// 3. Mic (high)
// 4. Notification (low)
// 5. Multiple Download (high)
//
// Prompt display order:
// 1. Multiple Download
// 2. Mic
// 3. Midi
// 4. Notification
// 5. Geolocation
TEST_F(PermissionRequestManagerTest, MultipleSimultaneous2Low3HighRequests) {
  MockPermissionRequest request_midi(RequestType::kMidiSysex,
                                     PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_midi);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  MockPermissionRequest request_notification(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &request_notification);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request_mic_.granted());
  EXPECT_TRUE(request2_.granted());
  EXPECT_FALSE(request_notification.granted());
  EXPECT_FALSE(request1_.granted());
  EXPECT_FALSE(request_midi.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request_notification.granted());
  EXPECT_FALSE(request1_.granted());
  EXPECT_FALSE(request_midi.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request_notification.granted());
  EXPECT_FALSE(request1_.granted());
  EXPECT_TRUE(request_midi.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_notification.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Verifies order of requests with mixed low-high priority requests input, added
// simultaneously several times, with both chip and quiet chip enabled.
//
// Permissions requested in order:
// 1. Geolocation(low) then Notification(low)
// 2. Mic (high) then multiple downloads (high)
// Prompt display order:
// 1. Multiple Download
// 2. Mic
// 3. Notification
// 4. Geolocation
TEST_F(PermissionRequestManagerTest, MultipleSimultaneous2Low2HighRequests) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  MockPermissionRequest request(RequestType::kNotifications,
                                PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
  EXPECT_FALSE(request_mic_.granted());
  EXPECT_FALSE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_F(PermissionRequestManagerTest, PEPCRequestNeverQuiet) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, PermissionUiSelector::QuietUiReason::kEnabledInPrefs,
      std::nullopt /* async_delay */);

  // PEPC request is not quieted by selector.
  MockPermissionRequest pepc_request(
      RequestType::kNotifications,
      /*embedded_permission_element_initiated=*/true);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &pepc_request);
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(prompt_factory_->RequestTypeSeen(pepc_request.request_type()));
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  // Regular request is quieted by selector.
  MockPermissionRequest request(RequestType::kNotifications,
                                PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(prompt_factory_->RequestTypeSeen(request.request_type()));
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace permissions
