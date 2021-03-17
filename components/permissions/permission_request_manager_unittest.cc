// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

namespace {
using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;
}

class PermissionRequestManagerTest
    : public content::RenderViewHostTestHarness,
      public ::testing::WithParamInterface<bool> {
 public:
  PermissionRequestManagerTest()
      : content::RenderViewHostTestHarness(),
        request1_("test1",
                  RequestType::kDiskQuota,
                  PermissionRequestGestureType::GESTURE),
        request2_("test2",
                  RequestType::kMultipleDownloads,
                  PermissionRequestGestureType::NO_GESTURE),
        request_mic_("mic",
                     RequestType::kMicStream,
                     PermissionRequestGestureType::NO_GESTURE),
        request_camera_("cam",
                        RequestType::kCameraStream,
                        PermissionRequestGestureType::NO_GESTURE),
#if !defined(OS_ANDROID)
        request_ptz_("ptz",
                     RequestType::kCameraPanTiltZoom,
                     PermissionRequestGestureType::NO_GESTURE),
#endif
        iframe_request_same_domain_("iframe",
                                    RequestType::kNotifications,
                                    GURL("http://www.google.com/some/url")),
        iframe_request_other_domain_("iframe",
                                     RequestType::kGeolocation,
                                     GURL("http://www.youtube.com")),
        iframe_request_camera_other_domain_("iframe",
                                            RequestType::kCameraStream,
                                            GURL("http://www.youtube.com")),
        iframe_request_mic_other_domain_("iframe",
                                         RequestType::kMicStream,
                                         GURL("http://www.youtube.com")) {
    feature_list_.InitWithFeatureState(permissions::features::kPermissionChip,
                                       GetParam());
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    url_ = GURL("http://www.google.com");
    NavigateAndCommit(url_);

    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  void Accept() {
    manager_->Accept();
    base::RunLoop().RunUntilIdle();
  }

  void Deny() {
    manager_->Deny();
    base::RunLoop().RunUntilIdle();
  }

  void Closing() {
    manager_->Closing();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForFrameLoad() {
    // PermissionRequestManager ignores all parameters. Yay?
    manager_->DOMContentLoaded(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame(main_rfh());
    base::RunLoop().RunUntilIdle();
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

 protected:
  GURL url_;
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  MockPermissionRequest request_mic_;
  MockPermissionRequest request_camera_;
#if !defined(OS_ANDROID)
  MockPermissionRequest request_ptz_;
#endif
  MockPermissionRequest iframe_request_same_domain_;
  MockPermissionRequest iframe_request_other_domain_;
  MockPermissionRequest iframe_request_camera_other_domain_;
  MockPermissionRequest iframe_request_mic_other_domain_;
  PermissionRequestManager* manager_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  TestPermissionsClient client_;
  base::test::ScopedFeatureList feature_list_;
};

////////////////////////////////////////////////////////////////////////////////
// General
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, NoRequests) {
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_P(PermissionRequestManagerTest, SingleRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_P(PermissionRequestManagerTest, SequentialRequests) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  Accept();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request2_.granted());
}

TEST_P(PermissionRequestManagerTest, ForgetRequestsOnPageNavigation) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
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

TEST_P(PermissionRequestManagerTest, RequestsDontNeedUserGesture) {
  WaitForFrameLoad();
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_P(PermissionRequestManagerTest, RequestsNotSupported) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  Accept();
  EXPECT_TRUE(request1_.granted());

  manager_->set_web_contents_supports_permission_requests(false);

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  EXPECT_TRUE(request2_.cancelled());
}

////////////////////////////////////////////////////////////////////////////////
// Requests grouping
////////////////////////////////////////////////////////////////////////////////

// Most requests should never be grouped.
TEST_P(PermissionRequestManagerTest, TwoRequestsUngrouped) {
  // Grouping for chip feature is tested in ThreeRequestsStackOrderChip.
  if (GetParam())
    return;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

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

TEST_P(PermissionRequestManagerTest, ThreeRequestsStackOrderChip) {
  if (!GetParam())
    return;

  // Test new permissions order, requests shouldn't be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
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
TEST_P(PermissionRequestManagerTest, ThreeRequestsOneByOneStackOrderChip) {
  if (!GetParam())
    return;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
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

// Only mic/camera requests from the same origin should be grouped.
TEST_P(PermissionRequestManagerTest, MicCameraGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

#if !defined(OS_ANDROID)
// Only camera/ptz requests from the same origin should be grouped.
TEST_P(PermissionRequestManagerTest, CameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_camera_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

// Only mic/camera/ptz requests from the same origin should be grouped.
TEST_P(PermissionRequestManagerTest, MicCameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 3);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());

  // If the requests come from different origins, they should not be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
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
#endif  // !defined(OS_ANDROID)

// Tests mix of grouped media requests and non-groupable request.
TEST_P(PermissionRequestManagerTest, MixOfMediaAndNotMediaRequests) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
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

////////////////////////////////////////////////////////////////////////////////
// Tab switching
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  MockTabSwitchAway();
#if defined(OS_ANDROID)
  EXPECT_TRUE(prompt_factory_->is_visible());
#else
  EXPECT_FALSE(prompt_factory_->is_visible());
#endif

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
}

TEST_P(PermissionRequestManagerTest, PermissionRequestWhileTabSwitchedAway) {
  MockTabSwitchAway();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
}

////////////////////////////////////////////////////////////////////////////////
// Duplicated requests
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, SameRequestRejected) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_P(PermissionRequestManagerTest, DuplicateRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

  MockPermissionRequest dupe_request("test1");
  manager_->AddRequest(web_contents()->GetMainFrame(), &dupe_request);
  EXPECT_FALSE(dupe_request.finished());
  EXPECT_FALSE(request1_.finished());

  MockPermissionRequest dupe_request2("test2");
  manager_->AddRequest(web_contents()->GetMainFrame(), &dupe_request2);
  EXPECT_FALSE(dupe_request2.finished());
  EXPECT_FALSE(request2_.finished());

  WaitForBubbleToBeShown();
  Accept();
  if (GetParam()) {
    EXPECT_TRUE(dupe_request2.finished());
    EXPECT_TRUE(request2_.finished());
  } else {
    EXPECT_TRUE(dupe_request.finished());
    EXPECT_TRUE(request1_.finished());
  }

  WaitForBubbleToBeShown();
  Accept();
  if (GetParam()) {
    EXPECT_TRUE(dupe_request.finished());
    EXPECT_TRUE(request1_.finished());
  } else {
    EXPECT_TRUE(dupe_request2.finished());
    EXPECT_TRUE(request2_.finished());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Requests from iframes
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, MainFrameNoRequestIFrameRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
  WaitForBubbleToBeShown();
  WaitForFrameLoad();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_P(PermissionRequestManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  if (GetParam()) {
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
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_P(PermissionRequestManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (GetParam()) {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_other_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_P(PermissionRequestManagerTest, IFrameRequestWhenMainRequestVisible) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
  WaitForFrameLoad();
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  if (GetParam()) {
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
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_P(PermissionRequestManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  if (GetParam()) {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_other_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_other_domain_.finished());
}

////////////////////////////////////////////////////////////////////////////////
// UMA logging
////////////////////////////////////////////////////////////////////////////////

// This code path (calling Accept on a non-merged bubble, with no accepted
// permission) would never be used in actual Chrome, but its still tested for
// completeness.
TEST_P(PermissionRequestManagerTest, UMAForSimpleDeniedBubbleAlternatePath) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(RequestTypeForUma::QUOTA), 1);
}

TEST_P(PermissionRequestManagerTest, UMAForTabSwitching) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(RequestTypeForUma::QUOTA), 1);

  MockTabSwitchAway();
  MockTabSwitchBack();
  histograms.ExpectUniqueSample(
      PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(RequestTypeForUma::QUOTA), 1);
}

////////////////////////////////////////////////////////////////////////////////
// UI selectors
////////////////////////////////////////////////////////////////////////////////

// Simulate a NotificationPermissionUiSelector that simply returns a
// predefined |ui_to_use| every time.
class MockNotificationPermissionUiSelector
    : public NotificationPermissionUiSelector {
 public:
  explicit MockNotificationPermissionUiSelector(
      base::Optional<QuietUiReason> quiet_ui_reason,
      base::Optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood,
      bool async)
      : quiet_ui_reason_(quiet_ui_reason),
        prediction_likelihood_(prediction_likelihood),
        async_(async) {}

  void SelectUiToUse(PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    Decision decision(quiet_ui_reason_, Decision::ShowNoWarning());
    if (async_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), decision));
    } else {
      std::move(callback).Run(decision);
    }
  }

  base::Optional<PermissionUmaUtil::PredictionGrantLikelihood>
  PredictedGrantLikelihoodForUKM() override {
    return prediction_likelihood_;
  }

  static void CreateForManager(
      PermissionRequestManager* manager,
      base::Optional<QuietUiReason> quiet_ui_reason,
      bool async,
      base::Optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood = base::nullopt) {
    manager->add_notification_permission_ui_selector_for_testing(
        std::make_unique<MockNotificationPermissionUiSelector>(
            quiet_ui_reason, prediction_likelihood, async));
  }

 private:
  base::Optional<QuietUiReason> quiet_ui_reason_;
  base::Optional<PermissionUmaUtil::PredictionGrantLikelihood>
      prediction_likelihood_;
  bool async_;
};

TEST_P(PermissionRequestManagerTest,
       UiSelectorNotUsedForPermissionsOtherThanNotification) {
  manager_->clear_notification_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      NotificationPermissionUiSelector::QuietUiReason::kEnabledInPrefs,
      false /* async */);

  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(
      prompt_factory_->RequestTypeSeen(request_camera_.GetRequestType()));
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  EXPECT_TRUE(request_camera_.granted());
}

TEST_P(PermissionRequestManagerTest, UiSelectorUsedForNotifications) {
  const struct {
    base::Optional<NotificationPermissionUiSelector::QuietUiReason>
        quiet_ui_reason;
    bool async;
  } kTests[] = {
      {QuietUiReason::kEnabledInPrefs, true},
      {NotificationPermissionUiSelector::Decision::UseNormalUi(), true},
      {QuietUiReason::kEnabledInPrefs, false},
      {NotificationPermissionUiSelector::Decision::UseNormalUi(), false},
  };

  for (const auto& test : kTests) {
    manager_->clear_notification_permission_ui_selector_for_testing();
    MockNotificationPermissionUiSelector::CreateForManager(
        manager_, test.quiet_ui_reason, test.async);

    MockPermissionRequest request("foo", RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.GetRequestType()));
    EXPECT_EQ(!!test.quiet_ui_reason,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();

    EXPECT_TRUE(request.granted());
  }
}

TEST_P(PermissionRequestManagerTest,
       UiSelectionHappensSeparatelyForEachRequest) {
  using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;
  manager_->clear_notification_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs, true);
  MockPermissionRequest request1("request1", RequestType::kNotifications,
                                 PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  MockPermissionRequest request2("request2", RequestType::kNotifications,
                                 PermissionRequestGestureType::GESTURE);
  manager_->clear_notification_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, NotificationPermissionUiSelector::Decision::UseNormalUi(),
      true);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

TEST_P(PermissionRequestManagerTest, MultipleUiSelectors) {
  using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;

  const struct {
    std::vector<base::Optional<QuietUiReason>> quiet_ui_reasons;
    std::vector<bool> simulate_delayed_decision;
    base::Optional<QuietUiReason> expected_reason;
  } kTests[] = {
      // Simple sync selectors, first one should take priority.
      {{QuietUiReason::kTriggeredByCrowdDeny, QuietUiReason::kEnabledInPrefs},
       {false, false},
       QuietUiReason::kTriggeredByCrowdDeny},
      // First selector is async but should still take priority even if it
      // returns later.
      {{QuietUiReason::kTriggeredByCrowdDeny, QuietUiReason::kEnabledInPrefs},
       {true, false},
       QuietUiReason::kTriggeredByCrowdDeny},
      // The first selector that has a quiet ui decision should be used.
      {{base::nullopt, base::nullopt,
        QuietUiReason::kTriggeredDueToAbusiveContent,
        QuietUiReason::kEnabledInPrefs},
       {false, true, true, false},
       QuietUiReason::kTriggeredDueToAbusiveContent},
      // If all selectors return a normal ui, it should use a normal ui.
      {{base::nullopt, base::nullopt}, {false, true}, base::nullopt},

      // Use a bunch of selectors both async and sync.
      {{base::nullopt, base::nullopt, base::nullopt, base::nullopt,
        base::nullopt, QuietUiReason::kTriggeredDueToAbusiveRequests,
        base::nullopt, QuietUiReason::kEnabledInPrefs},
       {false, true, false, true, true, true, false, false},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      // Use a bunch of selectors all sync.
      {{base::nullopt, base::nullopt, base::nullopt, base::nullopt,
        base::nullopt, QuietUiReason::kTriggeredDueToAbusiveRequests,
        base::nullopt, QuietUiReason::kEnabledInPrefs},
       {false, false, false, false, false, false, false, false},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      // Use a bunch of selectors all async.
      {{base::nullopt, base::nullopt, base::nullopt, base::nullopt,
        base::nullopt, QuietUiReason::kTriggeredDueToAbusiveRequests,
        base::nullopt, QuietUiReason::kEnabledInPrefs},
       {true, true, true, true, true, true, true, true},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
  };

  for (const auto& test : kTests) {
    manager_->clear_notification_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.quiet_ui_reasons.size(); ++i) {
      MockNotificationPermissionUiSelector::CreateForManager(
          manager_, test.quiet_ui_reasons[i],
          test.simulate_delayed_decision[i]);
    }

    MockPermissionRequest request("foo", RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.GetRequestType()));
    if (test.expected_reason.has_value()) {
      EXPECT_EQ(test.expected_reason, manager_->ReasonForUsingQuietUi());
    } else {
      EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    }

    Accept();
    EXPECT_TRUE(request.granted());
  }
}

TEST_P(PermissionRequestManagerTest, SelectorsPredictionLikelihood) {
  using QuietUiReason = NotificationPermissionUiSelector::QuietUiReason;
  using PredictionLikelihood = PermissionUmaUtil::PredictionGrantLikelihood;
  const auto VeryLikely = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY;
  const auto Neutral = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL;

  const struct {
    std::vector<bool> enable_quiet_uis;
    std::vector<base::Optional<PredictionLikelihood>> prediction_likelihoods;
    base::Optional<PredictionLikelihood> expected_prediction_likelihood;
  } kTests[] = {
      // Sanity check: prediction likelihood is populated correctly.
      {{true}, {VeryLikely}, VeryLikely},
      {{false}, {Neutral}, Neutral},

      // Prediction likelihood is populated only if the selector was considered.
      {{true, true}, {base::nullopt, VeryLikely}, base::nullopt},
      {{false, true}, {base::nullopt, VeryLikely}, VeryLikely},
      {{false, false}, {base::nullopt, VeryLikely}, VeryLikely},

      // First considered selector is preserved.
      {{true, true}, {Neutral, VeryLikely}, Neutral},
      {{false, true}, {Neutral, VeryLikely}, Neutral},
      {{false, false}, {Neutral, VeryLikely}, Neutral},
  };

  for (const auto& test : kTests) {
    manager_->clear_notification_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.enable_quiet_uis.size(); ++i) {
      MockNotificationPermissionUiSelector::CreateForManager(
          manager_,
          test.enable_quiet_uis[i]
              ? base::Optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)
              : base::nullopt,
          false /* async */, test.prediction_likelihoods[i]);
    }

    MockPermissionRequest request("foo", RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.GetRequestType()));
    EXPECT_EQ(test.expected_prediction_likelihood,
              manager_->prediction_grant_likelihood_for_testing());

    Accept();
    EXPECT_TRUE(request.granted());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PermissionRequestManagerTest,
                         ::testing::Values(false, true));

}  // namespace permissions
