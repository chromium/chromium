// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/memory_ablation_study.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
const base::Feature kCrosMemoryAblationStudy{"CrosMemoryAblationStudy",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace

class CrosMemoryAblationStudy : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list;
};

// Tests basic functionality of the MemoryAblationStudy class.
TEST_F(CrosMemoryAblationStudy, Basic) {
  // Ablate 137MB.
  base::FieldTrialParams params;
  params["ablation-size-mb"] = "137";
  feature_list.InitAndEnableFeatureWithParameters(kCrosMemoryAblationStudy,
                                                  params);

  // 450s should be enough to both allocate the memory and trigger a read.
  MemoryAblationStudy study;
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(450));
  size_t total_size = 0;
  for (MemoryAblationStudy::Region& region : study.regions_) {
    total_size += region.size();
  }
  EXPECT_EQ(total_size, 137 * 1024 * 1024u);
}

}  // namespace chromeos
