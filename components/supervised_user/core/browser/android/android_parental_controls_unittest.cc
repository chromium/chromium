// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/android_parental_controls.h"

#include "base/callback_list.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

using ::testing::_;

class MockSubscriber {
 public:
  MOCK_METHOD(void,
              OnDeviceParentalChanged,
              (const DeviceParentalControls& state));
};

TEST(AndroidParentalControlsTest, CheckNotificationsAreSent) {
  AndroidParentalControls android_parental_controls;
  MockSubscriber subscriber;

  // Once for subscription, and once for each time the state changes.
  EXPECT_CALL(subscriber, OnDeviceParentalChanged(_)).Times(5);

  base::CallbackListSubscription subscription =
      android_parental_controls.Subscribe(
          base::BindRepeating(&MockSubscriber::OnDeviceParentalChanged,
                              base::Unretained(&subscriber)));

  android_parental_controls.SetBrowserContentFiltersEnabledForTesting(true);
  android_parental_controls.SetSearchContentFiltersEnabledForTesting(true);
  android_parental_controls.SetBrowserContentFiltersEnabledForTesting(false);
  android_parental_controls.SetSearchContentFiltersEnabledForTesting(false);
}

}  // namespace
}  // namespace supervised_user
