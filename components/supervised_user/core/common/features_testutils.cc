// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/features_testutils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user::testing {

// static
::testing::internal::ParamGenerator<LocalWebApprovalsTestCase>
LocalWebApprovalsTestCase::Values() {
  return ::testing::Values(LocalWebApprovalsTestCase(true),
                           LocalWebApprovalsTestCase(false));
}
::testing::internal::ParamGenerator<LocalWebApprovalsTestCase>
LocalWebApprovalsTestCase::OnlySupported() {
  return ::testing::Values(LocalWebApprovalsTestCase(true));
}

LocalWebApprovalsTestCase::LocalWebApprovalsTestCase(
    bool is_local_web_approvals_supported)
    : is_local_web_approvals_supported_(is_local_web_approvals_supported) {}

std::unique_ptr<base::test::ScopedFeatureList>
LocalWebApprovalsTestCase::MakeFeatureList() {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  if (is_local_web_approvals_supported_) {
    feature_list->InitAndEnableFeature(kLocalWebApprovals);
  } else {
    feature_list->InitAndDisableFeature(kLocalWebApprovals);
  }
  return feature_list;
}

std::string LocalWebApprovalsTestCase::ToString() const {
  if (is_local_web_approvals_supported_) {
    return "LocalWebApprovalsSupported";
  } else {
    return "LocalWebApprovalsUnsupported";
  }
}

}  // namespace supervised_user::testing
