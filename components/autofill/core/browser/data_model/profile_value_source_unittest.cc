// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/profile_value_source.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

const char kGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kGuidB[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";

TEST(ProfileValueSource, Equality) {
  EXPECT_EQ(ProfileValueSource({kGuidA, NAME_FIRST}),
            ProfileValueSource({kGuidA, NAME_FIRST}));
  EXPECT_NE(ProfileValueSource({kGuidA, NAME_FIRST}),
            ProfileValueSource({kGuidB, NAME_FIRST}));
  EXPECT_NE(ProfileValueSource({kGuidA, NAME_FIRST}),
            ProfileValueSource({kGuidB, ADDRESS_HOME_STREET_NAME}));
}

TEST(PossibleProfileValueSources, PossibleValueSources_AddingSources) {
  PossibleProfileValueSources sources;

  // Initially, there should be no possible source
  ASSERT_TRUE(sources.GetAllPossibleValueSources().empty());

  // Once we added a value this should change.
  sources.AddPossibleValueSource(kGuidA, NAME_FULL);
  EXPECT_FALSE(sources.GetAllPossibleValueSources().empty());

  EXPECT_THAT(sources.GetAllPossibleValueSources(),
              testing::ElementsAre(ProfileValueSource{kGuidA, NAME_FULL}));
}

TEST(PossibleProfileValueSources, PossibleValueSources_AddingMultipleSources) {
  PossibleProfileValueSources sources;

  // Initially, there should be no possible source
  ASSERT_TRUE(sources.GetAllPossibleValueSources().empty());

  // Once we added a value this should change.
  sources.AddPossibleValueSource(kGuidA, NAME_FULL);
  sources.AddPossibleValueSource(kGuidB, NAME_FIRST);

  // We should be able to retrieve both sources.
  EXPECT_THAT(sources.GetAllPossibleValueSources(),
              testing::ElementsAre(ProfileValueSource{kGuidA, NAME_FULL},
                                   ProfileValueSource{kGuidB, NAME_FIRST}));
}

TEST(PossibleProfileValueSources, PossibleValueSources_IgnoreNonAddressTypes) {
  PossibleProfileValueSources sources;

  // Initially, there should be no possible source
  ASSERT_TRUE(sources.GetAllPossibleValueSources().empty());

  // Add a non-address type and make sure it is not added.
  sources.AddPossibleValueSource(kGuidA, CREDIT_CARD_NAME_FULL);
  EXPECT_TRUE(sources.GetAllPossibleValueSources().empty());

  // And make sure address types are accepted
  sources.AddPossibleValueSource(kGuidA, NAME_FULL);
  EXPECT_FALSE(sources.GetAllPossibleValueSources().empty());
}

TEST(PossibleProfileValueSources, PossibleValueSources_Clear) {
  PossibleProfileValueSources sources;

  // Initially, there should be no possible source
  ASSERT_TRUE(sources.GetAllPossibleValueSources().empty());

  // Add an address and make sure there is data stored.
  sources.AddPossibleValueSource(kGuidA, NAME_FULL);
  EXPECT_FALSE(sources.GetAllPossibleValueSources().empty());

  // Clear the value sources and verify that it actually becomes empty.
  sources.ClearAllPossibleValueSources();
  EXPECT_TRUE(sources.GetAllPossibleValueSources().empty());
}

}  // namespace
}  // namespace autofill
