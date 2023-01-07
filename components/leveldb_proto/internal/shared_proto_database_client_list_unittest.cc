// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {


class SharedProtoDatabaseClientListTest : public testing::Test {
 public:
  void SetUpExperiment(bool isExperimentOn) {
    scoped_feature_list_.InitWithFeatureState(kProtoDBSharedMigration,
                                              isExperimentOn);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SharedProtoDatabaseClientListTest, ShouldUseSharedDBTest) {
  // Set experiment to use Shared DB.
  SetUpExperiment(true);

  bool use_shared = SharedProtoDatabaseClientList::ShouldUseSharedDB(
      ProtoDbType::TEST_DATABASE1);

  ASSERT_TRUE(use_shared);
}

TEST_F(SharedProtoDatabaseClientListTest,
       ShouldUseSharedDBTest_OnlyWhenExperimentIsOn) {
  // Set experiment to use Unique DB.
  SetUpExperiment(false);

  bool use_shared = SharedProtoDatabaseClientList::ShouldUseSharedDB(
      ProtoDbType::TEST_DATABASE1);

  ASSERT_FALSE(use_shared);
}

TEST_F(SharedProtoDatabaseClientListTest,
       ShouldUseSharedDBTest_ExceptIfDbIsBlocklisted) {
  SetUpExperiment(true);

  // GCM_KEY_STORE is blocklisted, it won't use a shared DB, regardless of
  // experiment state.
  bool use_shared = SharedProtoDatabaseClientList::ShouldUseSharedDB(
      ProtoDbType::GCM_KEY_STORE);

  ASSERT_FALSE(use_shared);
}

}  // namespace leveldb_proto
