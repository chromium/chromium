// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

TEST(ClientFilterableStateTest, IsEnterprise) {
  // Test, for non enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_non_enterprise;
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());

  // Test, for enterprise clients, is_enterprise_function_ is called once.
  std::unique_ptr<ClientFilterableState> client_enterprise =
      ClientFilterableState::CreateWithIsEnterprise(
          base::BindOnce([] { return true; }));
  EXPECT_TRUE(client_enterprise->IsEnterprise());
  EXPECT_TRUE(client_enterprise->IsEnterprise());
}

TEST(ClientFilterableStateTest, GoogleGroups) {
  // Test that google_groups_function_ is called once.
  base::flat_set<uint64_t> expected_google_groups({1234, 5678});
  std::unique_ptr<ClientFilterableState> client =
      ClientFilterableState::CreateWithGoogleGroups(base::BindOnce(
          [] { return base::flat_set<uint64_t>({1234, 5678}); }));
  EXPECT_EQ(client->GoogleGroups(), expected_google_groups);
  EXPECT_EQ(client->GoogleGroups(), expected_google_groups);
}

TEST(ClientFilterableStateTest, GetHardwareManufacturer) {
  std::string manufacturer = ClientFilterableState::GetHardwareManufacturer();
#if BUILDFLAG(IS_ANDROID)
  // On Android, the value is not hardcoded, but it should not be empty.
  EXPECT_FALSE(manufacturer.empty());
#else
  // For all other platforms, we expect the empty string fallback.
  EXPECT_TRUE(manufacturer.empty());
#endif
}

TEST(ClientFilterableStateTest, EnterpriseGroups) {
  // Test that enterprise_groups_function_ is called once.
  base::flat_set<std::string> expected_enterprise_groups({"abcd", "efgh"});
  std::unique_ptr<ClientFilterableState> client =
      ClientFilterableState::CreateWithEnterpriseGroups(base::BindOnce(
          [] { return base::flat_set<std::string>({"abcd", "efgh"}); }));
  EXPECT_EQ(client->EnterpriseGroups(), expected_enterprise_groups);
  EXPECT_EQ(client->EnterpriseGroups(), expected_enterprise_groups);
}

}  // namespace
}  // namespace variations
