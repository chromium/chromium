// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
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

  TestingPrefServiceSimple pref_service;

  MockObserver observer;
  EXPECT_CALL(observer, OnContentFiltersObserverEnabled(
                            kBrowserContentFiltersSettingName))
      .Times(0);
  EXPECT_CALL(observer, OnContentFiltersObserverDisabled(
                            kBrowserContentFiltersSettingName))
      .Times(0);

  ContentFiltersObserverBridge bridge(kBrowserContentFiltersSettingName,
                                      &pref_service);

  bridge.AddObserver(&observer);
  bridge.Init();
  bridge.Shutdown();
}

// TODO(crbug.com/469694485): Remove test when filters no longer check parental
// control status.
TEST_F(ContentFiltersObserverBridgeTest,
       ParentalControlsVetoTrueValueButFalseIsPropagated) {
  TestingPrefServiceSimple pref_service;
  RegisterProfilePrefs(pref_service.registry());
  EnableParentalControls(pref_service);

  MockObserver observer;
  EXPECT_CALL(observer, OnContentFiltersObserverEnabled(
                            kBrowserContentFiltersSettingName))
      .Times(0);
  EXPECT_CALL(observer, OnContentFiltersObserverDisabled(
                            kBrowserContentFiltersSettingName))
      .Times(1);

  ContentFiltersObserverBridge bridge(kBrowserContentFiltersSettingName,
                                      &pref_service);

  bridge.AddObserver(&observer);
  // Vetoed, will not yield OnContentFiltersObserverEnabled
  bridge.SetEnabledForTesting(true);
  // Accepted, will yield OnContentFiltersObserverDisabled
  bridge.SetEnabledForTesting(false);
}

// TODO(crbug.com/469694485): Remove test when filters no longer check parental
// control status.
TEST_F(ContentFiltersObserverBridgeTest, RegularUsersAreNotifiedAboutChanges) {
  TestingPrefServiceSimple pref_service;
  RegisterProfilePrefs(pref_service.registry());
  DisableParentalControls(pref_service);

  MockObserver observer;
  EXPECT_CALL(observer, OnContentFiltersObserverEnabled(
                            kBrowserContentFiltersSettingName))
      .Times(1);
  EXPECT_CALL(observer, OnContentFiltersObserverDisabled(
                            kBrowserContentFiltersSettingName))
      .Times(1);

  ContentFiltersObserverBridge bridge(kBrowserContentFiltersSettingName,
                                      &pref_service);

  bridge.AddObserver(&observer);
  // Both settings will trigger notifications.
  bridge.SetEnabledForTesting(true);
  bridge.SetEnabledForTesting(false);
}

TEST_F(ContentFiltersObserverBridgeTest, NotificationsAreSent) {
  TestingPrefServiceSimple pref_service;
  RegisterProfilePrefs(pref_service.registry());
  DisableParentalControls(pref_service);

  MockObserver observer;
  EXPECT_CALL(observer, OnContentFiltersObserverEnabled(
                            kBrowserContentFiltersSettingName))
      .Times(1);
  EXPECT_CALL(observer, OnContentFiltersObserverDisabled(
                            kBrowserContentFiltersSettingName))
      .Times(1);

  ContentFiltersObserverBridge bridge(kBrowserContentFiltersSettingName,
                                      nullptr);

  bridge.AddObserver(&observer);
  // Both settings will trigger notifications.
  bridge.SetEnabledForTesting(true);
  bridge.SetEnabledForTesting(false);
}

}  // namespace
}  // namespace supervised_user
