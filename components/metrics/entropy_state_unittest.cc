// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/entropy_state.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class EntropyStateTest : public testing::Test {
 public:
  EntropyStateTest() { MetricsService::RegisterPrefs(prefs_.registry()); }

  EntropyStateTest(const EntropyStateTest&) = delete;
  EntropyStateTest& operator=(const EntropyStateTest&) = delete;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(EntropyStateTest, LowEntropySourceNotReset) {
  EntropyState entropy_state(&prefs_);
  // Get the low entropy source once, to initialize it.
  entropy_state.GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  entropy_state.low_entropy_source_ = 0;
  EXPECT_EQ(0, entropy_state.GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, entropy_state.GetLowEntropySource());
}

TEST_F(EntropyStateTest, PseudoLowEntropySourceNotReset) {
  EntropyState entropy_state(&prefs_);
  // Get the pseudo low entropy source once, to initialize it.
  entropy_state.GetPseudoLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  entropy_state.pseudo_low_entropy_source_ = 0;
  EXPECT_EQ(0, entropy_state.GetPseudoLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, entropy_state.GetPseudoLowEntropySource());
}

TEST_F(EntropyStateTest, HaveNoLowEntropySource) {
  EntropyState entropy_state(&prefs_);
  // If we have none of the new, old, or pseudo low entropy sources in prefs,
  // then the new source should be created...
  int new_low_source = entropy_state.GetLowEntropySource();
  EXPECT_TRUE(EntropyState::IsValidLowEntropySource(new_low_source))
      << new_low_source;
  int pseudo_low_source = entropy_state.GetPseudoLowEntropySource();
  EXPECT_TRUE(EntropyState::IsValidLowEntropySource(pseudo_low_source))
      << pseudo_low_source;
  // ...but the old source should not...
  EXPECT_EQ(EntropyState::kLowEntropySourceNotSet,
            entropy_state.GetOldLowEntropySource());
  // ...and the high entropy source should include the *new* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(new_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, HaveOnlyNewLowEntropySource) {
  // If we have the new low entropy sources in prefs, but not the old one...
  const int new_low_source = 1234;
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, new_low_source);

  EntropyState entropy_state(&prefs_);
  // ...then the new source should be loaded...
  EXPECT_EQ(new_low_source, entropy_state.GetLowEntropySource());
  // ...but the old source should not be created...
  EXPECT_EQ(EntropyState::kLowEntropySourceNotSet,
            entropy_state.GetOldLowEntropySource());
  // ...and the high entropy source should include the *new* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(new_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, HaveOnlyOldLowEntropySource) {
  // If we have the old low entropy sources in prefs, but not the new one...
  const int old_low_source = 5678;
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, old_low_source);

  // ...then the new source should be created...
  EntropyState entropy_state(&prefs_);

  int new_low_source = entropy_state.GetLowEntropySource();
  EXPECT_TRUE(EntropyState::IsValidLowEntropySource(new_low_source))
      << new_low_source;
  // ...and the old source should be loaded...
  EXPECT_EQ(old_low_source, entropy_state.GetOldLowEntropySource());
  // ...and the high entropy source should include the *old* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(old_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, HaveAllLowEntropySources) {
  // If we have all three of new, old, and pseudo low entropy sources in
  // prefs...
  const int new_low_source = 1234;
  const int old_low_source = 5678;
  const int pseudo_low_source = 4321;
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, new_low_source);
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, old_low_source);
  prefs_.SetInteger(prefs::kMetricsPseudoLowEntropySource, pseudo_low_source);

  // ...then all three should be loaded...
  EntropyState entropy_state(&prefs_);

  EXPECT_EQ(new_low_source, entropy_state.GetLowEntropySource());
  EXPECT_EQ(old_low_source, entropy_state.GetOldLowEntropySource());
  EXPECT_EQ(pseudo_low_source, entropy_state.GetPseudoLowEntropySource());
  // ...and the high entropy source should include the *old* low entropy source.
  std::string high_source = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  EXPECT_TRUE(base::EndsWith(high_source, base::NumberToString(old_low_source),
                             base::CompareCase::SENSITIVE))
      << high_source;
}

TEST_F(EntropyStateTest, CorruptNewLowEntropySources) {
  EntropyState entropy_state(&prefs_);
  const int corrupt_sources[] = {-12345, -1, 8000, 12345};
  for (int corrupt_source : corrupt_sources) {
    // If the new low entropy source has been corrupted...
    EXPECT_FALSE(EntropyState::IsValidLowEntropySource(corrupt_source))
        << corrupt_source;
    prefs_.SetInteger(prefs::kMetricsLowEntropySource, corrupt_source);
    // ...then a new source should be created.
    int loaded_source = entropy_state.GetLowEntropySource();
    EXPECT_TRUE(EntropyState::IsValidLowEntropySource(loaded_source))
        << loaded_source;
  }
}

TEST_F(EntropyStateTest, CorruptOldLowEntropySources) {
  EntropyState entropy_state(&prefs_);
  const int corrupt_sources[] = {-12345, -1, 8000, 12345};
  for (int corrupt_source : corrupt_sources) {
    // If the old low entropy source has been corrupted...
    EXPECT_FALSE(EntropyState::IsValidLowEntropySource(corrupt_source))
        << corrupt_source;
    prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, corrupt_source);
    // ...then it should be ignored.
    EXPECT_EQ(EntropyState::kLowEntropySourceNotSet,
              entropy_state.GetOldLowEntropySource());
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(EntropyStateTest, ClearPrefs) {
  // On Lacros we expect that there will be no clearing of prefs.
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, 1234);
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, 5678);
  prefs_.SetInteger(prefs::kMetricsPseudoLowEntropySource, 4321);

  EntropyState::ClearPrefs(&prefs_);

  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kMetricsLowEntropySource));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kMetricsOldLowEntropySource));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kMetricsPseudoLowEntropySource));
}

TEST_F(EntropyStateTest, SetExternalPrefs) {
  prefs_.ClearPref(prefs::kMetricsLowEntropySource);
  prefs_.ClearPref(prefs::kMetricsOldLowEntropySource);
  prefs_.ClearPref(prefs::kMetricsPseudoLowEntropySource);

  EntropyState::SetExternalPrefs(&prefs_, 1234, 4567, 3456);

  EXPECT_EQ(prefs_.GetInteger(prefs::kMetricsLowEntropySource), 1234);
  EXPECT_EQ(prefs_.GetInteger(prefs::kMetricsOldLowEntropySource), 4567);
  EXPECT_EQ(prefs_.GetInteger(prefs::kMetricsPseudoLowEntropySource), 3456);
}

#else

TEST_F(EntropyStateTest, ClearPrefs) {
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, 1234);
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, 5678);
  prefs_.SetInteger(prefs::kMetricsPseudoLowEntropySource, 4321);

  EntropyState::ClearPrefs(&prefs_);

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kMetricsLowEntropySource));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kMetricsOldLowEntropySource));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kMetricsPseudoLowEntropySource));
}
#endif

}  // namespace metrics
