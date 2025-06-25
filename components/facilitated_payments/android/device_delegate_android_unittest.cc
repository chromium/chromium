// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/device_delegate_android.h"

#include "base/android/application_status_listener.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class DeviceDelegateAndroidTest : public content::RenderViewHostTestHarness {
 public:
  DeviceDelegateAndroidTest() = default;
  ~DeviceDelegateAndroidTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    delegate_ = std::make_unique<DeviceDelegateAndroid>(web_contents());
  }

  void TearDown() override {
    delegate_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<DeviceDelegateAndroid> delegate_;
};

TEST_F(DeviceDelegateAndroidTest,
       ChromeGoesToBackgroundThenForeground_CallbackRun) {
  base::MockCallback<base::OnceClosure> mock_callback;
  delegate_->SetOnReturnToChromeCallback(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run);

  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
}

TEST_F(DeviceDelegateAndroidTest,
       ChromeGoesToForegroundWithoutGoingToBackground_CallbackNotRun) {
  base::MockCallback<base::OnceClosure> mock_callback;
  delegate_->SetOnReturnToChromeCallback(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);

  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
}

TEST_F(DeviceDelegateAndroidTest, ChromeGoesToBackground_CallbackNotRun) {
  base::MockCallback<base::OnceClosure> mock_callback;
  delegate_->SetOnReturnToChromeCallback(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);

  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
}

TEST_F(DeviceDelegateAndroidTest,
       MultipleBackgroundForegroundCycles_CallbackRunOnlyOnce) {
  base::MockCallback<base::OnceClosure> mock_callback;
  delegate_->SetOnReturnToChromeCallback(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(1);

  // First cycle: Background -> Foreground
  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Second cycle: Background -> Foreground
  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
}

TEST_F(
    DeviceDelegateAndroidTest,
    CallbackSetAfterChromeAlreadyInBackground_ThenForeground_CallbackNotRun) {
  base::MockCallback<base::OnceClosure> mock_callback;
  // App goes to background first, then callback is set.
  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  delegate_->SetOnReturnToChromeCallback(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);

  // Then app comes to foreground.
  delegate_->OnApplicationStateChanged(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
}

}  // namespace payments::facilitated
