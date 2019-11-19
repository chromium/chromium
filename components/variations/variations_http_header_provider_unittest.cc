// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_http_header_provider.h"

#include <string>

#include "base/base64.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/client_variations.pb.h"
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
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::CreateFieldTrial(trial_name, default_group_name));
  EXPECT_TRUE(trial);

  if (trial) {
    AssociateGoogleVariationID(key, trial->trial_name(), trial->group_name(),
                               id);
  }

  return trial;
}

}  // namespace

class VariationsHttpHeaderProviderTest : public ::testing::Test {
 public:
  VariationsHttpHeaderProviderTest() {}

  ~VariationsHttpHeaderProviderTest() override {}

  void TearDown() override { testing::ClearAllVariationIDs(); }
};

TEST_F(VariationsHttpHeaderProviderTest, ForceVariationIds_Valid) {
  base::test::SingleThreadTaskEnvironment task_environment;
  VariationsHttpHeaderProvider provider;

  // Valid experiment ids.
  EXPECT_EQ(VariationsHttpHeaderProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIds({"12", "456", "t789"}, ""));
  provider.InitVariationIDsCacheIfNeeded();
  std::string variations = provider.GetClientDataHeader(false);
  EXPECT_FALSE(variations.empty());
  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(12) != variation_ids.end());
  EXPECT_TRUE(variation_ids.find(456) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(789) != trigger_ids.end());
  EXPECT_FALSE(variation_ids.find(789) != variation_ids.end());
}

TEST_F(VariationsHttpHeaderProviderTest, ForceVariationIds_ValidCommandLine) {
  base::test::SingleThreadTaskEnvironment task_environment;
  VariationsHttpHeaderProvider provider;

  // Valid experiment ids.
  EXPECT_EQ(VariationsHttpHeaderProvider::ForceIdsResult::SUCCESS,
            provider.ForceVariationIds({"12"}, "456,t789"));
  provider.InitVariationIDsCacheIfNeeded();
  std::string variations = provider.GetClientDataHeader(false);
  EXPECT_FALSE(variations.empty());
  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
  EXPECT_TRUE(variation_ids.find(12) != variation_ids.end());
  EXPECT_TRUE(variation_ids.find(456) != variation_ids.end());
  EXPECT_TRUE(trigger_ids.find(789) != trigger_ids.end());
  EXPECT_FALSE(variation_ids.find(789) != variation_ids.end());
}

TEST_F(VariationsHttpHeaderProviderTest, ForceVariationIds_Invalid) {
  base::test::SingleThreadTaskEnvironment task_environment;
  VariationsHttpHeaderProvider provider;

  // Invalid experiment ids.
  EXPECT_EQ(VariationsHttpHeaderProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIds({"abcd12", "456"}, ""));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeader(false).empty());

  // Invalid trigger experiment id
  EXPECT_EQ(VariationsHttpHeaderProvider::ForceIdsResult::INVALID_VECTOR_ENTRY,
            provider.ForceVariationIds({"12", "tabc456"}, ""));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeader(false).empty());

  // Invalid command-line ids.
  EXPECT_EQ(VariationsHttpHeaderProvider::ForceIdsResult::INVALID_SWITCH_ENTRY,
            provider.ForceVariationIds({"12", "50"}, "tabc456"));
  provider.InitVariationIDsCacheIfNeeded();
  EXPECT_TRUE(provider.GetClientDataHeader(false).empty());
}

TEST_F(VariationsHttpHeaderProviderTest, OnFieldTrialGroupFinalized) {
  base::test::SingleThreadTaskEnvironment task_environment;
  VariationsHttpHeaderProvider provider;
  provider.InitVariationIDsCacheIfNeeded();

  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> trial_1(CreateTrialAndAssociateId(
      "t1", default_name, GOOGLE_WEB_PROPERTIES, 123));
  ASSERT_EQ(default_name, trial_1->group_name());

  scoped_refptr<base::FieldTrial> trial_2(CreateTrialAndAssociateId(
      "t2", default_name, GOOGLE_WEB_PROPERTIES_TRIGGER, 456));
  ASSERT_EQ(default_name, trial_2->group_name());

  scoped_refptr<base::FieldTrial> trial_3(CreateTrialAndAssociateId(
      "t3", default_name, GOOGLE_WEB_PROPERTIES_SIGNED_IN, 789));
  ASSERT_EQ(default_name, trial_3->group_name());

  // Run the message loop to make sure OnFieldTrialGroupFinalized is called for
  // the two field trials.
  base::RunLoop().RunUntilIdle();

  // Get non-signed in ids.
  {
    std::string variations = provider.GetClientDataHeader(false);
    std::set<VariationID> variation_ids;
    std::set<VariationID> trigger_ids;
    ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
    EXPECT_EQ(1U, variation_ids.size());
    EXPECT_TRUE(variation_ids.find(123) != variation_ids.end());
    EXPECT_EQ(1U, trigger_ids.size());
    EXPECT_TRUE(trigger_ids.find(456) != trigger_ids.end());
  }

  // Now, get signed-in ids.
  {
    std::string variations = provider.GetClientDataHeader(true);
    std::set<VariationID> variation_ids;
    std::set<VariationID> trigger_ids;
    ASSERT_TRUE(ExtractVariationIds(variations, &variation_ids, &trigger_ids));
    EXPECT_EQ(2U, variation_ids.size());
    EXPECT_TRUE(variation_ids.find(123) != variation_ids.end());
    EXPECT_TRUE(variation_ids.find(789) != variation_ids.end());
    EXPECT_EQ(1U, trigger_ids.size());
    EXPECT_TRUE(trigger_ids.find(456) != trigger_ids.end());
  }
}

TEST_F(VariationsHttpHeaderProviderTest, GetVariationsString) {
  base::test::SingleThreadTaskEnvironment task_environment;

  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES, 123);
  CreateTrialAndAssociateId("t2", "g2", GOOGLE_WEB_PROPERTIES, 124);
  // SIGNED_IN ids shouldn't be included.
  CreateTrialAndAssociateId("t3", "g3", GOOGLE_WEB_PROPERTIES_SIGNED_IN, 125);

  VariationsHttpHeaderProvider provider;
  provider.ForceVariationIds({"100", "200"}, "");
  EXPECT_EQ(" 100 123 124 200 ", provider.GetVariationsString());
}

TEST_F(VariationsHttpHeaderProviderTest, GetVariationsVector) {
  base::test::SingleThreadTaskEnvironment task_environment;
  CreateTrialAndAssociateId("t1", "g1", GOOGLE_WEB_PROPERTIES, 121);
  CreateTrialAndAssociateId("t2", "g2", GOOGLE_WEB_PROPERTIES, 122);
  CreateTrialAndAssociateId("t3", "g3", GOOGLE_WEB_PROPERTIES_TRIGGER, 123);
  CreateTrialAndAssociateId("t4", "g4", GOOGLE_WEB_PROPERTIES_TRIGGER, 124);
  CreateTrialAndAssociateId("t5", "g5", GOOGLE_WEB_PROPERTIES_SIGNED_IN, 125);

  VariationsHttpHeaderProvider provider;
  provider.ForceVariationIds({"100", "200", "t101"}, "");

  EXPECT_EQ((std::vector<VariationID>{100, 121, 122, 200}),
            provider.GetVariationsVector(GOOGLE_WEB_PROPERTIES));
  EXPECT_EQ((std::vector<VariationID>{101, 123, 124}),
            provider.GetVariationsVector(GOOGLE_WEB_PROPERTIES_TRIGGER));
  EXPECT_EQ((std::vector<VariationID>{125}),
            provider.GetVariationsVector(GOOGLE_WEB_PROPERTIES_SIGNED_IN));
}

}  // namespace variations
