// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_util.h"

#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class ModelQualityUtilTest : public testing::Test {
 public:
  void SetUp() override {
    model_execution::prefs::RegisterLocalStatePrefs(pref_service_.registry());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple pref_service_;
};

TEST_F(ModelQualityUtilTest, GetModelQualityClientId) {
  int64_t compose_client_id = GetOrCreateModelQualityClientId(
      UserVisibleFeatureKey::kCompose, &pref_service_);
  int64_t wallpaper_search_client_id = GetOrCreateModelQualityClientId(
      UserVisibleFeatureKey::kWallpaperSearch, &pref_service_);
  int64_t tab_organization_client_id = GetOrCreateModelQualityClientId(
      UserVisibleFeatureKey::kTabOrganization, &pref_service_);
  EXPECT_NE(compose_client_id, wallpaper_search_client_id);
  EXPECT_NE(wallpaper_search_client_id, tab_organization_client_id);
  EXPECT_NE(tab_organization_client_id, compose_client_id);

  // Advance clock by more than one day to check that the client ids are
  // different.
  task_environment_.AdvanceClock(base::Days(2));
  int64_t new_compose_client_id = GetOrCreateModelQualityClientId(
      UserVisibleFeatureKey::kCompose, &pref_service_);
  EXPECT_NE(compose_client_id, new_compose_client_id);
}

TEST_F(ModelQualityUtilTest, GetModelExecutionFeature) {
  EXPECT_EQ(
      UserVisibleFeatureKey::kCompose,
      GetModelExecutionFeature(proto::LogAiDataRequest::FeatureCase::kCompose));
  EXPECT_EQ(UserVisibleFeatureKey::kTabOrganization,
            GetModelExecutionFeature(
                proto::LogAiDataRequest::FeatureCase::kTabOrganization));
  EXPECT_EQ(UserVisibleFeatureKey::kWallpaperSearch,
            GetModelExecutionFeature(
                proto::LogAiDataRequest::FeatureCase::kWallpaperSearch));
}

}  // namespace optimization_guide
