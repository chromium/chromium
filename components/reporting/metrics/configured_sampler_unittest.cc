// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/configured_sampler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/sampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

TEST(ConfiguredSamplerTest, Default) {
  base::test::SingleThreadTaskEnvironment task_environment;

  static constexpr char enable_setting_path[] = "path";
  std::unique_ptr<Sampler> sampler = std::make_unique<test::FakeSampler>();
  auto* const sampler_ptr = sampler.get();
  test::FakeReportingSettings reporting_settings;

  ConfiguredSampler configured_sampler(std::move(sampler), enable_setting_path,
                                       /*setting_enabled_default_value=*/true,
                                       &reporting_settings);

  EXPECT_THAT(configured_sampler.GetSampler(), testing::Eq(sampler_ptr));
  EXPECT_THAT(configured_sampler.GetEnableSettingPath(),
              testing::StrEq(enable_setting_path));
  EXPECT_TRUE(configured_sampler.GetSettingEnabledDefaultValue());

  // Setting path does not exist, reporting enabled should be
  // `setting_enabled_default_value`.
  EXPECT_TRUE(configured_sampler.IsReportingEnabled());

  reporting_settings.SetBoolean(enable_setting_path, false);
  reporting_settings.SetIsTrusted(false);
  // Setting is set but settings are not trusted, reporting enabled should be
  // `setting_enabled_default_value`.
  EXPECT_TRUE(configured_sampler.IsReportingEnabled());

  reporting_settings.SetIsTrusted(true);
  // Setting is set and trusted, reporting enabled should be the setting actual
  // value.
  EXPECT_FALSE(configured_sampler.IsReportingEnabled());
  reporting_settings.SetBoolean(enable_setting_path, true);
  EXPECT_TRUE(configured_sampler.IsReportingEnabled());
}

}  // namespace
}  // namespace reporting
