// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_FEATURES_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_FEATURES_H_

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "testing/gtest/include/gtest/gtest.h"

// Helper class to override the feature for the duration of the test suite and
// combine it with the param interface.
template <typename TestCase>
class WithFeatureOverrideAndParamInterface
    : public testing::WithParamInterface<std::tuple<bool, TestCase>> {
 public:
  using testing::WithParamInterface<std::tuple<bool, TestCase>>::GetParam;

  explicit WithFeatureOverrideAndParamInterface(const base::Feature& feature) {
    scoped_feature_list_.InitWithFeatureState(feature, IsFeatureEnabled());
  }

  static bool IsFeatureEnabled() { return std::get<0>(GetParam()); }
  static const TestCase& GetTestCase() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_FEATURES_H_
