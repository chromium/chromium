// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_test_util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

using DeskTestUtilTest = testing::Test;

TEST_F(DeskTestUtilTest, PopulateRegistryCacheHasAppInfo) {
  auto cache = std::make_unique<apps::AppRegistryCache>();
  desk_test_util::PopulateAppRegistryCache(
      AccountId::FromUserEmail("test@gmail.com"), cache.get());
  EXPECT_EQ(10ul, cache->GetAllApps().size());
}

TEST_F(DeskTestUtilTest, AddOneAppIdToRegistryCacheHasAppInfo) {
  AccountId account_id = AccountId::FromUserEmail("test@gmail.com");
  auto cache = std::make_unique<apps::AppRegistryCache>();
  desk_test_util::PopulateAppRegistryCache(account_id, cache.get());
  desk_test_util::AddAppIdToAppRegistryCache(account_id, cache.get(), "test");
  EXPECT_EQ(11ul, cache->GetAllApps().size());
}

}  // namespace desks_storage
