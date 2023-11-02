// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/cloud_reporting_policy_handler.h"

#include "build/build_config.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";
constexpr char kFakeBrowserClientId[] = "fake-browser-client-id";
constexpr char kFakeBrowserDMToken[] = "fake-browser-dm-token";

class CloudReportingPolicyHandlerTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  // Returns true if successfully enrolled.
  bool EnrollWithChromeBrowserCloudManagement() {
    browser_dm_token_storage_ =
        std::make_unique<policy::FakeBrowserDMTokenStorage>();
    browser_dm_token_storage_->SetClientId(kFakeBrowserClientId);
    if (!IsTestingMachineEnrolledState()) {
      return false;
    }
    browser_dm_token_storage_->SetEnrollmentToken(kFakeEnrollmentToken);
    browser_dm_token_storage_->EnableStorage(true);
    browser_dm_token_storage_->SetDMToken(kFakeBrowserDMToken);
    return true;
  }

 private:
  bool IsTestingMachineEnrolledState() const { return GetParam(); }

  std::unique_ptr<policy::FakeBrowserDMTokenStorage> browser_dm_token_storage_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(MachineEnrolledOrNot,
                         CloudReportingPolicyHandlerTest,
                         testing::Bool());

TEST_P(CloudReportingPolicyHandlerTest, MachineEnrollment) {
#if !BUILDFLAG(IS_CHROMEOS)
  // CBCM device enrollment is not for chromeos.
  bool enrolled = EnrollWithChromeBrowserCloudManagement();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kCloudReportingEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(static_cast<bool>(true)), nullptr);
  CloudReportingPolicyHandler handler;
  policy::PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(errors.empty());
#else
  EXPECT_EQ(errors.empty(), enrolled);
#endif  // BUILDFLAG(IS_CHROMEOS)
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  bool enabled = false;
  EXPECT_TRUE(prefs.GetBoolean(kCloudReportingEnabled, &enabled));
  EXPECT_EQ(enabled, true);
}

}  // namespace enterprise_reporting
