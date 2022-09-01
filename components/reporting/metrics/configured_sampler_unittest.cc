// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/configured_sampler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/metrics/sampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

TEST(ConfiguredSamplerTest, Default) {
  static constexpr char enable_setting_path[] = "path";
  std::unique_ptr<Sampler> sampler = std::make_unique<test::FakeSampler>();
  auto* const sampler_ptr = sampler.get();

  ConfiguredSampler configured_sampler(std::move(sampler), enable_setting_path,
                                       /*setting_enabled_default_value=*/true);

  EXPECT_THAT(configured_sampler.GetSampler(), testing::Eq(sampler_ptr));
  EXPECT_THAT(configured_sampler.GetEnableSettingPath(),
              testing::StrEq(enable_setting_path));
  EXPECT_TRUE(configured_sampler.GetSettingEnabledDefaultValue());
}

}  // namespace
}  // namespace reporting
