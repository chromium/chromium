// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

class ContentFiltersObserverBridgeTest : public testing::Test {};

class MockObserver : public ContentFiltersObserverBridge::Observer {
 public:
  MOCK_METHOD(void, OnContentFiltersObserverChanged, (), (override));
};

TEST_F(ContentFiltersObserverBridgeTest, NotificationsAreSent) {
  MockObserver observer;
  EXPECT_CALL(observer, OnContentFiltersObserverChanged()).Times(2);

  ContentFiltersObserverBridge bridge(kBrowserContentFiltersSettingName);

  bridge.AddObserver(&observer);
  // Both settings will trigger notifications.
  bridge.SetEnabledForTesting(true);
  bridge.SetEnabledForTesting(false);
}

}  // namespace
}  // namespace supervised_user
