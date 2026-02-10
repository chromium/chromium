// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/android_permission_util.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

// This test suite is for testing the Android permission util functions that
// connect to Java via JNI. These functions are tested in isolation to avoid
// needing to mock the JNI bridge. Some of the Android permission Util functions
// can be executed only behind a Finch flag but because the flag verification
// happens in another place, the implementation of these functions does not
// require a direct feature activation. E.g., `ResolvePermissionRequest` and
// `DismissPermissionRequest` are behind the PermissionsAndroidClapperLoud flag,
// hence the corresponding histograms are verified in the tests but the flag is
// not explicitly set and/or verified in these tests.
class AndroidPermissionUtilTest : public content::RenderViewHostTestHarness {
 public:
  AndroidPermissionUtilTest() = default;
  ~AndroidPermissionUtilTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  void AddRequest(RequestType type,
                  MockPermissionRequest::MockPermissionRequestState* state) {
    auto request =
        std::make_unique<MockPermissionRequest>(type, state->GetWeakPtr());
    manager_->AddRequest(main_rfh(), std::move(request));
    base::RunLoop().RunUntilIdle();
  }

  TestPermissionsClient client_;
  raw_ptr<PermissionRequestManager> manager_ = nullptr;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(AndroidPermissionUtilTest, ResolvePermissionRequest_NoManager) {
  // Pass nullptr as web_contents.
  internal::ResolveClapperViaReset(nullptr);
  internal::ResolveClapperViaSubscribe(nullptr);
  internal::ResolveLoudClapperViaAllow(nullptr);
  internal::ResolveClapperViaClose(nullptr);
  // Should not crash.
}

TEST_F(AndroidPermissionUtilTest, ResolvePermissionRequest_NoRequests) {
  // Should not crash.
  internal::ResolveClapperViaReset(web_contents());
  internal::ResolveClapperViaSubscribe(web_contents());
  internal::ResolveLoudClapperViaAllow(web_contents());
  internal::ResolveClapperViaClose(web_contents());
}

TEST_F(AndroidPermissionUtilTest, ResolvePermissionRequest_MismatchType) {
  MockPermissionRequest::MockPermissionRequestState state;
  AddRequest(RequestType::kGeolocation, &state);

  // Calls require notifications permission type.
  internal::ResolveClapperViaReset(web_contents());
  internal::ResolveClapperViaSubscribe(web_contents());
  internal::ResolveLoudClapperViaAllow(web_contents());
  internal::ResolveClapperViaClose(web_contents());

  // Request should still be in progress and not decided.
  EXPECT_TRUE(manager_->IsRequestInProgress());
  EXPECT_FALSE(state.granted);
  EXPECT_FALSE(state.finished);
  EXPECT_FALSE(state.cancelled);
}

TEST_F(AndroidPermissionUtilTest, ResolveClapperViaSubscribe) {
  base::HistogramTester histogram_tester;
  MockPermissionRequest::MockPermissionRequestState state;
  AddRequest(RequestType::kNotifications, &state);

  internal::ResolveClapperViaSubscribe(web_contents());

  EXPECT_TRUE(state.granted);
  EXPECT_FALSE(state.cancelled);
  EXPECT_TRUE(state.finished);

  histogram_tester.ExpectBucketCount(
      "Permissions.ClapperLoud.PageInfo.Subscribed", true, 1);
}

TEST_F(AndroidPermissionUtilTest, ResolveClapperViaClose) {
  base::HistogramTester histogram_tester;
  MockPermissionRequest::MockPermissionRequestState state;
  AddRequest(RequestType::kNotifications, &state);

  internal::ResolveClapperViaClose(web_contents());

  EXPECT_FALSE(state.granted);
  EXPECT_FALSE(state.cancelled);
  EXPECT_TRUE(state.finished);

  histogram_tester.ExpectBucketCount("Permissions.ClapperLoud.PageInfo.Closed",
                                     true, 1);
}

TEST_F(AndroidPermissionUtilTest, ResolveClapperViaReset) {
  base::HistogramTester histogram_tester;
  MockPermissionRequest::MockPermissionRequestState state;
  AddRequest(RequestType::kNotifications, &state);

  internal::ResolveClapperViaReset(web_contents());

  // Default triggers Dismiss(), which calls Cancelled().
  EXPECT_TRUE(state.cancelled);
  EXPECT_FALSE(state.granted);
  EXPECT_TRUE(state.finished);

  histogram_tester.ExpectBucketCount("Permissions.ClapperLoud.PageInfo.Reset",
                                     true, 1);
}

TEST_F(AndroidPermissionUtilTest, ResolveClapperViaSubscribe_NoManager) {
  // Pass nullptr as web_contents.
  internal::ResolveClapperViaSubscribe(nullptr);
  // Should not crash.
}

TEST_F(AndroidPermissionUtilTest, ResolveClapperViaSubscribe_NoRequests) {
  internal::ResolveClapperViaSubscribe(web_contents());
  // Should not crash.
}

TEST_F(AndroidPermissionUtilTest, ResolveClapperViaSubscribe_MismatchType) {
  MockPermissionRequest::MockPermissionRequestState state;
  AddRequest(RequestType::kGeolocation, &state);

  // Call with Notifications type.
  internal::ResolveClapperViaSubscribe(web_contents());

  // Request should still be in progress and not decided.
  EXPECT_TRUE(manager_->IsRequestInProgress());
  EXPECT_FALSE(state.granted);
  EXPECT_FALSE(state.finished);
  EXPECT_FALSE(state.cancelled);
}

TEST_F(AndroidPermissionUtilTest, ResolveLoudClapperViaAllow) {
  base::HistogramTester histogram_tester;
  MockPermissionRequest::MockPermissionRequestState state;
  AddRequest(RequestType::kNotifications, &state);

  internal::ResolveLoudClapperViaAllow(web_contents());

  // Should be allowed (granted).
  EXPECT_FALSE(state.cancelled);
  EXPECT_TRUE(state.granted);
  EXPECT_TRUE(state.finished);

  // Should NOT log the Subscribed histogram.
  histogram_tester.ExpectBucketCount(
      "Permissions.ClapperLoud.PageInfo.Subscribed", true, 0);
}

}  // namespace permissions
