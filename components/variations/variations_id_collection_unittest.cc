// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_id_collection.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

const char kTrial1[] = "TrialNameOne";
const char kTrial2[] = "TrialNameTwo";
const char kTrial3[] = "TrialNameThree";
const char kGroup[] = "GroupName";
const VariationID kVariation1 = 111;
const VariationID kVariation2 = 222;
const VariationID kVariation3 = 333;

// Awkward helper functions to allow initializer lists inside macros.
std::set<VariationID> Set(std::set<VariationID> set) {
  return set;
}
std::vector<VariationID> Vec(std::vector<VariationID> vec) {
  return vec;
}

void SetupTrial(const std::string& trial_name,
                IDCollectionKey key,
                VariationID id) {
  AssociateGoogleVariationID(key, trial_name, kGroup, id);
  base::FieldTrialList::CreateFieldTrial(trial_name, kGroup);
}

void FinalizeTrial(const std::string& trial_name) {
  EXPECT_EQ(kGroup, base::FieldTrialList::FindFullName(trial_name));
  base::RunLoop().RunUntilIdle();
}

void SetupAndFinalizeTrial(const std::string& trial_name,
                           IDCollectionKey key,
                           VariationID id) {
  SetupTrial(trial_name, key, id);
  FinalizeTrial(trial_name);
}

}  // namespace

class VariationsIdCollectionTest : public ::testing::Test {
 public:
  VariationsIdCollectionTest() {}

  ~VariationsIdCollectionTest() override { testing::ClearAllVariationIDs(); }

  void OnNewId(VariationID new_id) { new_ids_.push_back(new_id); }

  void ResetCollection(IDCollectionKey key) {
    collection_ = std::make_unique<VariationsIdCollection>(
        key, base::BindRepeating(&VariationsIdCollectionTest::OnNewId,
                                 base::Unretained(this)));
  }

  const std::vector<VariationID>& GetNewIds() { return new_ids_; }

  VariationsIdCollection* collection() { return collection_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<VariationsIdCollection> collection_;
  std::vector<VariationID> new_ids_;
};

TEST_F(VariationsIdCollectionTest, VariousSetupAndFinalization) {
  SetupAndFinalizeTrial(kTrial1, GOOGLE_WEB_PROPERTIES, kVariation1);
  SetupTrial(kTrial2, GOOGLE_WEB_PROPERTIES, kVariation2);
  ResetCollection(GOOGLE_WEB_PROPERTIES);

  EXPECT_EQ(Set({kVariation1}), collection()->GetIds());
  EXPECT_EQ(Vec({}), GetNewIds());

  FinalizeTrial(kTrial2);
  SetupAndFinalizeTrial(kTrial3, GOOGLE_WEB_PROPERTIES, kVariation3);

  EXPECT_EQ(Set({kVariation1, kVariation2, kVariation3}),
            collection()->GetIds());
  EXPECT_EQ(Vec({kVariation2, kVariation3}), GetNewIds());
}

TEST_F(VariationsIdCollectionTest, VariousKeys) {
  SetupAndFinalizeTrial(kTrial1, GOOGLE_WEB_PROPERTIES, kVariation1);
  SetupAndFinalizeTrial(kTrial2, GOOGLE_WEB_PROPERTIES_SIGNED_IN, kVariation2);
  SetupAndFinalizeTrial(kTrial3, GOOGLE_WEB_PROPERTIES_TRIGGER, kVariation3);
  ResetCollection(GOOGLE_WEB_PROPERTIES_SIGNED_IN);

  EXPECT_EQ(Set({kVariation2}), collection()->GetIds());
  EXPECT_EQ(Vec({}), GetNewIds());
}

TEST_F(VariationsIdCollectionTest, MultipleFinalization) {
  ResetCollection(GOOGLE_WEB_PROPERTIES);
  collection()->OnFieldTrialGroupFinalized(kTrial1, kGroup);
  EXPECT_EQ(Set({}), collection()->GetIds());
  EXPECT_EQ(Vec({}), GetNewIds());

  // Even though OnFieldTrialGroupFinalized is called, the VariationID lookup
  // should still fail and it should be gracefully handled.
  SetupTrial(kTrial2, GOOGLE_WEB_PROPERTIES, kVariation1);
  collection()->OnFieldTrialGroupFinalized(kTrial1, kGroup);
  EXPECT_EQ(Set({}), collection()->GetIds());
  EXPECT_EQ(Vec({}), GetNewIds());

  FinalizeTrial(kTrial2);
  EXPECT_EQ(Set({kVariation1}), collection()->GetIds());
  EXPECT_EQ(Vec({kVariation1}), GetNewIds());

  // This shouldn't create any duplicates.
  collection()->OnFieldTrialGroupFinalized(kTrial1, kGroup);
  EXPECT_EQ(Set({kVariation1}), collection()->GetIds());
  EXPECT_EQ(Vec({kVariation1}), GetNewIds());
}

}  // namespace variations
