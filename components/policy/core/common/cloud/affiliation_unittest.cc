// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/affiliation.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kAffiliationId1[] = "abc";
constexpr char kAffiliationId2[] = "def";
}  // namespace

TEST(CloudManagementAffiliationTest, Affiliated) {
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);
  user_ids.insert(kAffiliationId2);

  base::flat_set<std::string> device_ids;
  device_ids.insert(kAffiliationId1);

  EXPECT_TRUE(policy::IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, Unaffiliated) {
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);

  base::flat_set<std::string> device_ids;
  user_ids.insert(kAffiliationId2);

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, UserIdsEmpty) {
  base::flat_set<std::string> user_ids;
  base::flat_set<std::string> device_ids;
  user_ids.insert(kAffiliationId1);

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, DeviceIdsEmpty) {
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);
  base::flat_set<std::string> device_ids;

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, BothIdsEmpty) {
  base::flat_set<std::string> user_ids;
  base::flat_set<std::string> device_ids;

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

}  // namespace policy
