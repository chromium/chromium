// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_safe_seed.h"

#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations::cros_early_boot::evaluate_seed {
namespace {

TEST(EarlyBootSafeSeed, FetchTime) {
  featured::SeedDetails details;

  constexpr int kFetchTimeMillisSinceWindowsEpoch = 1234567890;
  details.set_fetch_time(kFetchTimeMillisSinceWindowsEpoch);

  EarlyBootSafeSeed early_boot_safe_seed(details);

  EXPECT_EQ(early_boot_safe_seed.GetFetchTime(),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Milliseconds(kFetchTimeMillisSinceWindowsEpoch)));

  // Should not change.
  early_boot_safe_seed.SetFetchTime(base::Time::Now());
  EXPECT_EQ(early_boot_safe_seed.GetFetchTime(),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Milliseconds(kFetchTimeMillisSinceWindowsEpoch)));
}

TEST(EarlyBootSafeSeed, Milestone) {
  featured::SeedDetails details;
  details.set_milestone(100);

  EarlyBootSafeSeed early_boot_safe_seed(details);

  EXPECT_EQ(early_boot_safe_seed.GetMilestone(), 100);

  // Should not change.
  early_boot_safe_seed.SetMilestone(101);
  EXPECT_EQ(early_boot_safe_seed.GetMilestone(), 100);

  // Still should not change.
  early_boot_safe_seed.ClearState();
  EXPECT_EQ(early_boot_safe_seed.GetMilestone(), 100);
}

TEST(EarlyBootSafeSeed, TimeForStudyDateChecks) {
  featured::SeedDetails details;

  constexpr int kFetchTimeMillisSinceWindowsEpoch = 1234567890;
  details.set_date(kFetchTimeMillisSinceWindowsEpoch);

  EarlyBootSafeSeed early_boot_safe_seed(details);

  EXPECT_EQ(early_boot_safe_seed.GetTimeForStudyDateChecks(),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Milliseconds(kFetchTimeMillisSinceWindowsEpoch)));
  // Should not change.
  early_boot_safe_seed.SetTimeForStudyDateChecks(base::Time::Now());
  EXPECT_EQ(early_boot_safe_seed.GetTimeForStudyDateChecks(),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Milliseconds(kFetchTimeMillisSinceWindowsEpoch)));
}

TEST(EarlyBootSafeSeed, GetCompressedSeed) {
  featured::SeedDetails details;
  details.set_b64_compressed_data("compressed_data");

  EarlyBootSafeSeed early_boot_safe_seed(details);
  EXPECT_EQ(early_boot_safe_seed.GetCompressedSeed(), "compressed_data");
}

TEST(EarlyBootSafeSeed, GetSignature) {
  featured::SeedDetails details;
  details.set_signature("signature");

  EarlyBootSafeSeed early_boot_safe_seed(details);
  EXPECT_EQ(early_boot_safe_seed.GetSignature(), "signature");

  // Should not change.
  early_boot_safe_seed.SetSignature("asdf");
  EXPECT_EQ(early_boot_safe_seed.GetSignature(), "signature");
}

TEST(EarlyBootSafeSeed, GetLocale) {
  featured::SeedDetails details;
  details.set_locale("xx-YY");

  EarlyBootSafeSeed early_boot_safe_seed(details);
  EXPECT_EQ(early_boot_safe_seed.GetLocale(), "xx-YY");

  // Should not change.
  early_boot_safe_seed.SetLocale("zz-AA");
  EXPECT_EQ(early_boot_safe_seed.GetLocale(), "xx-YY");
}

TEST(EarlyBootSafeSeed, GetPermanentConsistencyCountry) {
  featured::SeedDetails details;
  details.set_permanent_consistency_country("us");

  EarlyBootSafeSeed early_boot_safe_seed(details);
  EXPECT_EQ(early_boot_safe_seed.GetPermanentConsistencyCountry(), "us");

  // Should not change.
  early_boot_safe_seed.SetPermanentConsistencyCountry("ca");
  EXPECT_EQ(early_boot_safe_seed.GetPermanentConsistencyCountry(), "us");
}

TEST(EarlyBootSafeSeed, GetSessionConsistencyCountry) {
  featured::SeedDetails details;
  details.set_session_consistency_country("us");

  EarlyBootSafeSeed early_boot_safe_seed(details);
  EXPECT_EQ(early_boot_safe_seed.GetSessionConsistencyCountry(), "us");

  early_boot_safe_seed.SetSessionConsistencyCountry("ca");
  EXPECT_EQ(early_boot_safe_seed.GetSessionConsistencyCountry(), "us");
}

// Mutators should not crash.
TEST(EarlyBootSafeSeed, MutatorsDontCrash) {
  featured::SeedDetails details;
  EarlyBootSafeSeed early_boot_safe_seed(details);

  early_boot_safe_seed.SetFetchTime(base::Time::Now());
  early_boot_safe_seed.SetMilestone(100);
  early_boot_safe_seed.SetTimeForStudyDateChecks(base::Time::Now());
  early_boot_safe_seed.SetCompressedSeed("data");
  early_boot_safe_seed.SetSignature("signature");
  early_boot_safe_seed.SetLocale("locale");
  early_boot_safe_seed.SetPermanentConsistencyCountry("us");
  early_boot_safe_seed.SetSessionConsistencyCountry("us");
  early_boot_safe_seed.ClearState();
}

}  // namespace
}  // namespace variations::cros_early_boot::evaluate_seed
