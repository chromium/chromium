// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_blocklist_data.h"

#include "components/plus_addresses/blocked_facets.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

class PlusAddressBlocklistDataTest : public testing::Test {
 protected:
  PlusAddressBlocklistDataTest() = default;
  ~PlusAddressBlocklistDataTest() override = default;

 protected:
  PlusAddressBlocklistData blocklist_data_;
};

TEST_F(PlusAddressBlocklistDataTest, EmptyProto) {
  ASSERT_FALSE(blocklist_data_.PopulateDataFromComponent(std::string()));
  EXPECT_THAT(blocklist_data_.GetExclusionPattern(), testing::IsNull());
  EXPECT_THAT(blocklist_data_.GetExceptionPattern(), testing::IsNull());
}

TEST_F(PlusAddressBlocklistDataTest, BadProto) {
  ASSERT_FALSE(blocklist_data_.PopulateDataFromComponent("rrr"));
  EXPECT_THAT(blocklist_data_.GetExclusionPattern(), testing::IsNull());
  EXPECT_THAT(blocklist_data_.GetExceptionPattern(), testing::IsNull());
}

TEST_F(PlusAddressBlocklistDataTest, ParsingSuccessful) {
  CompactPlusAddressBlockedFacets blocked_facets;
  blocked_facets.set_exclusion_pattern("foo");
  ASSERT_TRUE(blocklist_data_.PopulateDataFromComponent(
      blocked_facets.SerializeAsString()));

  blocked_facets.set_exception_pattern("bar");
  ASSERT_TRUE(blocklist_data_.PopulateDataFromComponent(
      blocked_facets.SerializeAsString()));

  EXPECT_EQ(blocklist_data_.GetExclusionPattern()->pattern(), "foo");
  EXPECT_EQ(blocklist_data_.GetExceptionPattern()->pattern(), "bar");
}

}  // namespace plus_addresses
