// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/ui/form_factor_metrics_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {
namespace {

constexpr SystemProfileProto::Hardware::FormFactor kFormFactor =
    SystemProfileProto::Hardware::FORM_FACTOR_DESKTOP;

class TestFormFactorMetricsProvider : public FormFactorMetricsProvider {
 public:
  TestFormFactorMetricsProvider() = default;

  TestFormFactorMetricsProvider(const TestFormFactorMetricsProvider&) = delete;
  TestFormFactorMetricsProvider& operator=(
      const TestFormFactorMetricsProvider&) = delete;

  ~TestFormFactorMetricsProvider() override = default;

 private:
  SystemProfileProto::Hardware::FormFactor GetFormFactor() const override {
    return kFormFactor;
  }
};

}  // namespace

TEST(FormFactorMetricsProviderTest, ProvideSystemProfileMetrics) {
  TestFormFactorMetricsProvider provider;
  SystemProfileProto system_profile;

  provider.ProvideSystemProfileMetrics(&system_profile);

  // Verify that the system profile has the form factor set.
  const SystemProfileProto::Hardware& hardware = system_profile.hardware();
  ASSERT_TRUE(hardware.has_form_factor());
  EXPECT_EQ(kFormFactor, hardware.form_factor());
}

}  // namespace metrics
