// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/device_delegate_android.h"

#include "base/android/application_status_listener.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "components/facilitated_payments/android/device_delegate_android_test_api.h"
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
  DeviceDelegateAndroid* delegate() { return delegate_.get(); }
  DeviceDelegateAndroidTestApi test_api() {
    return DeviceDelegateAndroidTestApi(delegate_.get());
  }

 private:
  std::unique_ptr<DeviceDelegateAndroid> delegate_;
};

TEST_F(DeviceDelegateAndroidTest, AppStatusListenerNullByDefault) {
  EXPECT_FALSE(test_api().app_status_listener());
}

TEST_F(DeviceDelegateAndroidTest,
       SetOnReturnToChromeCallbackAndObserveAppState_AppStatusListenerNotNull) {
  delegate()->SetOnReturnToChromeCallbackAndObserveAppState(base::DoNothing());

  EXPECT_TRUE(test_api().app_status_listener());
}

TEST_F(DeviceDelegateAndroidTest, ChromeMovedToForeground_CallbackNotRun) {
  base::MockCallback<base::OnceClosure> mock_callback;
  delegate()->SetOnReturnToChromeCallbackAndObserveAppState(
      mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);

  base::RunLoop run_loop;
  test_api().SetOnApplicationStateChangedCallbackForTesting(
      run_loop.QuitClosure());
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  run_loop.Run();
}

// The current activity can be paused by actions like opening the Settings page.
// Activity resumes on going back to the original page. This should not trigger
// the callback as the user has not left Chrome.
TEST_F(DeviceDelegateAndroidTest,
       ChromeActivityPausedAndResumed_CallbackNotRun) {
  base::MockCallback<base::OnceClosure> mock_callback;
  delegate()->SetOnReturnToChromeCallbackAndObserveAppState(
      mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);

  base::RunLoop run_loop;
  test_api().SetOnApplicationStateChangedCallbackForTesting(
      run_loop.QuitWhenIdleClosure());
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::ApplicationState::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  run_loop.Run();
}

TEST_F(DeviceDelegateAndroidTest,
       ChromeMovedToBackgroundThenForeground_CallbackRun) {
  base::MockCallback<base::OnceClosure> mock_callback;
  delegate()->SetOnReturnToChromeCallbackAndObserveAppState(
      mock_callback.Get());

  EXPECT_CALL(mock_callback, Run);

  base::RunLoop run_loop;
  test_api().SetOnApplicationStateChangedCallbackForTesting(
      run_loop.QuitWhenIdleClosure());
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  run_loop.Run();
}

TEST_F(DeviceDelegateAndroidTest,
       ChromeMovedToBackgroundThenForeground_AppStatusListenerReset) {
  delegate()->SetOnReturnToChromeCallbackAndObserveAppState(base::DoNothing());
  ASSERT_TRUE(test_api().app_status_listener());

  base::RunLoop run_loop;
  test_api().SetOnApplicationStateChangedCallbackForTesting(
      run_loop.QuitWhenIdleClosure());
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  run_loop.Run();

  EXPECT_FALSE(test_api().app_status_listener());
}

}  // namespace payments::facilitated
