// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_table.h"

#include "base/files/scoped_temp_dir.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

class PlusAddressTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.GetPath().AppendASCII("TestDB")));
  }

  base::ScopedTempDir temp_dir_;
  PlusAddressTable table_;
  WebDatabase db_;
};

TEST_F(PlusAddressTableTest, GetPlusProfiles) {
  const PlusProfile profile1 = test::GetPlusProfile();
  const PlusProfile profile2 = test::GetPlusProfile2();
  ASSERT_TRUE(table_.AddPlusProfile(profile1));
  ASSERT_TRUE(table_.AddPlusProfile(profile2));
  EXPECT_THAT(table_.GetPlusProfiles(),
              testing::UnorderedElementsAre(profile1, profile2));
}

TEST_F(PlusAddressTableTest, AddPlusProfile) {
  const PlusProfile profile = test::GetPlusProfile();
  EXPECT_TRUE(table_.AddPlusProfile(profile));
  EXPECT_FALSE(table_.AddPlusProfile(profile));
  EXPECT_TRUE(table_.AddPlusProfile(test::GetPlusProfile2()));
}

TEST_F(PlusAddressTableTest, ClearPlusProfiles) {
  ASSERT_TRUE(table_.AddPlusProfile(test::GetPlusProfile()));
  ASSERT_TRUE(table_.AddPlusProfile(test::GetPlusProfile2()));
  ASSERT_EQ(table_.GetPlusProfiles().size(), 2u);
  EXPECT_TRUE(table_.ClearPlusProfiles());
  EXPECT_TRUE(table_.GetPlusProfiles().empty());
}

}  // namespace

}  // namespace plus_addresses
