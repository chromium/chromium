// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

#include "base/test/scoped_feature_list.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

class ContentFiltersObserverBridgeTest : public testing::Test {};

class MockObserver : public ContentFiltersObserverBridge::Observer {
 public:
  MOCK_METHOD(void,
              OnContentFiltersObserverEnabled,
              (std::string_view),
              (override));
  MOCK_METHOD(void,
              OnContentFiltersObserverDisabled,
              (std::string_view),
              (override));
};

TEST_F(ContentFiltersObserverBridgeTest,
       WithFeatureDisabledCallbacksAreNotCalled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kPropagateDeviceContentFiltersToSupervisedUser);

  MockObserver observer;
  EXPECT_CALL(observer, OnContentFiltersObserverEnabled(
                            kBrowserContentFiltersSettingName))
      .Times(0);
  EXPECT_CALL(observer, OnContentFiltersObserverDisabled(
                            kBrowserContentFiltersSettingName))
      .Times(0);

  ContentFiltersObserverBridge bridge(kBrowserContentFiltersSettingName);

  bridge.AddObserver(&observer);
  bridge.Init();
  bridge.Shutdown();
}

TEST_F(ContentFiltersObserverBridgeTest, NotificationsAreSent) {
  MockObserver observer;
  EXPECT_CALL(observer, OnContentFiltersObserverEnabled(
                            kBrowserContentFiltersSettingName))
      .Times(1);
  EXPECT_CALL(observer, OnContentFiltersObserverDisabled(
                            kBrowserContentFiltersSettingName))
      .Times(1);

  ContentFiltersObserverBridge bridge(kBrowserContentFiltersSettingName);

  bridge.AddObserver(&observer);
  // Both settings will trigger notifications.
  bridge.SetEnabledForTesting(true);
  bridge.SetEnabledForTesting(false);
}

}  // namespace
}  // namespace supervised_user
