// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_ids_provider.h"

#include <string>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
class VariationsIdsProviderTest : public ::testing::Test {
 public:
  VariationsIdsProviderTest() = default;

  ~VariationsIdsProviderTest() override = default;

  void TearDown() override { testing::ClearAllVariationIDs(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(VariationsIdsProviderTest, ForceVariationIds_Valid) {
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);

  // Valid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIds({"12", "456", "t789"}, ""));
  provider.InitVariationIDsCacheIfNeeded();
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
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
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);

  // Valid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIds({"12"}, "456,t789"));
  provider.InitVariationIDsCacheIfNeeded();
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
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
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);

  // Invalid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIds({"abcd12", "456"}, ""));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Invalid trigger experiment id
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIds({"12", "tabc456"}, ""));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Invalid command-line ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY,
            provider.ForceVariationIds({"12", "50"}, "tabc456"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Duplicate experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIds({"1", "2", "t1"}, ""));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Duplicate command-line ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY,
            provider.ForceVariationIds({}, "t10,11,10"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());

  // Duplicate experiment and command-line ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY,
            provider.ForceVariationIds({"20", "t21"}, "21"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());
}

TEST_F(VariationsIdsProviderTest, ForceDisableVariationIds_ValidCommandLine) {
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);

  // Valid experiment ids.
  EXPECT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIds({"1", "2", "t3", "t4"}, "5,6,t7,t8"));
  EXPECT_TRUE(provider.ForceDisableVariationIds("2,t4,6,t8"));
  provider.InitVariationIDsCacheIfNeeded();
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
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
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);

  // Invalid command-line ids.
  EXPECT_FALSE(provider.ForceDisableVariationIds("abc"));
  EXPECT_FALSE(provider.ForceDisableVariationIds("tabc456"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());
}

TEST_F(VariationsIdsProviderTest, LowEntropySourceValue_Valid) {
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);

  std::optional<int> valid_low_entropy_source_value = 5;
  provider.SetLowEntropySourceValue(valid_low_entropy_source_value);
  provider.InitVariationIDsCacheIfNeeded();
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
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
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);

  std::optional<int> null_low_entropy_source_value = std::nullopt;
  provider.SetLowEntropySourceValue(null_low_entropy_source_value);

  // Valid experiment ids.
  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 12);
  CreateTrialAndAssociateId("t2", "g2", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 456);
  provider.InitVariationIDsCacheIfNeeded();
  variations::mojom::VariationsHeadersPtr headers =
      provider.GetClientDataHeaders(/*is_signed_in=*/false);
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
  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  provider.InitVariationIDsCacheIfNeeded();

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

  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  provider.ForceVariationIds({"100", "200"}, "");
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

  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  provider.ForceVariationIds({"100", "200"}, "");
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

  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  provider.ForceVariationIds({"100", "200", "t101"}, "");

  EXPECT_EQ((std::vector<VariationID>{100, 121, 200}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{122}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101, 123}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT}));
  EXPECT_EQ((std::vector<VariationID>{124}),
            provider.GetVariationsVector(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{125}),
            provider.GetVariationsVector({GOOGLE_WEB_PROPERTIES_SIGNED_IN}));
  EXPECT_EQ((std::vector<VariationID>{126}),
            provider.GetVariationsVector({GOOGLE_APP}));
  EXPECT_EQ(
      (std::vector<VariationID>{100, 101, 121, 122, 123, 124, 125, 126, 200}),
      provider.GetVariationsVector(
          {GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, GOOGLE_WEB_PROPERTIES_FIRST_PARTY,
           GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
           GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY,
           GOOGLE_WEB_PROPERTIES_SIGNED_IN, GOOGLE_APP}));
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

  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  provider.ForceVariationIds({"100", "t101"}, "");
  EXPECT_EQ((std::vector<VariationID>{100, 101, 121, 122, 123, 124, 125}),
            provider.GetVariationsVectorForWebPropertiesKeys());
}

TEST_F(VariationsIdsProviderTest, GetVariationsVectorImpl) {
  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 121);
  CreateTrialAndAssociateId("t2", "g2", GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 122);
  CreateTrialAndAssociateId("t3", "g3",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 123);
  CreateTrialAndAssociateId("t4", "g4",
                            GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 124);
  CreateTrialAndAssociateId("t5", "g5", GOOGLE_WEB_PROPERTIES_SIGNED_IN, 125);
  CreateTrialAndAssociateId("t6", "g6", GOOGLE_APP, 125);  // Duplicate.

  VariationsIdsProvider provider(
      VariationsIdsProvider::Mode::kUseSignedInState);
  provider.ForceVariationIds({"100", "200", "t101"}, "");

  EXPECT_EQ(
      (std::vector<VariationID>{100, 121, 122, 200}),
      provider.GetVariationsVectorImpl({GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                        GOOGLE_WEB_PROPERTIES_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{101, 123, 124}),
            provider.GetVariationsVectorImpl(
                {GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
                 GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY}));
  EXPECT_EQ((std::vector<VariationID>{125}),
            provider.GetVariationsVectorImpl(
                {GOOGLE_WEB_PROPERTIES_SIGNED_IN, GOOGLE_APP}));
}

}  // namespace variations
