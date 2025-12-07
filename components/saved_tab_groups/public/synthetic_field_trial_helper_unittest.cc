// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/synthetic_field_trial_helper.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {

class SyntheticFieldTrialHelperTest : public testing::Test {
 public:
  SyntheticFieldTrialHelperTest() = default;
  ~SyntheticFieldTrialHelperTest() override = default;

  void SetUp() override {
    synthetic_field_trial_helper_ = std::make_unique<SyntheticFieldTrialHelper>(
        base::BindRepeating(
            &SyntheticFieldTrialHelperTest::OnHadSavedTabGroupChanged,
            base::Unretained(this)),
        base::BindRepeating(
            &SyntheticFieldTrialHelperTest::OnHadSharedTabGroupChanged,
            base::Unretained(this)));
  }

 protected:
  void OnHadSavedTabGroupChanged(bool had_saved_tab_group) {
    on_had_saved_tab_group_changed_count_++;
    had_saved_tab_group_ = had_saved_tab_group;
  }

  void OnHadSharedTabGroupChanged(bool had_shared_tab_group) {
    on_had_shared_tab_group_changed_count_++;
    had_shared_tab_group_ = had_shared_tab_group;
  }

  int on_had_saved_tab_group_changed_count_ = 0;
  int on_had_shared_tab_group_changed_count_ = 0;
  bool had_saved_tab_group_ = false;
  bool had_shared_tab_group_ = false;

  std::unique_ptr<SyntheticFieldTrialHelper> synthetic_field_trial_helper_;
};

TEST_F(SyntheticFieldTrialHelperTest, UpdateHadSavedTabGroupIfNeeded) {
  synthetic_field_trial_helper_->UpdateHadSavedTabGroupIfNeeded(false);
  EXPECT_FALSE(had_saved_tab_group_);
  EXPECT_EQ(on_had_saved_tab_group_changed_count_, 1);

  synthetic_field_trial_helper_->UpdateHadSavedTabGroupIfNeeded(false);
  EXPECT_FALSE(had_saved_tab_group_);
  EXPECT_EQ(on_had_saved_tab_group_changed_count_, 1);

  synthetic_field_trial_helper_->UpdateHadSavedTabGroupIfNeeded(true);
  EXPECT_TRUE(had_saved_tab_group_);
  EXPECT_EQ(on_had_saved_tab_group_changed_count_, 2);

  synthetic_field_trial_helper_->UpdateHadSavedTabGroupIfNeeded(false);
  EXPECT_TRUE(had_saved_tab_group_);
  EXPECT_EQ(on_had_saved_tab_group_changed_count_, 2);

  synthetic_field_trial_helper_->UpdateHadSavedTabGroupIfNeeded(true);
  EXPECT_TRUE(had_saved_tab_group_);
  EXPECT_EQ(on_had_saved_tab_group_changed_count_, 2);
}

TEST_F(SyntheticFieldTrialHelperTest, UpdateHadSharedTabGroupIfNeeded) {
  synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(false);
  EXPECT_FALSE(had_shared_tab_group_);
  EXPECT_EQ(on_had_shared_tab_group_changed_count_, 1);

  synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(false);
  EXPECT_FALSE(had_shared_tab_group_);
  EXPECT_EQ(on_had_shared_tab_group_changed_count_, 1);

  synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(true);
  EXPECT_TRUE(had_shared_tab_group_);
  EXPECT_EQ(on_had_shared_tab_group_changed_count_, 2);

  synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(false);
  EXPECT_TRUE(had_shared_tab_group_);
  EXPECT_EQ(on_had_shared_tab_group_changed_count_, 2);

  synthetic_field_trial_helper_->UpdateHadSharedTabGroupIfNeeded(true);
  EXPECT_TRUE(had_shared_tab_group_);
  EXPECT_EQ(on_had_shared_tab_group_changed_count_, 2);
}
}  // namespace
}  // namespace tab_groups
