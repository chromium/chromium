// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(ClientFilterableStateTest, IsEnterprise) {
  // Test, for non enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_non_enterprise(
      base::BindOnce([] { return false; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());

  // Test, for enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_enterprise(
      base::BindOnce([] { return true; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  EXPECT_TRUE(client_enterprise.IsEnterprise());
  EXPECT_TRUE(client_enterprise.IsEnterprise());
}

TEST(ClientFilterableStateTest, GoogleGroups) {
  // Test that google_groups_function_ is called once.
  base::flat_set<uint64_t> expected_google_groups =
      base::flat_set<uint64_t>(1234, 5678);
  ClientFilterableState client(
      base::BindOnce([] { return false; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(1234, 5678); }));
  EXPECT_EQ(client.GoogleGroups(), expected_google_groups);
  EXPECT_EQ(client.GoogleGroups(), expected_google_groups);
}

}  // namespace variations
