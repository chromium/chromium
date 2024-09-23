// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/entropy_state.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
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
  // If we have none of the new, old, or pseudo low entropy sources stored in
  // prefs, then the new source should be created...
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
  // If we have the new low entropy sources stored in prefs, but not the old
  // one...
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
  // If we have the old low entropy sources stored in prefs, but not the new
  // one...
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
  prefs_.SetString(prefs::kMetricsLimitedEntropyRandomizationSource,
                   "00000000000000000000000000000001");

  EntropyState::ClearPrefs(&prefs_);

  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kMetricsLowEntropySource));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kMetricsOldLowEntropySource));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kMetricsPseudoLowEntropySource));
  EXPECT_TRUE(
      prefs_.HasPrefPath(prefs::kMetricsLimitedEntropyRandomizationSource));
}

TEST_F(EntropyStateTest, SetExternalPrefs) {
  prefs_.ClearPref(prefs::kMetricsLowEntropySource);
  prefs_.ClearPref(prefs::kMetricsOldLowEntropySource);
  prefs_.ClearPref(prefs::kMetricsPseudoLowEntropySource);
  prefs_.ClearPref(prefs::kMetricsLimitedEntropyRandomizationSource);

  std::string limited_entropy_randomization_source =
      "00000000000000000000000000000001";
  EntropyState::SetExternalPrefs(&prefs_, 1234, 4567, 3456,
                                 limited_entropy_randomization_source);

  EXPECT_EQ(prefs_.GetInteger(prefs::kMetricsLowEntropySource), 1234);
  EXPECT_EQ(prefs_.GetInteger(prefs::kMetricsOldLowEntropySource), 4567);
  EXPECT_EQ(prefs_.GetInteger(prefs::kMetricsPseudoLowEntropySource), 3456);
  EXPECT_EQ(prefs_.GetString(prefs::kMetricsLimitedEntropyRandomizationSource),
            limited_entropy_randomization_source);
}

TEST_F(EntropyStateTest, SetEmptyStringToLimitedEntropyRandomizationSource) {
  prefs_.ClearPref(prefs::kMetricsLimitedEntropyRandomizationSource);

  EntropyState::SetExternalPrefs(&prefs_, 1234, 4567, 3456, std::string_view());

  EXPECT_FALSE(
      prefs_.HasPrefPath(prefs::kMetricsLimitedEntropyRandomizationSource));
}

#else

TEST_F(EntropyStateTest, ClearPrefs) {
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, 1234);
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, 5678);
  prefs_.SetInteger(prefs::kMetricsPseudoLowEntropySource, 4321);
  prefs_.SetString(prefs::kMetricsLimitedEntropyRandomizationSource,
                   "00000000000000000000000000000001");

  EntropyState::ClearPrefs(&prefs_);

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kMetricsLowEntropySource));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kMetricsOldLowEntropySource));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kMetricsPseudoLowEntropySource));
  EXPECT_FALSE(
      prefs_.HasPrefPath(prefs::kMetricsLimitedEntropyRandomizationSource));
}
#endif

TEST_F(EntropyStateTest, ClearingPrefWillNotResetValuesDuringSession) {
  // Setting test values in prefs;
  prefs_.SetInteger(prefs::kMetricsLowEntropySource, 1234);
  prefs_.SetInteger(prefs::kMetricsOldLowEntropySource, 5678);
  prefs_.SetInteger(prefs::kMetricsPseudoLowEntropySource, 4321);
  prefs_.SetString(prefs::kMetricsLimitedEntropyRandomizationSource,
                   "00000000000000000000000000000001");
  EntropyState entropy_state(&prefs_);

  // Generates all managed values. This should load the values from prefs.
  auto low = entropy_state.GetLowEntropySource();
  auto old_low = entropy_state.GetOldLowEntropySource();
  auto pseudo_low = entropy_state.GetPseudoLowEntropySource();
  auto high = entropy_state.GetHighEntropySource(
      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF");
  auto limited = entropy_state.GetLimitedEntropyRandomizationSource();

  EntropyState::ClearPrefs(&prefs_);

  // Clearing values in prefs should not result in returning different values.
  EXPECT_EQ(low, entropy_state.GetLowEntropySource());
  EXPECT_EQ(old_low, entropy_state.GetOldLowEntropySource());
  EXPECT_EQ(pseudo_low, entropy_state.GetPseudoLowEntropySource());
  EXPECT_EQ(high, entropy_state.GetHighEntropySource(
                      "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF"));
  EXPECT_EQ(limited, entropy_state.GetLimitedEntropyRandomizationSource());
}

TEST_F(EntropyStateTest,
       GenerateLimitedEntropyRandomizationSourceWhenNotAvailable) {
  // Pref for limited entropy randomization source is unset.
  EXPECT_FALSE(
      prefs_.HasPrefPath(prefs::kMetricsLimitedEntropyRandomizationSource));

  // Generate a new limited entropy randomization source.
  EntropyState entropy_state(&prefs_);
  entropy_state.GetLimitedEntropyRandomizationSource();

  // There should be a generated value and it should be stored in prefs.
  auto getter_value = entropy_state.GetLimitedEntropyRandomizationSource();
  auto pref_value =
      prefs_.GetString(prefs::kMetricsLimitedEntropyRandomizationSource);
  EXPECT_NE("", getter_value);
  EXPECT_EQ(getter_value, pref_value);
}

TEST_F(EntropyStateTest, LoadLimitedEntropyRandomizationSourceFromPref) {
  // There is a previously generated limited entropy randomization source stored
  // in prefs.
  auto* test_value = "00000000000000000000000000000001";
  prefs_.SetString(prefs::kMetricsLimitedEntropyRandomizationSource,
                   test_value);

  // This should load the previous value from prefs.
  EntropyState entropy_state(&prefs_);
  entropy_state.GetLimitedEntropyRandomizationSource();

  // Verify that the previous value was returned.
  EXPECT_EQ(test_value, entropy_state.GetLimitedEntropyRandomizationSource());
  // Verify that the value stored in prefs is not altered.
  EXPECT_EQ(test_value,
            prefs_.GetString(prefs::kMetricsLimitedEntropyRandomizationSource));
}

TEST_F(EntropyStateTest, LimitedEntropyRandomizationSourceNotReset) {
  EntropyState entropy_state(&prefs_);

  // Attempts to generate the limited entropy randomization source twice.
  auto first_call_value = entropy_state.GetLimitedEntropyRandomizationSource();
  auto second_call_value = entropy_state.GetLimitedEntropyRandomizationSource();

  // The generated value should not be empty.
  EXPECT_NE("", first_call_value);
  // The values returned from the two calls should be identical.
  EXPECT_EQ(first_call_value, second_call_value);
}

TEST_F(EntropyStateTest, ResetLimitedEntropyRandomizationSourceThroughCmdLine) {
  // Setup a command line flag to reset the variations state.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kResetVariationState);
  // ...and store a previously generated limited entropy randomization source
  // value in prefs.
  auto* test_value = "00000000000000000000000000000001";
  prefs_.SetString(prefs::kMetricsLimitedEntropyRandomizationSource,
                   test_value);

  // Attempts to generate a limited entropy randomization source value.
  EntropyState entropy_state(&prefs_);
  entropy_state.GetLimitedEntropyRandomizationSource();

  // The generated value should not be the one in prefs initially.
  EXPECT_NE(test_value, entropy_state.GetLimitedEntropyRandomizationSource());
  // There should be a new value, and the new value should overwrite the one in
  // prefs initially.
  EXPECT_EQ(entropy_state.GetLimitedEntropyRandomizationSource(),
            prefs_.GetString(prefs::kMetricsLimitedEntropyRandomizationSource));
}

TEST_F(EntropyStateTest, ValidLimitedEntropyRandomizationSource) {
  const char* test_values[] = {
      "00000000000000000000000000000001",
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
      "0123456789ABCDEF0123456789ABCDEF",
  };
  for (auto* test_value : test_values) {
    EXPECT_TRUE(
        EntropyState::IsValidLimitedEntropyRandomizationSource(test_value))
        << "Expect EntropyState::IsValidLimitedEntropyRandomizationSource("
        << test_value << ") to be true.";
  }
}

TEST_F(EntropyStateTest, InvalidLimitedEntropyRandomizationSource) {
  const char* test_values[] = {
      // The empty string is not a valid limited entropy randomization source.
      "",
      // A value with all zero is a not a valid `base::UnguessableToken`.
      "00000000000000000000000000000000",
      // Not a hex string representing 128 bits.
      "1234",
      // A string with valid length of 128 bits but 'X' is not a hex value.
      "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
      // A invalid hex string because of the lower case letters.
      "0123456789abcdef0123456789abcdef",
  };
  for (auto* test_value : test_values) {
    EXPECT_FALSE(
        EntropyState::IsValidLimitedEntropyRandomizationSource(test_value))
        << "Expect EntropyState::IsValidLimitedEntropyRandomizationSource("
        << test_value << ") to be false.";
  }
}

}  // namespace metrics
