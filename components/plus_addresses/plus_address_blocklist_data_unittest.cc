// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_blocklist_data.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/plus_addresses/blocked_facets.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

constexpr char kUmaKeyResponseParsingResult[] =
    "PlusAddresses.Blocklist.ParsingResult";

class PlusAddressBlocklistDataTest : public testing::Test {
 public:
  PlusAddressBlocklistDataTest() = default;
  ~PlusAddressBlocklistDataTest() override = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  PlusAddressBlocklistData blocklist_data_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PlusAddressBlocklistDataTest, EmptyProto) {
  ASSERT_FALSE(blocklist_data_.PopulateDataFromComponent(std::string()));
  EXPECT_THAT(blocklist_data_.GetExclusionPattern(), testing::IsNull());
  EXPECT_THAT(blocklist_data_.GetExceptionPattern(), testing::IsNull());
  histogram_tester().ExpectUniqueSample(
      kUmaKeyResponseParsingResult,
      PlusAddressBlocklistDataParsingResult::kEmptyResponse, 1u);
}

TEST_F(PlusAddressBlocklistDataTest, BadProto) {
  ASSERT_FALSE(blocklist_data_.PopulateDataFromComponent("rrr"));
  EXPECT_THAT(blocklist_data_.GetExclusionPattern(), testing::IsNull());
  EXPECT_THAT(blocklist_data_.GetExceptionPattern(), testing::IsNull());
  histogram_tester().ExpectUniqueSample(
      kUmaKeyResponseParsingResult,
      PlusAddressBlocklistDataParsingResult::kParsingError, 1u);
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
  histogram_tester().ExpectUniqueSample(
      kUmaKeyResponseParsingResult,
      PlusAddressBlocklistDataParsingResult::kSuccess, 2u);
}

}  // namespace
}  // namespace plus_addresses
