// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_ids_provider.h"

#include <string>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

using variations::test::ScopedVariationsIdsProvider;

class VariationsIdsProviderTest : public ::testing::Test {
 public:
  VariationsIdsProviderTest() = default;

  ~VariationsIdsProviderTest() override = default;

  void TearDown() override { variations::test::ClearAllVariationIDs(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

}  // namespace

TEST_F(VariationsIdsProviderTest, ForceVariationIds_Valid) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  // Valid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIdsForTesting({"12", "456", "t789"}, ""));
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
  ASSERT_TRUE(headers);
  EXPECT_FALSE(headers->headers_map.empty());
  const std::string variations =
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);
  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(12) != variation_ids.end());
  EXPECT_TRUE(variation_ids.find(456) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(789) != trigger_ids.end());
  EXPECT_FALSE(variation_ids.find(789) != variation_ids.end());
}

TEST_F(VariationsIdsProviderTest, ForceVariationIds_ValidCommandLine) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  // Valid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIdsForTesting({"12"}, "456,t789"));
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
  ASSERT_TRUE(headers);
  EXPECT_FALSE(headers->headers_map.empty());
  const std::string variations =
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(12) != variation_ids.end());
  EXPECT_TRUE(variation_ids.find(456) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(789) != trigger_ids.end());
  EXPECT_FALSE(variation_ids.find(789) != variation_ids.end());
}

TEST_F(VariationsIdsProviderTest, ForceVariationIds_Invalid) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  // Invalid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIdsForTesting({"abcd12", "456"}, ""));
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Invalid trigger experiment id
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIdsForTesting({"12", "tabc456"}, ""));
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Invalid command-line ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY,
            provider.ForceVariationIdsForTesting({"12", "50"}, "tabc456"));
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Duplicate experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIdsForTesting({"1", "2", "t1"}, ""));
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Duplicate command-line ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY,
            provider.ForceVariationIdsForTesting({}, "t10,11,10"));
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Duplicate experiment and command-line ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY,
            provider.ForceVariationIdsForTesting({"20", "t21"}, "21"));
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());
}

TEST_F(VariationsIdsProviderTest, ForceDisableVariationIds_ValidCommandLine) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  // Valid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIdsForTesting({"1", "2", "t3", "t4"}, "5,6,t7,t8"));
  EXPECT_TRUE(provider.ForceDisableVariationIds("2,t4,6,t8"));
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
  ASSERT_TRUE(headers);
  EXPECT_FALSE(headers->headers_map.empty());
  const std::string variations =
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(1) != variation_ids.end());
  EXPECT_FALSE(variation_ids.find(2) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(3) != trigger_ids.end());
  EXPECT_FALSE(trigger_ids.find(4) != trigger_ids.end());
  EXPECT_TRUE(variation_ids.find(5) != variation_ids.end());
  EXPECT_FALSE(variation_ids.find(6) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(7) != trigger_ids.end());
  EXPECT_FALSE(trigger_ids.find(8) != trigger_ids.end());
}

TEST_F(VariationsIdsProviderTest, ForceDisableVariationIds_Invalid) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  // Invalid command-line ids.
  EXPECT_FALSE(provider.ForceDisableVariationIds("abc"));
  EXPECT_FALSE(provider.ForceDisableVariationIds("tabc456"));
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());
}

TEST_F(VariationsIdsProviderTest, LowEntropySourceValue_Valid) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  std::optional<int> valid_low_entropy_source_value = 5;
  provider.SetLowEntropySourceValue(valid_low_entropy_source_value);
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
  ASSERT_TRUE(headers);
  EXPECT_FALSE(headers->headers_map.empty());

  const std::string variations_header_first_party = headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);
  const std::string variations_header_any_context =
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

  std::set<VariationID> variation_ids_first_party;
  std::set<VariationID> trigger_ids_first_party;
  ASSERT_TRUE(ExtractVariationIds(variations_header_first_party,
                                  &variation_ids_first_party,
                                  &trigger_ids_first_party));
  std::set<VariationID> variation_ids_any_context;
  std::set<VariationID> trigger_ids_any_context;
  ASSERT_TRUE(ExtractVariationIds(variations_header_any_context,
                                  &variation_ids_any_context,
                                  &trigger_ids_any_context));

  // 3320983 is the offset value of kLowEntropySourceVariationIdRangeMin + 5.
  EXPECT_TRUE(base::Contains(variation_ids_first_party, 3320983));
  EXPECT_TRUE(base::Contains(variation_ids_any_context, 3320983));
}

TEST_F(VariationsIdsProviderTest, LowEntropySourceValue_Null) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  std::optional<int> null_low_entropy_source_value = std::nullopt;
  provider.SetLowEntropySourceValue(null_low_entropy_source_value);

  // Valid experiment ids.
  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 12);
  CreateTrialAndAssociateId("t2", "g2", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 456);
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
  ASSERT_TRUE(headers);
  EXPECT_FALSE(headers->headers_map.empty());

  const std::string variations_header_first_party = headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);
  const std::string variations_header_any_context =
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

  std::set<VariationID> variation_ids_first_party;
  std::set<VariationID> trigger_ids_first_party;
  ASSERT_TRUE(ExtractVariationIds(variations_header_first_party,
                                  &variation_ids_first_party,
                                  &trigger_ids_first_party));
  std::set<VariationID> variation_ids_any_context;
  std::set<VariationID> trigger_ids_any_context;
  ASSERT_TRUE(ExtractVariationIds(variations_header_any_context,
                                  &variation_ids_any_context,
                                  &trigger_ids_any_context));

  // We test to make sure that only two valid variation IDs are present and that
  // the low entropy source value is not added to the sets.
  EXPECT_TRUE(base::Contains(variation_ids_first_party, 12));
  EXPECT_TRUE(base::Contains(variation_ids_first_party, 456));
  EXPECT_FALSE(base::Contains(variation_ids_first_party, 3320983));
  EXPECT_TRUE(base::Contains(variation_ids_any_context, 12));
  EXPECT_TRUE(base::Contains(variation_ids_any_context, 456));
  EXPECT_FALSE(base::Contains(variation_ids_any_context, 3320983));

  // Check to make sure that no other variation IDs are present.
  EXPECT_EQ(2U, variation_ids_first_party.size());
  EXPECT_EQ(2U, variation_ids_any_context.size());
}

TEST_F(VariationsIdsProviderTest, OnFieldTrialGroupFinalized) {
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> trial_1(CreateTrialAndAssociateId(
      "t1", default_name, GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 11));
  ASSERT_EQ(default_name, trial_1->group_name());

  scoped_refptr<base::FieldTrial> trial_2(CreateTrialAndAssociateId(
      "t2", default_name, GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 22));
  ASSERT_EQ(default_name, trial_2->group_name());

  scoped_refptr<base::FieldTrial> trial_3(CreateTrialAndAssociateId(
      "t3", default_name, GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 33));
  ASSERT_EQ(default_name, trial_3->group_name());

  scoped_refptr<base::FieldTrial> trial_4(CreateTrialAndAssociateId(
      "t4", default_name, GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 44));
  ASSERT_EQ(default_name, trial_4->group_name());

  scoped_refptr<base::FieldTrial> trial_5(CreateTrialAndAssociateId(
      "t5", default_name, GOOGLE_WEB_PROPERTIES_SIGNED_IN, 55));
  ASSERT_EQ(default_name, trial_5->group_name());

  // Run the message loop to make sure OnFieldTrialGroupFinalized is called for
  // the two field trials.
  base::RunLoop().RunUntilIdle();

  // Get non-signed in ids.
  {
    variations::mojom::VariationsHeadersPtr headers =
        provider.GetClientDataHeaders(/*is_signed_in=*/false);
    ASSERT_TRUE(headers);
    const std::string variations_header_first_party = headers->headers_map.at(
        variations::mojom::GoogleWebVisibility::FIRST_PARTY);
    const std::string variations_header_any_context =
        headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

    std::set<VariationID> ids_first_party;
    std::set<VariationID> trigger_ids_first_party;
    ASSERT_TRUE(ExtractVariationIds(variations_header_first_party,
                                    &ids_first_party,
                                    &trigger_ids_first_party));
    std::set<VariationID> ids_any_context;
    std::set<VariationID> trigger_ids_any_context;
    ASSERT_TRUE(ExtractVariationIds(variations_header_any_context,
                                    &ids_any_context,
                                    &trigger_ids_any_context));

    EXPECT_EQ(2U, ids_first_party.size());
    EXPECT_TRUE(base::Contains(ids_first_party, 11));
    EXPECT_TRUE(base::Contains(ids_first_party, 22));
    EXPECT_EQ(2U, trigger_ids_first_party.size());
    EXPECT_TRUE(base::Contains(trigger_ids_first_party, 33));
    EXPECT_TRUE(base::Contains(trigger_ids_first_party, 44));

    // IDs associated with FIRST_PARTY ID CollectionKeys should be excluded from
    // the variations header that may be sent in third-party contexts.
    EXPECT_EQ(1U, ids_any_context.size());
    EXPECT_TRUE(base::Contains(ids_any_context, 11));
    // Note '22' is omitted.
    EXPECT_EQ(1U, trigger_ids_any_context.size());
    EXPECT_TRUE(base::Contains(trigger_ids_any_context, 33));
    // Note '44' is omitted.
  }

  // Now, get signed-in ids.
  {
    variations::mojom::VariationsHeadersPtr headers =
        provider.GetClientDataHeaders(/*is_signed_in=*/true);
    ASSERT_TRUE(headers);
    const std::string variations_header_first_party = headers->headers_map.at(
        variations::mojom::GoogleWebVisibility::FIRST_PARTY);
    const std::string variations_header_any_context =
        headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

    std::set<VariationID> ids_first_party;
    std::set<VariationID> trigger_ids_first_party;
    ASSERT_TRUE(ExtractVariationIds(variations_header_first_party,
                                    &ids_first_party,
                                    &trigger_ids_first_party));
    std::set<VariationID> ids_any_context;
    std::set<VariationID> trigger_ids_any_context;
    ASSERT_TRUE(ExtractVariationIds(variations_header_any_context,
                                    &ids_any_context,
                                    &trigger_ids_any_context));

    EXPECT_EQ(3U, ids_first_party.size());
    EXPECT_TRUE(base::Contains(ids_first_party, 11));
    EXPECT_TRUE(base::Contains(ids_first_party, 22));
    EXPECT_TRUE(base::Contains(ids_any_context, 55));
    EXPECT_EQ(2U, trigger_ids_first_party.size());
    EXPECT_TRUE(base::Contains(trigger_ids_first_party, 33));
    EXPECT_TRUE(base::Contains(trigger_ids_first_party, 44));

    // IDs associated with FIRST_PARTY ID CollectionKeys should be excluded from
    // the variations header that may be sent in third-party contexts.
    EXPECT_EQ(2U, ids_any_context.size());
    EXPECT_TRUE(base::Contains(ids_any_context, 11));
    // Note '22' is omitted.
    EXPECT_TRUE(base::Contains(ids_any_context, 55));
    EXPECT_EQ(1U, trigger_ids_any_context.size());
    EXPECT_TRUE(base::Contains(trigger_ids_any_context, 33));
    // Note '44' is omitted.
  }
}

TEST_F(VariationsIdsProviderTest, GetGoogleAppVariationsString) {
  // No GOOGLE_WEB_PROPERTIES(_X) ids should be included.
  CreateTrialAndAssociateId("t1", "g1",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 121);
  CreateTrialAndAssociateId("t2", "g2",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 122);
  CreateTrialAndAssociateId("t3", "g3", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 123);
  CreateTrialAndAssociateId("t4", "g4", GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 124);
  CreateTrialAndAssociateId("t5", "g5", GOOGLE_WEB_PROPERTIES_SIGNED_IN, 125);

  // GOOGLE_APP ids should be included.
  CreateTrialAndAssociateId("t6", "g6", GOOGLE_APP, 126);

  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  provider.ForceVariationIdsForTesting({"100", "200"}, "");
  EXPECT_EQ(" 126 ", provider.GetGoogleAppVariationsString());
}

TEST_F(VariationsIdsProviderTest, GetVariationsString) {
  // Trigger ids shouldn't be included.
  CreateTrialAndAssociateId("t1", "g1",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 121);
  CreateTrialAndAssociateId("t2", "g2",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 122);

  // These ids should be included.
  CreateTrialAndAssociateId("t3", "g3", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 123);
  CreateTrialAndAssociateId("t4", "g4", GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 124);

  // Signed-in ids shouldn't be included.
  CreateTrialAndAssociateId("t5", "g5", GOOGLE_WEB_PROPERTIES_SIGNED_IN, 125);

  // GOOGLE_APP ids shouldn't be included.
  CreateTrialAndAssociateId("t6", "g6", GOOGLE_APP, 126);

  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  provider.ForceVariationIdsForTesting({"100", "200"}, "");
  EXPECT_EQ(" 100 123 124 200 ", provider.GetVariationsString());
}

TEST_F(VariationsIdsProviderTest, GetVariationsVector) {
  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 121);
  CreateTrialAndAssociateId("t3", "g3", GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 122);
  CreateTrialAndAssociateId("t4", "g4",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 123);
  CreateTrialAndAssociateId("t5", "g5",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 124);
  CreateTrialAndAssociateId("t6", "g6", GOOGLE_WEB_PROPERTIES_SIGNED_IN, 125);
  CreateTrialAndAssociateId("t7", "g7", GOOGLE_APP, 126);

  // Note that the order of the IDs is deterministic, so we can assert on the
  // exact contents of the vector.
  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  provider.ForceVariationIdsForTesting({"100", "200", "t101"}, "");

   // Test Non-Trigger IDS, separately and together.
  EXPECT_EQ((std::vector<VariationID>{100, 121, 200}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{122}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{100, 121, 122, 200}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                          GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));

  // Test Trigger IDS, separately and together.
  EXPECT_EQ((std::vector<VariationID>{101, 123}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{124}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101, 123, 124}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
                 GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));

  // Test Signed-in IDS, GOOGLE_APP IDs, separately.
  EXPECT_EQ((std::vector<VariationID>{125}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_SIGNED_IN}));
  EXPECT_EQ((std::vector<VariationID>{126}),
            provider.GetVariationsVector({GOOGLE_APP}));

  // Test getting everything all at once.
  EXPECT_EQ(
      (std::vector<VariationID>{100, 101, 121, 122, 123, 124, 125, 126, 200}),
      provider.GetVariationsVector(
          {GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, GOOGLE_WEB_PROPERTIES_FIRST_PARTY,
           GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
           GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY,
           GOOGLE_WEB_PROPERTIES_SIGNED_IN, GOOGLE_APP}));
}

TEST_F(VariationsIdsProviderTest, GetTimeboxedVariationsVector) {
  const base::Time day_0 = base::Time::Now();
  const base::Time day_1 = day_0 + base::Days(1);
  const base::Time day_2 = day_0 + base::Days(2);
  const base::Time day_3 = day_0 + base::Days(3);
  const base::Time day_4 = day_0 + base::Days(4);
  const base::Time day_5 = day_0 + base::Days(5);
  const base::Time day_6 = day_0 + base::Days(6);

  CreateTrialAndAssociateId("Day_0_to_4", "g1",
                            GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 333,
                            TimeWindow(day_0, day_4));
  CreateTrialAndAssociateId("Day_2_to_5", "g2",
                            GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 444,
                            TimeWindow(day_2, day_5));
  auto trial = CreateInactiveTrialAndAssociateId(
      "Day_3_to_6", "g3", GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 555,
      TimeWindow(day_3, day_6));

  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  provider.ForceVariationIdsForTesting({"100", "200", "t101"}, "");

  // Day 1: The Day 0 and forced variations ids are active.
  // Note that the order of the IDs is deterministic, so we can assert on the
  // exact contents of the vector.
  scoped_provider.time_for_testing = day_1;
  EXPECT_EQ((std::vector<VariationID>{100, 200, 333}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));

  // Day 2: The Day 2 study enters its time box, 444 is added to the first-party
  // context. The Day 0 study is still active, so it's not removed.
  scoped_provider.time_for_testing = day_2 + base::Hours(1);
  EXPECT_EQ((std::vector<VariationID>{100, 200, 333}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{444}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));

  // Day 3: We still haven't activated the Day 3 study, so nothing changes even
  // though the time is inside the time box.
  scoped_provider.time_for_testing = day_3 + base::Hours(1);
  EXPECT_EQ((std::vector<VariationID>{100, 200, 333}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{444}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));

  // If we activate the Day 3 study, 555 should be added to the trigger-any
  // context. The rest remains the same.
  trial->Activate();
  EXPECT_EQ((std::vector<VariationID>{100, 200, 333}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{444}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101, 555}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));

  // Day 4: The Day 0 study exits its time box, so it's removed from the any
  // context. The rest remains the same.
  scoped_provider.time_for_testing = day_4 + base::Hours(1);
  EXPECT_EQ((std::vector<VariationID>{100, 200}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{444}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101, 555}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));
}

TEST_F(VariationsIdsProviderTest, GetVariationsVectorForWebPropertiesKeys) {
  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 121);
  CreateTrialAndAssociateId("t2", "g2", GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 122);
  CreateTrialAndAssociateId("t3", "g3",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 123);
  CreateTrialAndAssociateId("t4", "g4",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 124);
  CreateTrialAndAssociateId("t5", "g5", GOOGLE_WEB_PROPERTIES_SIGNED_IN, 125);

  // GOOGLE_APP ids shouldn't be included.
  CreateTrialAndAssociateId("t6", "g6", GOOGLE_APP, 126);

  ScopedVariationsIdsProvider scoped_provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  auto& provider = *scoped_provider;

  provider.ForceVariationIdsForTesting({"100", "t101"}, "");
  EXPECT_EQ((std::vector<VariationID>{100, 101, 121, 122, 123, 124, 125}),
            provider.GetVariationsVectorForWebPropertiesKeys());
}

}  // namespace variations
