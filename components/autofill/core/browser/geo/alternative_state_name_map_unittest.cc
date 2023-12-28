// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace test {

// Tests that map is not empty when an entry has been added to it.
TEST(AlternativeStateNameMapTest, IsEntryAddedToMap) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();
  EXPECT_FALSE(
      AlternativeStateNameMap::GetInstance()->IsLocalisedStateNamesMapEmpty());
}

// Tests that the state canonical name is present when an entry is added to
// the map.
TEST(AlternativeStateNameMapTest, StateCanonicalString) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();
  const char* const kValidMatches[] = {"Bavaria", "BY",  "Bayern", "by",
                                       "BAVARIA", "B.Y", "BAYern", "B-Y"};

  for (const char* valid_match : kValidMatches) {
    SCOPED_TRACE(valid_match);
    EXPECT_NE(AlternativeStateNameMap::GetCanonicalStateName(
                  "DE", base::ASCIIToUTF16(valid_match)),
              std::nullopt);
  }

  EXPECT_EQ(AlternativeStateNameMap::GetCanonicalStateName("US", u"Bavaria"),
            std::nullopt);
  EXPECT_EQ(AlternativeStateNameMap::GetCanonicalStateName("DE", u""),
            std::nullopt);
  EXPECT_EQ(AlternativeStateNameMap::GetCanonicalStateName("", u""),
            std::nullopt);
}

// Tests that the separate entries are created in the map for the different
// country codes.
TEST(AlternativeStateNameMapTest, SeparateEntryForDifferentCounties) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting("DE");
  test::PopulateAlternativeStateNameMapForTesting("US");
  EXPECT_NE(AlternativeStateNameMap::GetCanonicalStateName("DE", u"Bavaria"),
            std::nullopt);
  EXPECT_NE(AlternativeStateNameMap::GetCanonicalStateName("US", u"Bavaria"),
            std::nullopt);
}

// Tests that |AlternativeStateNameMap::NormalizeStateName()| removes "-", " "
// and "." from the text.
TEST(AlternativeStateNameMapTest, StripText) {
  struct {
    const char* test_string;
    const char* expected;
  } test_cases[] = {{"B.Y", "BY"},
                    {"The Golden Sun", "TheGoldenSun"},
                    {"Bavaria - BY", "BavariaBY"}};
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "test_string: " << test_case.test_string
                                    << " | expected: " << test_case.expected);
    AlternativeStateNameMap::StateName text =
        AlternativeStateNameMap::StateName(
            base::ASCIIToUTF16(test_case.test_string));
    EXPECT_EQ(AlternativeStateNameMap::NormalizeStateName(text).value(),
              base::ASCIIToUTF16(test_case.expected));
  }
}

// Tests that the correct entries are returned when the maps in
// AlternativeStateNameMap are queried.
TEST(AlternativeStateNameMapTest, GetEntry) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();
  AlternativeStateNameMap* alternative_state_name_map =
      AlternativeStateNameMap::GetInstance();
  EXPECT_EQ(alternative_state_name_map->GetEntry(
                AlternativeStateNameMap::CountryCode("DE"),
                AlternativeStateNameMap::StateName(u"Random")),
            std::nullopt);
  auto entry = alternative_state_name_map->GetEntry(
      AlternativeStateNameMap::CountryCode("DE"),
      AlternativeStateNameMap::StateName(u"Bavaria"));
  EXPECT_NE(entry, std::nullopt);
  ASSERT_TRUE(entry->has_canonical_name());
  EXPECT_EQ(entry->canonical_name(), "Bavaria");
  EXPECT_THAT(entry->abbreviations(),
              testing::UnorderedElementsAreArray({"BY"}));
  EXPECT_THAT(entry->alternative_names(),
              testing::UnorderedElementsAreArray({"Bayern"}));
}

}  // namespace test
}  // namespace autofill
