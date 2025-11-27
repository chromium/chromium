// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

#include "base/test/scoped_feature_list.h"
#include "components/supervised_user/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

class ContentFiltersObserverBridgeTest : public testing::Test {};

TEST_F(ContentFiltersObserverBridgeTest,
       WithFeatureDisabledCallbacksAreNotCalled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kPropagateDeviceContentFiltersToSupervisedUser);

  ContentFiltersObserverBridge bridge(
      "test_setting", base::BindRepeating([]() {
        CHECK(false) << "Callback called when feature is disabled";
      }),
      base::BindRepeating(
          []() { CHECK(false) << "Callback called when feature is disabled"; }),
      base::BindRepeating([]() { return false; }));

  bridge.Init();
  bridge.Shutdown();
}

}  // namespace
}  // namespace supervised_user
