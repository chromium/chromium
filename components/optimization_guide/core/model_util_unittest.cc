// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_util.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

proto::ModelCacheKey CreateModelCacheKey(const std::string& locale) {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(locale);
  return model_cache_key;
}

}  // namespace

TEST(ModelUtilTest, ModelCacheKeyHash) {
  EXPECT_EQ(GetModelCacheKeyHash(CreateModelCacheKey("en-US")),
            GetModelCacheKeyHash(CreateModelCacheKey("en-US")));
  EXPECT_NE(GetModelCacheKeyHash(CreateModelCacheKey("en-US")),
            GetModelCacheKeyHash(CreateModelCacheKey("en-UK")));
  EXPECT_TRUE(
      base::ranges::all_of(GetModelCacheKeyHash(CreateModelCacheKey("en-US")),
                           [](char ch) { return base::IsHexDigit(ch); }));
}

TEST(ModelUtilTest, PredictionModelVersionInKillSwitch) {
  const std::map<proto::OptimizationTarget, std::set<int64_t>>
      test_killswitch_model_versions = {
          {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, {1, 3}},
          {proto::OPTIMIZATION_TARGET_MODEL_VALIDATION, {5}},
      };

  EXPECT_FALSE(IsPredictionModelVersionInKillSwitch(
      {}, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 1));

  EXPECT_TRUE(IsPredictionModelVersionInKillSwitch(
      test_killswitch_model_versions,
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 1));
  EXPECT_TRUE(IsPredictionModelVersionInKillSwitch(
      test_killswitch_model_versions,
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 3));
  EXPECT_TRUE(IsPredictionModelVersionInKillSwitch(
      test_killswitch_model_versions,
      proto::OPTIMIZATION_TARGET_MODEL_VALIDATION, 5));
  EXPECT_FALSE(IsPredictionModelVersionInKillSwitch(
      test_killswitch_model_versions,
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 2));
  EXPECT_FALSE(IsPredictionModelVersionInKillSwitch(
      test_killswitch_model_versions,
      proto::OPTIMIZATION_TARGET_MODEL_VALIDATION, 1));
  EXPECT_FALSE(IsPredictionModelVersionInKillSwitch(
      test_killswitch_model_versions, proto::OPTIMIZATION_TARGET_PAGE_TOPICS,
      1));
}

}  // namespace optimization_guide
