// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/address_on_typing/address_on_typing_manager.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"
#include "components/strike_database/test_inmemory_strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AddressOnTypingManagerTest : public testing::Test {
 public:
  AddressOnTypingManagerTest() {
    test_strike_database_ =
        std::make_unique<strike_database::TestInMemoryStrikeDatabase>();
    strike_database_ =
        std::make_unique<AddressOnTypingSuggestionStrikeDatabase>(
            test_strike_database_.get());
    manager_ = std::make_unique<AddressOnTypingManager>(strike_database_.get());
  }

  AddressOnTypingManager& manager() { return *manager_; }

  AddressOnTypingSuggestionStrikeDatabase& strike_database() {
    return *strike_database_;
  }

  void KillAndRecreateManager() {
    manager_.reset();
    manager_ = std::make_unique<AddressOnTypingManager>(strike_database_.get());
  }

 private:
  std::unique_ptr<strike_database::TestInMemoryStrikeDatabase>
      test_strike_database_;
  std::unique_ptr<AddressOnTypingSuggestionStrikeDatabase> strike_database_;
  std::unique_ptr<AddressOnTypingManager> manager_;
};

// Tests that suggestions that were not accepted have their types included to
// the strike database, conversely accepting a suggestion removes them.
TEST_F(AddressOnTypingManagerTest, AcceptingSuggestion_ClearRespectiveTypes) {
  manager().OnDidShowAddressOnTyping(
      {EMAIL_ADDRESS, NAME_FULL, ADDRESS_HOME_LINE1});
  // Accept the suggestion built using `ADDRESS_HOME_LINE1`.
  manager().OnDidAcceptAddressOnTyping(ADDRESS_HOME_LINE1);

  // Strikes are added at destruction time.
  KillAndRecreateManager();
  EXPECT_EQ(strike_database().GetStrikes(base::NumberToString(EMAIL_ADDRESS)),
            1);
  EXPECT_EQ(strike_database().GetStrikes(base::NumberToString(NAME_FULL)), 1);
  // Note the suggestion built using `ADDRESS_HOME_LINE1` was accepted.
  EXPECT_EQ(
      strike_database().GetStrikes(base::NumberToString(ADDRESS_HOME_LINE1)),
      0);

  // Now accept an `EMAIL_ADDRESS` suggestion, which should clear the strike
  // database for it.
  manager().OnDidShowAddressOnTyping({EMAIL_ADDRESS});
  manager().OnDidAcceptAddressOnTyping(EMAIL_ADDRESS);
  EXPECT_EQ(strike_database().GetStrikes(base::NumberToString(EMAIL_ADDRESS)),
            0);
}

TEST_F(AddressOnTypingManagerTest, StrikeLimitReached_MetricEmitted) {
  base::HistogramTester histogram_tester;

  // Show and decline suggestions enough times.
  for (int i = 0; i < strike_database().GetMaxStrikesLimit() - 1; i++) {
    // Show a suggestion for EMAIL_ADDRESS, but don't accept it.
    manager().OnDidShowAddressOnTyping({EMAIL_ADDRESS});
    KillAndRecreateManager();
    histogram_tester.ExpectBucketCount(
        "Autofill.AddressSuggestionOnTypingFieldTypeAddedToStrikeDatabase",
        EMAIL_ADDRESS, 0);
  }

  manager().OnDidShowAddressOnTyping({EMAIL_ADDRESS});
  KillAndRecreateManager();
  histogram_tester.ExpectBucketCount(
      "Autofill.AddressSuggestionOnTypingFieldTypeAddedToStrikeDatabase",
      EMAIL_ADDRESS, 1);
}

}  // namespace autofill
