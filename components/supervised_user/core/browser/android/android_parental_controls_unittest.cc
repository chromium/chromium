// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/android_parental_controls.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

class AndroidParentalControlsTest : public testing::Test {};

class MockObserver : public AndroidParentalControls::Observer {
 public:
  MOCK_METHOD(void, OnSearchContentFiltersEnabled, (), (override));
  MOCK_METHOD(void, OnSearchContentFiltersDisabled, (), (override));
  MOCK_METHOD(void, OnBrowserContentFiltersEnabled, (), (override));
  MOCK_METHOD(void, OnBrowserContentFiltersDisabled, (), (override));
};

TEST(AndroidParentalControlsTest, CheckNotificationsAreSent) {
  AndroidParentalControls android_parental_controls;
  MockObserver observer;

  EXPECT_CALL(observer, OnSearchContentFiltersEnabled()).Times(1);
  EXPECT_CALL(observer, OnSearchContentFiltersDisabled()).Times(1);
  EXPECT_CALL(observer, OnBrowserContentFiltersEnabled()).Times(1);
  EXPECT_CALL(observer, OnBrowserContentFiltersDisabled()).Times(1);

  android_parental_controls.AddObserver(&observer);
  android_parental_controls.SetBrowserContentFiltersEnabledForTesting(true);
  android_parental_controls.SetSearchContentFiltersEnabledForTesting(true);
  android_parental_controls.SetBrowserContentFiltersEnabledForTesting(false);
  android_parental_controls.SetSearchContentFiltersEnabledForTesting(false);
}

}  // namespace
}  // namespace supervised_user
