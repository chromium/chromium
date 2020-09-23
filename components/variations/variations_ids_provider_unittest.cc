// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_ids_provider.h"

#include <string>

#include "base/base64.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

// Decodes the variations header and extracts the variation ids.
bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  ClientVariations proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i)
    trigger_ids->insert(proto.trigger_variation_id(i));
  return true;
}

scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id) {
  AssociateGoogleVariationID(key, trial_name, default_group_name, id);
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::CreateFieldTrial(trial_name, default_group_name));
  EXPECT_TRUE(trial);

  if (trial) {
    // Ensure the trial is registered under the correct key so we can look it
    // up.
    trial->group();
  }

  return trial;
}

}  // namespace

class VariationsIdsProviderTest : public ::testing::Test {
 public:
  VariationsIdsProviderTest() {}

  ~VariationsIdsProviderTest() override {}

  void TearDown() override { testing::ClearAllVariationIDs(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(VariationsIdsProviderTest, ForceVariationIds_Valid) {
  VariationsIdsProvider provider;

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
  VariationsIdsProvider provider;

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
  VariationsIdsProvider provider;

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
}

TEST_F(VariationsIdsProviderTest, ForceDisableVariationIds_ValidCommandLine) {
  VariationsIdsProvider provider;

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
  VariationsIdsProvider provider;

  // Invalid command-line ids.
  EXPECT_FALSE(provider.ForceDisableVariationIds("abc"));
  EXPECT_FALSE(provider.ForceDisableVariationIds("tabc456"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeaders(/*is_signed_in=*/false).is_null());
}

TEST_F(VariationsIdsProviderTest, OnFieldTrialGroupFinalized) {
  VariationsIdsProvider provider;
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
      "t4", default_name, GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 44));
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
    const std::string variations =
        headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

    std::set<VariationID> variation_ids;
    std::set<VariationID> trigger_ids;
    ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
    EXPECT_EQ(2U, variation_ids.size());
    EXPECT_TRUE(variation_ids.find(11) != variation_ids.end());
    EXPECT_TRUE(variation_ids.find(22) != variation_ids.end());
    EXPECT_EQ(2U, trigger_ids.size());
    EXPECT_TRUE(trigger_ids.find(33) != trigger_ids.end());
    EXPECT_TRUE(trigger_ids.find(44) != trigger_ids.end());
  }

  // Now, get signed-in ids.
  {
    variations::mojom::VariationsHeadersPtr headers =
        provider.GetClientDataHeaders(/*is_signed_in=*/true);
    const std::string variations =
        headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

    std::set<VariationID> variation_ids;
    std::set<VariationID> trigger_ids;
    ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
    EXPECT_EQ(3U, variation_ids.size());
    EXPECT_TRUE(variation_ids.find(11) != variation_ids.end());
    EXPECT_TRUE(variation_ids.find(22) != variation_ids.end());
    EXPECT_TRUE(variation_ids.find(55) != variation_ids.end());
    EXPECT_EQ(2U, trigger_ids.size());
    EXPECT_TRUE(trigger_ids.find(33) != trigger_ids.end());
    EXPECT_TRUE(trigger_ids.find(44) != trigger_ids.end());
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

  VariationsIdsProvider provider;
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

  VariationsIdsProvider provider;
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

  VariationsIdsProvider provider;
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

  VariationsIdsProvider provider;
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

  VariationsIdsProvider provider;
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
