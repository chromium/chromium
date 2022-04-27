// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"

#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::PasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTrackerImpl;

namespace {

void RegisterPasswordChangeSuccessTrackerPreferences(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      password_manager::prefs::kPasswordChangeSuccessTrackerVersion, 0);
  registry->RegisterListPref(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
}

}  // namespace

TEST(PasswordChangeSuccessTrackerImpl, DeletedOutdatedEventRecords) {
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  RegisterPasswordChangeSuccessTrackerPreferences(pref_service_.registry());

  // Set an outdated version that contains flows.
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordChangeSuccessTrackerVersion, 0);

  base::Value::List flows;
  flows.Append(base::Value::Dict());
  flows.Append(base::Value::Dict());
  pref_service_.SetList(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows,
      std::move(flows));

  const base::Value* value = pref_service_.Get(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->GetList().size(), 2u);

  std::unique_ptr<PasswordChangeSuccessTracker>
      password_change_success_tracker_ =
          std::make_unique<PasswordChangeSuccessTrackerImpl>(&pref_service_);

  // Version has been updated and old records deleted.
  absl::optional<int> version = pref_service_.GetInteger(
      password_manager::prefs::kPasswordChangeSuccessTrackerVersion);
  ASSERT_TRUE(version);
  EXPECT_EQ(version.value(), PasswordChangeSuccessTrackerImpl::kTrackerVersion);

  value = pref_service_.Get(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->GetList().size(), 0u);
}
