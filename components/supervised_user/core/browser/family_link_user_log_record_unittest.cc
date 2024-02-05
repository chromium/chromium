// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_user_log_record.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

namespace {
constexpr char kChildEmail[] = "name@gmail.com";
}  // namespace

class FamilyLinkUserLogRecordTest : public ::testing::Test {
 public:
  signin::IdentityTestEnvironment* GetIdentityTestEnv() {
    return &identity_test_env_;
  }

  std::unique_ptr<FamilyLinkUserLogRecord> CreateFamilyLinkUserLogRecord() {
    return std::make_unique<FamilyLinkUserLogRecord>(
        FamilyLinkUserLogRecord::Create(identity_test_env_.identity_manager()));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(FamilyLinkUserLogRecordTest, SignedOutIsUnsupervised) {
  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kUnsupervised);
}

TEST_F(FamilyLinkUserLogRecordTest, CapabilitiesUnknownDefault) {
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_FALSE(supervision_status.has_value());
}

TEST_F(FamilyLinkUserLogRecordTest, SupervisionEnabledByUser) {
  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_is_opted_in_to_parental_supervision(true);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByUser);
}

TEST_F(FamilyLinkUserLogRecordTest, SupervisionEnabledByPolicy) {
  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_is_opted_in_to_parental_supervision(false);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByPolicy);
}

TEST_F(FamilyLinkUserLogRecordTest, NotSupervised) {
  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_is_opted_in_to_parental_supervision(false);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);

  std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
      CreateFamilyLinkUserLogRecord()->GetSupervisionStatusForPrimaryAccount();
  ASSERT_TRUE(supervision_status.has_value());
  ASSERT_EQ(supervision_status.value(),
            FamilyLinkUserLogRecord::Segment::kUnsupervised);
}

}  // namespace supervised_user
