// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_TESTUTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_TESTUTILS_H_

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user::testing {

// Defines possible space of valid states of this test use case, and for each
// state configures the features.
class LocalWebApprovalsTestCase {
 public:
  // Enumerates possible values of the test case.
  static ::testing::internal::ParamGenerator<LocalWebApprovalsTestCase>
  Values();
  static ::testing::internal::ParamGenerator<LocalWebApprovalsTestCase>
  OnlySupported();

  explicit LocalWebApprovalsTestCase(bool is_local_web_approvals_supported);

  // Constructs the FeatureList to be held by test fixture. ScopedFeatureList is
  // not copyable nor assignable.
  std::unique_ptr<base::test::ScopedFeatureList> MakeFeatureList();

  // Stringifies the test name as requested (but doesn't indicate effective
  // value).
  std::string ToString() const;
  explicit operator std::string() const { return ToString(); }

 private:
  bool is_local_web_approvals_supported_;
};
}  // namespace supervised_user::testing

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_TESTUTILS_H_
