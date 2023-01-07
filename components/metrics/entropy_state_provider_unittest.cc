// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/entropy_state_provider.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

class EntropyStateProviderTest : public testing::Test {
 public:
  EntropyStateProviderTest() {
    MetricsService::RegisterPrefs(prefs_.registry());
  }

  EntropyStateProviderTest(const EntropyStateProviderTest&) = delete;
  EntropyStateProviderTest& operator=(const EntropyStateProviderTest&) = delete;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(EntropyStateProviderTest, PopulateAllLowEntropySources) {
  const int new_low_source = 1234;
  const int old_low_source = 5678;
  const int pseudo_low_source = 4321;
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, new_low_source);
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, old_low_source);
  prefs_.SetInteger(prefs::kMetricsPseudoLowEntropySource, pseudo_low_source);

  EntropyStateProvider provider(&prefs_);
  SystemProfileProto system_profile;

  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_EQ(new_low_source, system_profile.low_entropy_source());
  EXPECT_EQ(old_low_source, system_profile.old_low_entropy_source());
  EXPECT_EQ(pseudo_low_source, system_profile.pseudo_low_entropy_source());
}

}  // namespace metrics
