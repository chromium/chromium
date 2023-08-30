// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_seed_store.h"

#include <string>

#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations::cros_early_boot::evaluate_seed {

namespace {

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100.
VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("test");
  study->set_default_experiment_name("abc");
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name("abc");
  experiment->set_probability_weight(100);
  seed.set_serial_number("123");
  return seed;
}

TEST(EarlyBootSeedStoreTest, LoadSafeSeed) {
  const VariationsSeed safe_seed = CreateTestSeed();

  featured::SeedDetails safe_seed_details;
  safe_seed_details.set_date(123456789);
  safe_seed_details.set_fetch_time(987654321);
  safe_seed_details.set_locale("xx-YY");
  safe_seed_details.set_permanent_consistency_country("us");
  safe_seed_details.set_session_consistency_country("ca");

  std::string serialized_seed;
  ASSERT_TRUE(safe_seed.SerializeToString(&serialized_seed));
  safe_seed_details.set_compressed_data(serialized_seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  // Allow empty signature.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);

  EarlyBootSeedStore store(&prefs, safe_seed_details);

  VariationsSeed actual_seed;
  ClientFilterableState actual_client_state(
      /*is_enterprise_function=*/base::BindOnce([] { return false; }),
      /*google_groups_function=*/base::BindOnce(
          [] { return base::flat_set<uint64_t>(); }));
  EXPECT_TRUE(store.LoadSafeSeed(&actual_seed, &actual_client_state));

  EXPECT_THAT(actual_seed, EqualsProto(safe_seed));
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::Milliseconds(safe_seed_details.date())),
            actual_client_state.reference_date);
  EXPECT_EQ(safe_seed_details.locale(), actual_client_state.locale);
  EXPECT_EQ(safe_seed_details.permanent_consistency_country(),
            actual_client_state.permanent_consistency_country);
  EXPECT_EQ(safe_seed_details.session_consistency_country(),
            actual_client_state.session_consistency_country);

  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::Milliseconds(safe_seed_details.fetch_time())),
            store.GetSafeSeedFetchTime());
}

TEST(EarlyBootSeedStoreDeathTest, LoadSafeSeed_Unspecified) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  EarlyBootSeedStore store(&prefs, absl::nullopt);
  VariationsSeed actual_seed;
  ClientFilterableState actual_client_state(
      /*is_enterprise_function=*/base::BindOnce([] { return false; }),
      /*google_groups_function=*/base::BindOnce(
          [] { return base::flat_set<uint64_t>(); }));
  EXPECT_DEATH(store.LoadSafeSeed(&actual_seed, &actual_client_state), "");
}

TEST(EarlyBootSeedStoreTest, LoadSafeSeed_Invalid) {
  featured::SeedDetails safe_seed_details;
  safe_seed_details.set_compressed_data("bad");

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  // Allow empty signature.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);

  EarlyBootSeedStore store(&prefs, safe_seed_details);

  VariationsSeed actual_seed;
  ClientFilterableState actual_client_state(
      /*is_enterprise_function=*/base::BindOnce([] { return false; }),
      /*google_groups_function=*/base::BindOnce(
          [] { return base::flat_set<uint64_t>(); }));
  EXPECT_FALSE(store.LoadSafeSeed(&actual_seed, &actual_client_state));
}

}  // namespace
}  // namespace variations::cros_early_boot::evaluate_seed
