// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "url/gurl.h"

namespace permissions {

namespace {

using QuietUiReason = PermissionUiSelector::QuietUiReason;
using Decision = PermissionUiSelector::Decision;

using testing::SizeIs;

constexpr int kPermissionTypeGeolocationWithOptions = 139;

}  // namespace

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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
        request_ptz_(RequestType::kCameraPanTiltZoom,
                     PermissionRequestGestureType::NO_GESTURE),
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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

    // This is needed to make sure prefs initialized in PermissionsUmaUtil
    // class.
    user_prefs::UserPrefs::Set(browser_context(), &prefs_);
    prefs_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        false);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    manager_ = nullptr;
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

  void Ignore() {
    manager_->Ignore();
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

  std::unique_ptr<MockPermissionRequest::MockPermissionRequestState>
  CreateAndAddRequest(RequestType type,
                      bool should_be_seen,
                      int expected_request_count) {
    auto request_state =
        std::make_unique<MockPermissionRequest::MockPermissionRequestState>();
    std::unique_ptr<MockPermissionRequest> request =
        std::make_unique<MockPermissionRequest>(
            type, PermissionRequestGestureType::GESTURE,
            request_state->GetWeakPtr());
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    if (should_be_seen) {
      EXPECT_TRUE(prompt_factory_->RequestTypeSeen(type));
    } else {
      EXPECT_FALSE(prompt_factory_->RequestTypeSeen(type));
    }
    EXPECT_EQ(prompt_factory_->TotalRequestCount(), expected_request_count);

    return request_state;
  }

  void WaitAndAcceptPromptForRequest(
      MockPermissionRequest::MockPermissionRequestState* request_state) {
    WaitForBubbleToBeShown();

    EXPECT_FALSE(request_state->finished);
    EXPECT_TRUE(prompt_factory_->is_visible());
    ASSERT_EQ(prompt_factory_->request_count(), 1);

    Accept();
    EXPECT_TRUE(request_state->granted);
  }

 protected:
  std::unique_ptr<MockPermissionRequest> CreateRequest(
      std::pair<RequestType, PermissionRequestGestureType> request_params,
      base::WeakPtr<MockPermissionRequest::MockPermissionRequestState> state =
          nullptr) {
    return std::make_unique<permissions::MockPermissionRequest>(
        request_params.first, request_params.second, state);
  }

  std::unique_ptr<permissions::MockPermissionRequest> CreateRequest(
      std::pair<GURL, RequestType> request_params,
      base::WeakPtr<MockPermissionRequest::MockPermissionRequestState> state =
          nullptr) {
    return std::make_unique<permissions::MockPermissionRequest>(
        request_params.first, request_params.second, state);
  }

  std::pair<RequestType, PermissionRequestGestureType> request1_;
  std::pair<RequestType, PermissionRequestGestureType> request2_;
  std::pair<RequestType, PermissionRequestGestureType> request_mic_;
  std::pair<RequestType, PermissionRequestGestureType> request_camera_;
#if !BUILDFLAG(IS_ANDROID)
  std::pair<RequestType, PermissionRequestGestureType> request_ptz_;
#endif

  std::pair<GURL, RequestType> iframe_request_same_domain_;
  std::pair<GURL, RequestType> iframe_request_other_domain_;
  std::pair<GURL, RequestType> iframe_request_camera_other_domain_;
  std::pair<GURL, RequestType> iframe_request_mic_other_domain_;

  raw_ptr<PermissionRequestManager> manager_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  TestPermissionsClient client_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
};

////////////////////////////////////////////////////////////////////////////////
// General
////////////////////////////////////////////////////////////////////////////////

TEST_F(PermissionRequestManagerTest, NoRequests) {
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, SingleRequest) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_state.granted);
}

TEST_F(PermissionRequestManagerTest, SequentialRequests) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_state.granted);
  EXPECT_FALSE(prompt_factory_->is_visible());

  MockPermissionRequest::MockPermissionRequestState request2_state;
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  Accept();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request2_state.granted);
}

TEST_F(PermissionRequestManagerTest, ForgetRequestsOnPageNavigation) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  MockPermissionRequest::MockPermissionRequestState
      iframe_request_other_domain_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(iframe_request_other_domain_,
                    iframe_request_other_domain_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForBubbleToBeShown();

  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request1_state.finished);
  EXPECT_TRUE(request2_state.finished);
  EXPECT_TRUE(iframe_request_other_domain_state.finished);
}

TEST_F(PermissionRequestManagerTest, RequestsDontNeedUserGesture) {
  WaitForFrameLoad();
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(iframe_request_other_domain_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_));
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_F(PermissionRequestManagerTest, RequestsNotSupported) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();
  Accept();
  EXPECT_TRUE(request1_state.granted);

  manager_->set_web_contents_supports_permission_requests(false);

  MockPermissionRequest::MockPermissionRequestState request2_state;
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  EXPECT_TRUE(request2_state.cancelled);
}

////////////////////////////////////////////////////////////////////////////////
// Requests grouping
////////////////////////////////////////////////////////////////////////////////

// Android is the only platform that does not support the permission chip.
#if BUILDFLAG(IS_ANDROID)
// Most requests should never be grouped.
// Grouping for chip feature is tested in ThreeRequestsStackOrderChip.
TEST_F(PermissionRequestManagerTest, TwoRequestsUngrouped) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_state.granted);

  ASSERT_EQ(prompt_factory_->show_count(), 2);
}

// Tests for non-Android platforms which support the permission chip.
#else   // BUILDFLAG(IS_ANDROID)
TEST_F(PermissionRequestManagerTest, ThreeRequestsStackOrderChip) {
  // Test new permissions order, requests shouldn't be grouped.
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request2_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
}

// Test new permissions order by adding requests one at a time.
TEST_F(PermissionRequestManagerTest, ThreeRequestsOneByOneStackOrderChip) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request2_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Only mic/camera requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, MicCameraGrouped) {
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  MockPermissionRequest::MockPermissionRequestState request_camera_state;

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_camera_, request_camera_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_TRUE(request_camera_state.granted);
}

// If mic/camera requests come from different origins, they should not be
// grouped.
TEST_F(PermissionRequestManagerTest, MicCameraDifferentOrigins) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(iframe_request_mic_other_domain_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request_camera_));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

#if !BUILDFLAG(IS_ANDROID)
// Only camera/ptz requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, CameraPtzGrouped) {
  MockPermissionRequest::MockPermissionRequestState request_camera_state;
  MockPermissionRequest::MockPermissionRequestState request_ptz_state;

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_camera_, request_camera_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_ptz_, request_ptz_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_camera_state.granted);
  EXPECT_TRUE(request_ptz_state.granted);
}

TEST_F(PermissionRequestManagerTest, CameraPtzDifferentOrigins) {
  // If camera/ptz requests come from different origins, they should not be
  // grouped.
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(iframe_request_camera_other_domain_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request_ptz_));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

// Only mic/camera/ptz requests from the same origin should be grouped.
TEST_F(PermissionRequestManagerTest, MicCameraPtzGrouped) {
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  MockPermissionRequest::MockPermissionRequestState request_camera_state;
  MockPermissionRequest::MockPermissionRequestState request_ptz_state;

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_camera_, request_camera_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_ptz_, request_ptz_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 3);

  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_TRUE(request_camera_state.granted);
  EXPECT_TRUE(request_ptz_state.granted);
}

// If mic/camera/ptz requests come from different origins, they should not be
// grouped.
TEST_F(PermissionRequestManagerTest, MicCameraPtzDifferentOrigins) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(iframe_request_mic_other_domain_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request_camera_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request_ptz_));
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
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request_camera_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request_mic_));
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
                       CreateRequest(iframe_request_camera_other_domain_));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  OpenHelpCenterLink();
  SUCCEED();
}

TEST_F(PermissionRequestManagerTest, OpenHelpCenterLink_RequestNotSupported) {
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  EXPECT_DEATH_IF_SUPPORTED(OpenHelpCenterLink(), "");
}

////////////////////////////////////////////////////////////////////////////////
// Tab switching
////////////////////////////////////////////////////////////////////////////////

#if BUILDFLAG(IS_ANDROID)
TEST_F(PermissionRequestManagerTest, TwoRequestsTabSwitch) {
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  MockPermissionRequest::MockPermissionRequestState request_camera_state;

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_camera_, request_camera_state.GetWeakPtr()));
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
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_TRUE(request_camera_state.granted);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PermissionRequestManagerTest, PermissionRequestWhileTabSwitchedAway) {
  MockTabSwitchAway();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_));
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
  auto request1_state = CreateAndAddRequest(request1_.first, true, 1);
  auto dupe_request1_state = CreateAndAddRequest(request1_.first, true, 1);

  EXPECT_FALSE(request1_state->finished);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(request1_state->granted);
  EXPECT_FALSE(prompt_factory_->is_visible());
}

class QuicklyDeletedRequest : public PermissionRequest {
 public:
  QuicklyDeletedRequest(const GURL& requesting_origin,
                        RequestType request_type,
                        PermissionRequestGestureType gesture_type)
      : PermissionRequest(
            std::make_unique<PermissionRequestData>(
                std::make_unique<ContentSettingPermissionResolver>(
                    request_type),
                /*user_gesture=*/gesture_type ==
                    PermissionRequestGestureType::GESTURE,
                requesting_origin),
            base::BindLambdaForTesting(
                [](PermissionDecision decision,
                   bool is_final_decision,
                   const PermissionRequestData&) { NOTREACHED(); })) {}

  static std::unique_ptr<QuicklyDeletedRequest> CreateRequest(
      MockPermissionRequest* request) {
    return std::make_unique<QuicklyDeletedRequest>(request->requesting_origin(),
                                                   request->request_type(),
                                                   request->GetGestureType());
  }
};

TEST_F(PermissionRequestManagerTest, DuplicateRequest) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request1_dupe_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  MockPermissionRequest::MockPermissionRequestState request2_dupe_state;

  auto request1 = std::make_unique<MockPermissionRequest>(
      request1_.first, PermissionRequestGestureType::GESTURE,
      request1_state.GetWeakPtr());
  auto request1_dupe =
      request1->CreateDuplicateRequest(request1_dupe_state.GetWeakPtr());
  auto request2 = std::make_unique<MockPermissionRequest>(
      request2_.first, PermissionRequestGestureType::GESTURE,
      request2_state.GetWeakPtr());
  auto request2_dupe =
      request2->CreateDuplicateRequest(request2_dupe_state.GetWeakPtr());

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1));
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request2));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1_dupe));
  EXPECT_FALSE(request1_dupe_state.finished);
  EXPECT_FALSE(request1_state.finished);

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request2_dupe));
  EXPECT_FALSE(request2_dupe_state.finished);
  EXPECT_FALSE(request2_state.finished);

  WaitForBubbleToBeShown();
  Accept();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request2_dupe_state.finished);
    EXPECT_TRUE(request2_state.finished);
  } else {
    EXPECT_TRUE(request1_dupe_state.finished);
    EXPECT_TRUE(request1_state.finished);
  }

  WaitForBubbleToBeShown();
  Accept();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_dupe_state.finished);
    EXPECT_TRUE(request1_state.finished);
  } else {
    EXPECT_TRUE(request2_dupe_state.finished);
    EXPECT_TRUE(request2_state.finished);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Requests from iframes
////////////////////////////////////////////////////////////////////////////////

TEST_F(PermissionRequestManagerTest, MainFrameNoRequestIFrameRequest) {
  MockPermissionRequest::MockPermissionRequestState
      iframe_request_same_domain_state;

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(iframe_request_same_domain_,
                    iframe_request_same_domain_state.GetWeakPtr()));
  WaitForBubbleToBeShown();
  WaitForFrameLoad();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_state.finished);
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestSameDomain) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState
      iframe_request_same_domain_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(iframe_request_same_domain_,
                    iframe_request_same_domain_state.GetWeakPtr()));
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_same_domain_state.finished);
    EXPECT_FALSE(request1_state.finished);
  } else {
    EXPECT_TRUE(request1_state.finished);
    EXPECT_FALSE(iframe_request_same_domain_state.finished);
  }

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());

  Closing();
  EXPECT_FALSE(prompt_factory_->is_visible());
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_state.finished);
  } else {
    EXPECT_TRUE(iframe_request_same_domain_state.finished);
  }
}

TEST_F(PermissionRequestManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState
      iframe_request_other_domain_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(iframe_request_other_domain_,
                    iframe_request_other_domain_state.GetWeakPtr()));
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_other_domain_state.finished);
    EXPECT_FALSE(request1_state.finished);
  } else {
    EXPECT_TRUE(request1_state.finished);
    EXPECT_FALSE(iframe_request_other_domain_state.finished);
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_state.finished);
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_state.finished);
  } else {
    EXPECT_TRUE(iframe_request_other_domain_state.finished);
  }
}

TEST_F(PermissionRequestManagerTest, IFrameRequestWhenMainRequestVisible) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState
      iframe_request_other_domain_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(iframe_request_same_domain_,
                    iframe_request_other_domain_state.GetWeakPtr()));
  WaitForFrameLoad();
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_other_domain_state.finished);
    EXPECT_FALSE(request1_state.finished);
  } else {
    EXPECT_TRUE(request1_state.finished);
    EXPECT_FALSE(iframe_request_other_domain_state.finished);
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_state.finished);
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_state.finished);
  } else {
    EXPECT_TRUE(iframe_request_other_domain_state.finished);
  }
}

TEST_F(PermissionRequestManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState
      iframe_request_other_domain_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(iframe_request_other_domain_,
                    iframe_request_other_domain_state.GetWeakPtr()));
  WaitForFrameLoad();
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(iframe_request_other_domain_state.finished);
    EXPECT_FALSE(request1_state.finished);
  } else {
    EXPECT_TRUE(request1_state.finished);
    EXPECT_FALSE(iframe_request_other_domain_state.finished);
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_TRUE(request1_state.finished);
  } else {
    EXPECT_TRUE(iframe_request_other_domain_state.finished);
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

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_));
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptDenied,
                                static_cast<base::HistogramBase::Sample32>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);
}

TEST_F(PermissionRequestManagerTest, UMAForTabSwitching) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_));
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptShown,
                                static_cast<base::HistogramBase::Sample32>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);

  MockTabSwitchAway();
  MockTabSwitchBack();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptShown,
                                static_cast<base::HistogramBase::Sample32>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);
}

////////////////////////////////////////////////////////////////////////////////
// UI selectors
////////////////////////////////////////////////////////////////////////////////

// Simulate a PermissionUiSelector that simply returns a predefined |ui_to_use|
// every time.
class MockNotificationGeolocationPermissionUiSelector
    : public PermissionUiSelector {
 public:
  explicit MockNotificationGeolocationPermissionUiSelector(
      const Decision& decision,
      std::optional<PermissionUiSelector::PredictionGrantLikelihood>
          prediction_likelihood,
      std::optional<base::TimeDelta> async_delay)
      : decision_(decision),
        prediction_likelihood_(prediction_likelihood),
        async_delay_(async_delay) {}

  void SelectUiToUse(content::WebContents* web_contents,
                     PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    selected_ui_to_use_ = true;
    if (async_delay_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(std::move(callback), decision_),
          async_delay_.value());
    } else {
      std::move(callback).Run(decision_);
    }
  }

  bool IsPermissionRequestSupported(RequestType request_type) override {
    return request_type == RequestType::kNotifications ||
           request_type == RequestType::kGeolocation;
  }

  std::optional<PermissionUiSelector::PredictionGrantLikelihood>
  PredictedGrantLikelihoodForUKM() override {
    return prediction_likelihood_;
  }

  static void CreateForManager(
      PermissionRequestManager* manager,
      const Decision& decision,
      std::optional<base::TimeDelta> async_delay,
      std::optional<PermissionUiSelector::PredictionGrantLikelihood>
          prediction_likelihood = std::nullopt) {
    manager->add_permission_ui_selector_for_testing(
        std::make_unique<MockNotificationGeolocationPermissionUiSelector>(
            decision, prediction_likelihood, async_delay));
  }

  bool selected_ui_to_use() const { return selected_ui_to_use_; }

 private:
  Decision decision_;
  std::optional<PermissionUiSelector::PredictionGrantLikelihood>
      prediction_likelihood_;
  std::optional<base::TimeDelta> async_delay_;
  bool selected_ui_to_use_ = false;
};

// Same as the MockNotificationGeolocationPermissionUiSelector but handling only
// the Camera stream request type
class MockCameraStreamPermissionUiSelector
    : public MockNotificationGeolocationPermissionUiSelector {
 public:
  explicit MockCameraStreamPermissionUiSelector(
      const Decision& decision,
      std::optional<PermissionUiSelector::PredictionGrantLikelihood>
          prediction_likelihood,
      std::optional<base::TimeDelta> async_delay)
      : MockNotificationGeolocationPermissionUiSelector(decision,
                                                        prediction_likelihood,
                                                        async_delay) {}

  bool IsPermissionRequestSupported(RequestType request_type) override {
    return request_type == RequestType::kCameraStream;
  }

  static void CreateForManager(
      PermissionRequestManager* manager,
      const Decision& decision,
      std::optional<base::TimeDelta> async_delay,
      std::optional<PermissionUiSelector::PredictionGrantLikelihood>
          prediction_likelihood = std::nullopt) {
    manager->add_permission_ui_selector_for_testing(
        std::make_unique<MockCameraStreamPermissionUiSelector>(
            decision, prediction_likelihood, async_delay));
  }
};

TEST_F(PermissionRequestManagerTest,
       UiSelectorNotUsedForPermissionsOtherThanNotification) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(PermissionUiSelector::QuietUiReason::kEnabledInPrefs,
                           Decision::ShowNoWarning()),
      std::nullopt /* async_delay */);

  MockPermissionRequest::MockPermissionRequestState request_camera_state;
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_camera_, request_camera_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(
      prompt_factory_->RequestTypeSeen(request_camera_state.request_type));
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  EXPECT_TRUE(request_camera_state.granted);
}

TEST_F(PermissionRequestManagerTest, UiSelectorUsedForNotifications) {
  const struct {
    Decision decision;
    std::optional<base::TimeDelta> async_delay;
  } kTests[] = {
      {Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                            Decision::ShowNoWarning()),
       std::make_optional<base::TimeDelta>()},
      {Decision::UseNormalUiAndShowNoWarning(),
       std::make_optional<base::TimeDelta>()},
      {Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                            Decision::ShowNoWarning()),
       std::nullopt},
      {Decision::UseNormalUiAndShowNoWarning(), std::nullopt},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    MockNotificationGeolocationPermissionUiSelector::CreateForManager(
        manager_, test.decision, test.async_delay);

    MockPermissionRequest::MockPermissionRequestState request_state;
    auto request = std::make_unique<MockPermissionRequest>(
        RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
        request_state.GetWeakPtr());

    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request_state.request_type));
    EXPECT_EQ(test.decision.quiet_ui_reason.has_value(),
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();

    EXPECT_TRUE(request_state.granted);
  }
}

TEST_F(PermissionRequestManagerTest, UiSelectorUsedForGeolocation) {
  base::test::ScopedFeatureList
      enable_permission_predictions_geolocation_accuracy(
          features::kPermissionPredictionsGeolocationAccuracy);

  const struct {
    Decision decision;
    std::optional<base::TimeDelta> async_delay;
    GeolocationAccuracy expected_accuracy;
  } kTests[] = {
      {
          Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                               Decision::ShowNoWarning()),
          std::make_optional<base::TimeDelta>(),
          GeolocationAccuracy::kPrecise,
      },
      {
          Decision::UseNormalUiAndShowNoWarning(),
          std::make_optional<base::TimeDelta>(),
          GeolocationAccuracy::kPrecise,
      },
      {
          Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                               Decision::ShowNoWarning()),
          std::nullopt,
          GeolocationAccuracy::kPrecise,
      },
      {
          Decision::UseNormalUiAndShowNoWarning(),
          std::nullopt,
          GeolocationAccuracy::kPrecise,
      },
      {
          Decision::UseNormalUi(
              Decision::ShowNoWarning(),
              PermissionUiSelector::GeolocationAccuracy::kApproximate),
          std::nullopt,
          GeolocationAccuracy::kApproximate,
      },
      {
          Decision::UseNormalUi(
              Decision::ShowNoWarning(),
              PermissionUiSelector::GeolocationAccuracy::kPrecise),
          std::nullopt,
          GeolocationAccuracy::kPrecise,
      },
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    MockNotificationGeolocationPermissionUiSelector::CreateForManager(
        manager_, test.decision, test.async_delay);

    MockPermissionRequest::MockPermissionRequestState request_state;
    auto request = std::make_unique<MockPermissionRequest>(
        RequestType::kGeolocation, PermissionRequestGestureType::GESTURE,
        request_state.GetWeakPtr());

    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request_state.request_type));
    EXPECT_EQ(test.decision.quiet_ui_reason.has_value(),
              manager_->ShouldCurrentRequestUseQuietUI());
    EXPECT_EQ(test.expected_accuracy,
              manager_->GetInitialGeolocationAccuracySelection());
    Accept();

    EXPECT_TRUE(request_state.granted);
  }
}

TEST_F(PermissionRequestManagerTest,
       UiSelectionHappensSeparatelyForEachRequest) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                           Decision::ShowNoWarning()),
      std::make_optional<base::TimeDelta>());
  auto request1 = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  auto request2 = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE);
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_, Decision::UseNormalUiAndShowNoWarning(),
      std::make_optional<base::TimeDelta>());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request2));
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

TEST_F(PermissionRequestManagerTest, SkipNextUiSelector) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                           Decision::ShowNoWarning()),
      /* async_delay */ std::nullopt);
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_, Decision::UseNormalUiAndShowNoWarning(),
      /* async_delay */ std::nullopt);
  auto request1 = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1));
  WaitForBubbleToBeShown();
  auto* next_selector =
      manager_->get_permission_ui_selectors_for_testing().back().get();
  EXPECT_FALSE(static_cast<MockNotificationGeolocationPermissionUiSelector*>(
                   next_selector)
                   ->selected_ui_to_use());
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

TEST_F(PermissionRequestManagerTest, MultipleUiSelectors) {
  base::test::ScopedFeatureList
      enable_permission_predictions_geolocation_accuracy(
          features::kPermissionPredictionsGeolocationAccuracy);

  const struct {
    std::vector<Decision> decisions;
    std::vector<bool> simulate_delayed_decision;
    std::optional<QuietUiReason> expected_reason;
    GeolocationAccuracy expected_accuracy;
  } kTests[] = {
      // Simple sync selectors, first one should take priority.
      {
          {Decision::UseQuietUi(QuietUiReason::kTriggeredByCrowdDeny,
                                Decision::ShowNoWarning()),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {false, false},
          QuietUiReason::kTriggeredByCrowdDeny,
          GeolocationAccuracy::kPrecise,
      },
      {
          {Decision::UseQuietUi(
               QuietUiReason::kTriggeredDueToDisruptiveBehavior,
               Decision::ShowNoWarning()),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {false, false},
          QuietUiReason::kTriggeredDueToDisruptiveBehavior,
          GeolocationAccuracy::kPrecise,
      },
      {
          {Decision::UseQuietUi(
               QuietUiReason::kTriggeredDueToDisruptiveBehavior,
               Decision::ShowNoWarning()),
           Decision::UseQuietUi(
               QuietUiReason::kServicePredictedVeryUnlikelyGrant,
               Decision::ShowNoWarning())},
          {false, false},
          QuietUiReason::kTriggeredDueToDisruptiveBehavior,
          GeolocationAccuracy::kPrecise,
      },
      {
          {Decision::UseQuietUi(
               QuietUiReason::kTriggeredDueToDisruptiveBehavior,
               Decision::ShowNoWarning()),
           Decision::UseQuietUi(QuietUiReason::kTriggeredByCrowdDeny,
                                Decision::ShowNoWarning())},
          {false, false},
          QuietUiReason::kTriggeredDueToDisruptiveBehavior,
          GeolocationAccuracy::kPrecise,
      },
      // First selector is async but should still take priority even if it
      // returns later.
      {
          {Decision::UseQuietUi(QuietUiReason::kTriggeredByCrowdDeny,
                                Decision::ShowNoWarning()),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {true, false},
          QuietUiReason::kTriggeredByCrowdDeny,
          GeolocationAccuracy::kPrecise,
      },
      {
          {Decision::UseQuietUi(
               QuietUiReason::kTriggeredDueToDisruptiveBehavior,
               Decision::ShowNoWarning()),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {true, false},
          QuietUiReason::kTriggeredDueToDisruptiveBehavior,
          GeolocationAccuracy::kPrecise,
      },
      // The first selector that has a quiet ui decision should be used.
      {
          {Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kTriggeredDueToAbusiveContent,
                                Decision::ShowNoWarning()),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {false, true, true, false},
          QuietUiReason::kTriggeredDueToAbusiveContent,
          GeolocationAccuracy::kPrecise,
      },
      // If all selectors return a normal ui, it should use a normal ui.
      {
          {Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning()},
          {false, true},
          std::nullopt,
          GeolocationAccuracy::kPrecise,
      },
      // If all selectors return a normal ui, geolocation accuracy should
      // reflect the accuracy returned by the highest priority selection.
      {
          {Decision::UseNormalUi(
               Decision::ShowNoWarning(),
               PermissionUiSelector::GeolocationAccuracy::kApproximate),
           Decision::UseNormalUiAndShowNoWarning()},
          {false, true},
          std::nullopt,
          GeolocationAccuracy::kApproximate,
      },
      {
          {Decision::UseNormalUi(
               Decision::ShowNoWarning(),
               PermissionUiSelector::GeolocationAccuracy::kApproximate),
           Decision::UseNormalUi(
               Decision::ShowNoWarning(),
               PermissionUiSelector::GeolocationAccuracy::kPrecise)},
          {false, true},
          std::nullopt,
          GeolocationAccuracy::kApproximate,
      },
      {
          {Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUi(
               Decision::ShowNoWarning(),
               PermissionUiSelector::GeolocationAccuracy::kApproximate)},
          {false, true},
          std::nullopt,
          GeolocationAccuracy::kApproximate,
      },

      // Use a bunch of selectors both async and sync.
      {
          {Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kTriggeredDueToAbusiveRequests,
                                Decision::ShowNoWarning()),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {false, true, false, true, true, true, false, false},
          QuietUiReason::kTriggeredDueToAbusiveRequests,
          GeolocationAccuracy::kPrecise,
      },
      // Use a bunch of selectors all sync.
      {
          {Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kTriggeredDueToAbusiveRequests,
                                Decision::ShowNoWarning()),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {false, false, false, false, false, false, false, false},
          QuietUiReason::kTriggeredDueToAbusiveRequests,
          GeolocationAccuracy::kPrecise,
      },
      // Use a bunch of selectors all async.
      {
          {Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kTriggeredDueToAbusiveRequests,
                                Decision::ShowNoWarning()),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {true, true, true, true, true, true, true, true},
          QuietUiReason::kTriggeredDueToAbusiveRequests,
          GeolocationAccuracy::kPrecise,
      },
      // Use a bunch of selectors both async and sync.
      {
          {Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(
               QuietUiReason::kTriggeredDueToDisruptiveBehavior,
               Decision::ShowNoWarning()),
           Decision::UseNormalUiAndShowNoWarning(),
           Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                Decision::ShowNoWarning())},
          {true, false, false, true, true, true, false, false},
          QuietUiReason::kTriggeredDueToDisruptiveBehavior,
          GeolocationAccuracy::kPrecise,
      },
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.decisions.size(); ++i) {
      MockNotificationGeolocationPermissionUiSelector::CreateForManager(
          manager_, test.decisions[i],
          test.simulate_delayed_decision[i]
              ? std::make_optional<base::TimeDelta>()
              : std::nullopt);
    }

    MockPermissionRequest::MockPermissionRequestState request_state;
    auto request = std::make_unique<MockPermissionRequest>(
        RequestType::kGeolocation, PermissionRequestGestureType::GESTURE,
        request_state.GetWeakPtr());

    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request_state.request_type));
    if (test.expected_reason.has_value()) {
      EXPECT_EQ(test.expected_reason, manager_->ReasonForUsingQuietUi());
    } else {
      EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    }
    EXPECT_EQ(test.expected_accuracy,
              manager_->GetInitialGeolocationAccuracySelection());

    Accept();
    EXPECT_TRUE(request_state.granted);
  }
}

TEST_F(PermissionRequestManagerTest, SelectorsPredictionLikelihood) {
  using PredictionLikelihood = PermissionUiSelector::PredictionGrantLikelihood;
  const auto VeryLikely = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY;
  const auto Neutral = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL;

  const struct {
    std::vector<bool> enable_quiet_uis;
    std::vector<std::optional<PredictionLikelihood>> prediction_likelihoods;
    std::vector<bool> simulate_delayed_decision;
    std::optional<PredictionLikelihood> expected_prediction_likelihood;
  } kTests[] = {
      // Sanity check: prediction likelihood is populated correctly.
      {{true}, {VeryLikely}, {false}, VeryLikely},
      {{false}, {Neutral}, {false}, Neutral},

      // Prediction likelihood is populated only if the selector was considered.
      {{true, true}, {std::nullopt, VeryLikely}, {false, false}, std::nullopt},
      {{false, true}, {std::nullopt, VeryLikely}, {false, false}, VeryLikely},
      {{false, false}, {std::nullopt, VeryLikely}, {false, false}, VeryLikely},

      // Prediction likelihood is populated only if the selector was considered,
      // even if the second selector returns first.
      {{true, true}, {std::nullopt, VeryLikely}, {true, false}, std::nullopt},
      {{false, true}, {std::nullopt, VeryLikely}, {true, false}, VeryLikely},
      {{false, false}, {std::nullopt, VeryLikely}, {true, false}, VeryLikely},

      // First considered selector is preserved.
      {{true, true}, {Neutral, VeryLikely}, {false, false}, Neutral},
      {{false, true}, {Neutral, VeryLikely}, {false, false}, Neutral},
      {{false, false}, {Neutral, VeryLikely}, {false, false}, Neutral},

      // First considered selector is preserved, even if the second selector
      // returns first.
      {{true, true}, {Neutral, VeryLikely}, {true, false}, Neutral},
      {{false, true}, {Neutral, VeryLikely}, {true, false}, Neutral},
      {{false, false}, {Neutral, VeryLikely}, {true, false}, Neutral},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.enable_quiet_uis.size(); ++i) {
      MockNotificationGeolocationPermissionUiSelector::CreateForManager(
          manager_,
          test.enable_quiet_uis[i]
              ? Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                                     Decision::ShowNoWarning())
              : Decision::UseNormalUiAndShowNoWarning(),
          test.simulate_delayed_decision[i]
              ? std::make_optional<base::TimeDelta>()
              : std::nullopt,
          test.prediction_likelihoods[i]);
    }

    MockPermissionRequest::MockPermissionRequestState request_state;
    auto request = std::make_unique<MockPermissionRequest>(
        RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
        request_state.GetWeakPtr());

    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request_state.request_type));
    EXPECT_EQ(test.expected_prediction_likelihood,
              manager_->prediction_grant_likelihood_for_testing());

    Accept();
    EXPECT_TRUE(request_state.granted);
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
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                           Decision::ShowNoWarning()),
      std::make_optional<base::TimeDelta>());
  for (const auto& test : kTests) {
    auto request = std::make_unique<MockPermissionRequest>(
        test.request_type, PermissionRequestGestureType::GESTURE);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    EXPECT_EQ(test.should_request_use_quiet_ui,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();
  }
  // Adding a mock PermissionUiSelector that handles Camera stream.
  MockCameraStreamPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(QuietUiReason::kEnabledInPrefs,
                           Decision::ShowNoWarning()),
      std::make_optional<base::TimeDelta>());
  // Now the RequestType::kCameraStream should show a quiet UI as well
  auto request2 = std::make_unique<MockPermissionRequest>(
      RequestType::kCameraStream, PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request2));
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

////////////////////////////////////////////////////////////////////////////////
// Quiet UI chip. Low priority for Notifications & Geolocation.
////////////////////////////////////////////////////////////////////////////////

TEST_F(PermissionRequestManagerTest, NotificationsSingleBubbleAndChipRequest) {
  MockPermissionRequest::MockPermissionRequestState request_state;
  auto request = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
      request_state.GetWeakPtr());

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request_state.granted);
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
  auto request_notifications = CreateAndAddRequest(RequestType::kNotifications,
                                                   /*should_be_seen=*/true, 1);
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/false, 1);
  auto request_camera = CreateAndAddRequest(RequestType::kCameraStream,
                                            /*should_be_seen=*/false, 1);

  WaitAndAcceptPromptForRequest(request_notifications.get());
  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_camera.get());

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
  auto request_camera = CreateAndAddRequest(RequestType::kCameraStream,
                                            /*should_be_seen=*/true, 1);
  auto request_clipboard =
      CreateAndAddRequest(RequestType::kClipboard, /*should_be_seen=*/true, 2);
  auto request_midi =
      CreateAndAddRequest(RequestType::kMidiSysex, /*should_be_seen=*/true, 3);

  WaitAndAcceptPromptForRequest(request_midi.get());
  WaitAndAcceptPromptForRequest(request_clipboard.get());
  WaitAndAcceptPromptForRequest(request_camera.get());

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
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(QuietUiReason::kTriggeredDueToAbusiveRequests,
                           Decision::ShowNoWarning()),
      std::make_optional<base::TimeDelta>(base::Seconds(2)));
  MockPermissionRequest::MockPermissionRequestState request1_state;
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));

  task_environment()->FastForwardBy(base::Seconds(1));

  MockPermissionRequest::MockPermissionRequestState request_state;
  auto request = std::make_unique<MockPermissionRequest>(
      RequestType::kClipboard, PermissionRequestGestureType::GESTURE,
      request_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request));
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  WaitForBubbleToBeShown();
  manager_->clear_permission_ui_selector_for_testing();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
}

// Class to run tests both with kApproximateGeolocationPermission enabled and
// disabled;
class PermissionRequestManagerAlsoWithApproximateGeolocationTest
    : public PermissionRequestManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  PermissionRequestManagerAlsoWithApproximateGeolocationTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          content_settings::features::kApproximateGeolocationPermission);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          content_settings::features::kApproximateGeolocationPermission);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

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
TEST_P(PermissionRequestManagerAlsoWithApproximateGeolocationTest,
       AbusiveNotificationsGeolocationQuietUIChipRequest) {
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(
          PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
          Decision::ShowNoWarning()),
      std::nullopt /* async_delay */);

  auto request_notifications = CreateAndAddRequest(RequestType::kNotifications,
                                                   /*should_be_seen=*/true, 1);

  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 2);

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
TEST_P(PermissionRequestManagerAlsoWithApproximateGeolocationTest,
       AbusiveNotificationsShownLongEnough) {
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(
          PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
          Decision::ShowNoWarning()),
      std::nullopt /* async_delay */);

  auto request_notifications = CreateAndAddRequest(RequestType::kNotifications,
                                                   /*should_be_seen=*/true, 1);

  // More then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(9000));

  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 2);

  // The second permission was requested after 8.5 second window, the quiet UI
  // Notifiations request for an abusive origin is automatically ignored.
  EXPECT_FALSE(request_notifications->granted);
  EXPECT_TRUE(request_notifications->finished);

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
TEST_P(PermissionRequestManagerAlsoWithApproximateGeolocationTest,
       AbusiveNotificationsShownLongEnoughCamera) {
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(
          PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
          Decision::ShowNoWarning()),
      std::nullopt /* async_delay */);

  auto request_notifications = CreateAndAddRequest(RequestType::kNotifications,
                                                   /*should_be_seen=*/true, 1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 2);
  auto request_camera = CreateAndAddRequest(RequestType::kCameraStream,
                                            /*should_be_seen=*/true, 3);

  // The second permission was requested in 8.5 second window, the quiet UI
  // Notifiations request for an abusive origin is not automatically ignored.
  EXPECT_FALSE(request_notifications->granted);
  EXPECT_FALSE(request_notifications->finished);

  WaitAndAcceptPromptForRequest(request_camera.get());
  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

INSTANTIATE_TEST_SUITE_P(
    kApproximateGeolocationPermissionFeature,
    PermissionRequestManagerAlsoWithApproximateGeolocationTest,
    testing::Values(false, true));
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
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(
          PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
          Decision::ShowNoWarning()),
      std::nullopt /* async_delay */);

  auto request_camera = CreateAndAddRequest(RequestType::kCameraStream,
                                            /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  auto request_notifications = CreateAndAddRequest(RequestType::kNotifications,
                                                   /*should_be_seen=*/false, 1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/false, 1);

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted);
  EXPECT_FALSE(request_notifications->finished);

  WaitAndAcceptPromptForRequest(request_camera.get());
  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

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
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(
          PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
          Decision::ShowNoWarning()),
      std::nullopt /* async_delay */);

  auto request_camera = CreateAndAddRequest(RequestType::kCameraStream,
                                            /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  auto request_notifications = CreateAndAddRequest(RequestType::kNotifications,
                                                   /*should_be_seen=*/false, 1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/false, 1);

  // Since the chip is enabled, MIDI should be shown.
  auto request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                          /*should_be_seen=*/true, 2);

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted);
  EXPECT_FALSE(request_notifications->finished);

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
  auto request_camera = CreateAndAddRequest(RequestType::kCameraStream,
                                            /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  auto request_notifications = CreateAndAddRequest(RequestType::kNotifications,
                                                   /*should_be_seen=*/false, 1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::Milliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/false, 1);

  // If Chip is enabled, MIDI should be shown, otherwise MIDI should not be
  // shown.
  auto request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                          /*should_be_seen=*/true, 2);

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted);
  EXPECT_FALSE(request_notifications->finished);

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
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;

  auto request1 = CreateRequest(request1_, request1_state.GetWeakPtr());
  request1->RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(
            web_contents()->GetPrimaryMainFrame(),
            CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
  EXPECT_FALSE(request_mic_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
}

// Verifies that a high-priority request cannot preempt a low-priority request
// if the high-priority request comes in as the result of a permission prompt
// being accepted once.
// Permissions requested in order:
// 1. Gelocation (low)
// 2. Mic (high)
TEST_F(PermissionRequestManagerTest, ReentrantPermissionRequestAcceptOnce) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  auto request1 = CreateRequest(request1_, request1_state.GetWeakPtr());

  request1->RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(
            web_contents()->GetPrimaryMainFrame(),
            CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  AcceptThisTime();
  EXPECT_TRUE(request1_state.granted);
  EXPECT_FALSE(request_mic_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
}

// Verifies that a high-priority request cannot preempt a low-priority request
// if the high-priority request comes in as the result of a permission prompt
// being denied.
// Permissions requested in order:
// 1. Gelocation (low)
// 2. Mic (high)
TEST_F(PermissionRequestManagerTest, ReentrantPermissionRequestDeny) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  auto request1 = CreateRequest(request1_, request1_state.GetWeakPtr());
  request1->RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(
            web_contents()->GetPrimaryMainFrame(),
            CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Deny();
  EXPECT_FALSE(request1_state.granted);
  EXPECT_FALSE(request_mic_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request1_state.granted);
  EXPECT_TRUE(request_mic_state.granted);
}

// Verifies that a high-priority request cannot preempt a low-priority request
// if the high-priority request comes in as the result of a permission prompt
// being dismissed.
// Permissions requested in order:
// 1. Gelocation (low)
// 2. Mic (high)
TEST_F(PermissionRequestManagerTest, ReentrantPermissionRequestCancelled) {
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  auto request1 = CreateRequest(request1_, request1_state.GetWeakPtr());
  request1->RegisterOnPermissionDecidedCallback(
      base::BindLambdaForTesting([&]() {
        manager_->AddRequest(
            web_contents()->GetPrimaryMainFrame(),
            CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
      }));

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request1));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(request1_state.cancelled);
  EXPECT_FALSE(request_mic_state.cancelled);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request1_state.granted);
  EXPECT_TRUE(request_mic_state.granted);
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
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request2_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
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
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  MockPermissionRequest::MockPermissionRequestState request_state;
  auto request = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
      request_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
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
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));

  MockPermissionRequest::MockPermissionRequestState request_state;
  auto request = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
      request_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
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
  MockPermissionRequest::MockPermissionRequestState request_midi_state;
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;
  MockPermissionRequest::MockPermissionRequestState request_notification_state;

  auto request_midi = std::make_unique<MockPermissionRequest>(
      RequestType::kMidiSysex, PermissionRequestGestureType::GESTURE,
      request_midi_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request_midi));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  auto request_notification = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
      request_notification_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request_notification));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request_mic_state.granted);
  EXPECT_TRUE(request2_state.granted);
  EXPECT_FALSE(request_notification_state.granted);
  EXPECT_FALSE(request1_state.granted);
  EXPECT_FALSE(request_midi_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request_notification_state.granted);
  EXPECT_FALSE(request1_state.granted);
  EXPECT_FALSE(request_midi_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_FALSE(request_notification_state.granted);
  EXPECT_FALSE(request1_state.granted);
  EXPECT_TRUE(request_midi_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_notification_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
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
  MockPermissionRequest::MockPermissionRequestState request_state;
  MockPermissionRequest::MockPermissionRequestState request1_state;
  MockPermissionRequest::MockPermissionRequestState request2_state;
  MockPermissionRequest::MockPermissionRequestState request_mic_state;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request1_, request1_state.GetWeakPtr()));
  auto request = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
      request_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request));
  WaitForBubbleToBeShown();

  manager_->AddRequest(
      web_contents()->GetPrimaryMainFrame(),
      CreateRequest(request_mic_, request_mic_state.GetWeakPtr()));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(request2_, request2_state.GetWeakPtr()));
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_state.granted);
  EXPECT_FALSE(request_mic_state.granted);
  EXPECT_FALSE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_state.granted);
  EXPECT_FALSE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_state.granted);
  EXPECT_FALSE(request1_state.granted);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_state.granted);
}

TEST_F(PermissionRequestManagerTest, PEPCRequestNeverQuiet) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationGeolocationPermissionUiSelector::CreateForManager(
      manager_,
      Decision::UseQuietUi(PermissionUiSelector::QuietUiReason::kEnabledInPrefs,
                           Decision::ShowNoWarning()),
      std::nullopt /* async_delay */);

  // PEPC request is not quieted by selector.
  MockPermissionRequest::MockPermissionRequestState pepc_request_state;
  auto pepc_request = std::make_unique<MockPermissionRequest>(
      GURL(MockPermissionRequest::kDefaultOrigin), RequestType::kNotifications,
      /*embedded_permission_element_initiated=*/true,
      pepc_request_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(pepc_request));
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(
      prompt_factory_->RequestTypeSeen(pepc_request_state.request_type));
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  // Regular request is quieted by selector.
  MockPermissionRequest::MockPermissionRequestState request_state;
  auto request = std::make_unique<MockPermissionRequest>(
      RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
      request_state.GetWeakPtr());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(request));
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(prompt_factory_->RequestTypeSeen(request_state.request_type));
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

#endif  // BUILDFLAG(IS_ANDROID)

class PermissionRequestManagerApproximateGeolocationTest
    : public PermissionRequestManagerTest,
      public testing::WithParamInterface<GeolocationAccuracy> {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      content_settings::features::kApproximateGeolocationPermission};
};

// Match UkmPromptOptions in permission_uma_util.cc.
constexpr int64_t kPromptOptionsApproximate = 1;
constexpr int64_t kPromptOptionsPrecise = 2;

TEST_P(PermissionRequestManagerApproximateGeolocationTest,
       ReportAccuracyInUmaAOnAccept) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 1);

  GeolocationAccuracy accuracy = GetParam();
  manager_->SetPromptOptions(GeolocationPromptOptions{accuracy});
  WaitAndAcceptPromptForRequest(request_geolocation.get());

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Geolocation.Accepted.Accuracy",
      /*sample=*/static_cast<int>(accuracy),
      /*expected_bucket_count=*/1);

  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
      entries = test_ukm_recorder.GetEntriesByName("Permission");
  ASSERT_THAT(entries, SizeIs(1));
  EXPECT_EQ(
      *test_ukm_recorder.GetEntryMetric(entries.front(), "PermissionType"),
      kPermissionTypeGeolocationWithOptions);
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "Action"),
            static_cast<int64_t>(PermissionAction::GRANTED));
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "PromptOptions"),
            accuracy == GeolocationAccuracy::kPrecise
                ? kPromptOptionsPrecise
                : kPromptOptionsApproximate);
}

TEST_P(PermissionRequestManagerApproximateGeolocationTest,
       ReportAccuracyInUmaOnAcceptThisTime) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 1);

  GeolocationAccuracy accuracy = GetParam();
  manager_->SetPromptOptions(GeolocationPromptOptions{accuracy});
  WaitForBubbleToBeShown();
  AcceptThisTime();

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Geolocation.AcceptedOnce.Accuracy",
      /*sample=*/static_cast<int>(accuracy),
      /*expected_bucket_count=*/1);

  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
      entries = test_ukm_recorder.GetEntriesByName("Permission");
  ASSERT_THAT(entries, SizeIs(1));
  EXPECT_EQ(
      *test_ukm_recorder.GetEntryMetric(entries.front(), "PermissionType"),
      kPermissionTypeGeolocationWithOptions);
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "Action"),
            static_cast<int64_t>(PermissionAction::GRANTED_ONCE));
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "PromptOptions"),
            accuracy == GeolocationAccuracy::kPrecise
                ? kPromptOptionsPrecise
                : kPromptOptionsApproximate);
}

TEST_P(PermissionRequestManagerApproximateGeolocationTest,
       ReportAccuracyInUmaOnDeny) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 1);

  GeolocationAccuracy accuracy = GetParam();
  manager_->SetPromptOptions(GeolocationPromptOptions{accuracy});
  WaitForBubbleToBeShown();
  Deny();

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Geolocation.Denied.Accuracy",
      /*sample=*/static_cast<int>(accuracy),
      /*expected_bucket_count=*/1);
  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
      entries = test_ukm_recorder.GetEntriesByName("Permission");
  ASSERT_THAT(entries, SizeIs(1));
  EXPECT_EQ(
      *test_ukm_recorder.GetEntryMetric(entries.front(), "PermissionType"),
      kPermissionTypeGeolocationWithOptions);
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "Action"),
            static_cast<int64_t>(PermissionAction::DENIED));
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "PromptOptions"),
            accuracy == GeolocationAccuracy::kPrecise
                ? kPromptOptionsPrecise
                : kPromptOptionsApproximate);
}

TEST_P(PermissionRequestManagerApproximateGeolocationTest,
       ReportAccuracyInUmaOnDismiss) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 1);

  GeolocationAccuracy accuracy = GetParam();
  manager_->SetPromptOptions(GeolocationPromptOptions{accuracy});
  WaitForBubbleToBeShown();
  Closing();

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Geolocation.Dismissed.Accuracy",
      /*sample=*/static_cast<int>(accuracy),
      /*expected_bucket_count=*/1);
  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
      entries = test_ukm_recorder.GetEntriesByName("Permission");
  ASSERT_THAT(entries, SizeIs(1));
  EXPECT_EQ(
      *test_ukm_recorder.GetEntryMetric(entries.front(), "PermissionType"),
      kPermissionTypeGeolocationWithOptions);
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "Action"),
            static_cast<int64_t>(PermissionAction::DISMISSED));
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "PromptOptions"),
            accuracy == GeolocationAccuracy::kPrecise
                ? kPromptOptionsPrecise
                : kPromptOptionsApproximate);
}

TEST_P(PermissionRequestManagerApproximateGeolocationTest,
       ReportAccuracyInUmaOnIgnore) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto request_geolocation = CreateAndAddRequest(RequestType::kGeolocation,
                                                 /*should_be_seen=*/true, 1);

  GeolocationAccuracy accuracy = GetParam();
  manager_->SetPromptOptions(GeolocationPromptOptions{accuracy});
  WaitForBubbleToBeShown();
  Ignore();

  histograms.ExpectUniqueSample(
      "Permissions.Prompt.Geolocation.Ignored.Accuracy",
      /*sample=*/static_cast<int>(accuracy),
      /*expected_bucket_count=*/1);

  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
      entries = test_ukm_recorder.GetEntriesByName("Permission");
  ASSERT_THAT(entries, SizeIs(1));
  EXPECT_EQ(
      *test_ukm_recorder.GetEntryMetric(entries.front(), "PermissionType"),
      kPermissionTypeGeolocationWithOptions);
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "Action"),
            static_cast<int64_t>(PermissionAction::IGNORED));
  EXPECT_EQ(*test_ukm_recorder.GetEntryMetric(entries.front(), "PromptOptions"),
            accuracy == GeolocationAccuracy::kPrecise
                ? kPromptOptionsPrecise
                : kPromptOptionsApproximate);
}

INSTANTIATE_TEST_SUITE_P(Accuracies,
                         PermissionRequestManagerApproximateGeolocationTest,
                         testing::Values(GeolocationAccuracy::kPrecise,
                                         GeolocationAccuracy::kApproximate));

}  // namespace permissions
