// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_sync_util.h"

#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/sync/protocol/entity_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

TEST(PlusAddressSyncUtilTest, ProfileToEntityDataAndBack) {
  PlusProfile profile = test::CreatePlusProfile();
  syncer::EntityData entity_data = EntityDataFromPlusProfile(profile);
  EXPECT_EQ(PlusProfileFromEntityData(entity_data), profile);
}

}  // namespace

}  // namespace plus_addresses
