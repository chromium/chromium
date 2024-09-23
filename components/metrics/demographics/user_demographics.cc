// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographics/user_demographics.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace metrics {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr auto kSyncDemographicsPrefFlags =
    user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF;
#else
constexpr auto kSyncOsDemographicsPrefFlags =
    user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF;
// TODO(crbug.com/40240008): Make this non-syncable (on Ash only) after full
// rollout of the syncable os priority pref; then delete it locally from Ash
// devices.
constexpr auto kSyncDemographicsPrefFlags =
    user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF;
#endif

constexpr auto kUserDemographicsBirthYearOffsetPrefFlags =
    PrefRegistry::NO_REGISTRATION_FLAGS;
constexpr auto kDeprecatedDemographicsBirthYearOffsetPrefFlags =
    PrefRegistry::NO_REGISTRATION_FLAGS;

namespace {

const base::Value::Dict& GetDemographicsDict(PrefService* profile_prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40240008): On Ash only, clear sync demographics pref once
  // os-level syncable pref is fully rolled out and Ash drops support for
  // non-os-level syncable prefs.
  if (profile_prefs->HasPrefPath(kSyncOsDemographicsPrefName)) {
    return profile_prefs->GetDict(kSyncOsDemographicsPrefName);
  }
#endif
  return profile_prefs->GetDict(kSyncDemographicsPrefName);
}

void MigrateBirthYearOffset(PrefService* to_local_state,
                            PrefService* from_profile_prefs) {
  const int profile_offset = from_profile_prefs->GetInteger(
      kDeprecatedDemographicsBirthYearOffsetPrefName);
  if (profile_offset == kUserDemographicsBirthYearNoiseOffsetDefaultValue)
    return;

  // TODO(crbug.com/40240008): clear/remove deprecated pref after 2023/09

  const int local_offset =
      to_local_state->GetInteger(kUserDemographicsBirthYearOffsetPrefName);
  if (local_offset == kUserDemographicsBirthYearNoiseOffsetDefaultValue) {
    to_local_state->SetInteger(kUserDemographicsBirthYearOffsetPrefName,
                               profile_offset);
  }
}

// Returns the noise offset for the birth year. If not found in |local_state|,
// the offset will be randomly generated within the offset range and cached in
// |local_state|.
int GetBirthYearOffset(PrefService* local_state) {
  int offset =
      local_state->GetInteger(kUserDemographicsBirthYearOffsetPrefName);
  if (offset == kUserDemographicsBirthYearNoiseOffsetDefaultValue) {
    // Generate a new random offset when not already cached.
    offset = base::RandInt(-kUserDemographicsBirthYearNoiseOffsetRange,
                           kUserDemographicsBirthYearNoiseOffsetRange);
    local_state->SetInteger(kUserDemographicsBirthYearOffsetPrefName, offset);
  }
  return offset;
}

// Determines whether the synced user has provided a birth year to Google which
// is eligible, once aggregated and anonymized, to measure usage of Chrome
// features by age groups. See doc of DemographicMetricsProvider in
// demographic_metrics_provider.h for more details.
bool HasEligibleBirthYear(base::Time now, int user_birth_year, int offset) {
  // Compute user age.
  base::Time::Exploded exploded_now_time;
  now.LocalExplode(&exploded_now_time);
  int user_age = exploded_now_time.year - (user_birth_year + offset);

  // Verify if the synced user's age has a population size in the age
  // distribution of the society that is big enough to not raise the entropy of
  // the demographics too much. At a certain point, as the age increase, the
  // size of the population starts declining sharply as you can see in this
  // approximate representation of the age distribution:
  // |       ________         max age
  // |______/        \_________ |
  // |                          |\
  // |                          | \
  // +--------------------------|---------
  //  0 10 20 30 40 50 60 70 80 90 100+
  if (user_age > kUserDemographicsMaxAgeInYears)
    return false;

  // Verify if the synced user is old enough. Use > rather than >= because we
  // want to be sure that the user is at least |kUserDemographicsMinAgeInYears|
  // without disclosing their birth date, which requires to add an extra year
  // margin to the minimal age to be safe. For example, if we are in 2019-07-10
  // (now) and the user was born in 1999-08-10, the user is not yet 20 years old
  // (minimal age) but we cannot know that because we only have access to the
  // year of the dates (2019 and 1999 respectively). If we make sure that the
  // minimal age (computed at year granularity) is at least 21, we are 100% sure
  // that the user will be at least 20 years old when providing the user’s birth
  // year and gender.
  return user_age > kUserDemographicsMinAgeInYears;
}

// Gets the synced user's birth year from synced prefs, see doc of
// DemographicMetricsProvider in demographic_metrics_provider.h for more
// details.
std::optional<int> GetUserBirthYear(const base::Value::Dict& demographics) {
  return demographics.FindInt(kSyncDemographicsBirthYearPath);
}

// Gets the synced user's gender from synced prefs, see doc of
// DemographicMetricsProvider in demographic_metrics_provider.h for more
// details.
std::optional<UserDemographicsProto_Gender> GetUserGender(
    const base::Value::Dict& demographics) {
  const std::optional<int> gender_int =
      demographics.FindInt(kSyncDemographicsGenderPath);

  // Verify that the gender is unset.
  if (!gender_int)
    return std::nullopt;

  // Verify that the gender number is a valid UserDemographicsProto_Gender
  // encoding.
  if (!UserDemographicsProto_Gender_IsValid(*gender_int))
    return std::nullopt;

  const auto gender = UserDemographicsProto_Gender(*gender_int);

  // Verify that the gender is in a large enough population set to preserve
  // anonymity.
  if (gender != UserDemographicsProto::GENDER_FEMALE &&
      gender != UserDemographicsProto::GENDER_MALE) {
    return std::nullopt;
  }

  return gender;
}

}  // namespace

// static
UserDemographicsResult UserDemographicsResult::ForValue(
    UserDemographics value) {
  return UserDemographicsResult(std::move(value),
                                UserDemographicsStatus::kSuccess);
}

// static
UserDemographicsResult UserDemographicsResult::ForStatus(
    UserDemographicsStatus status) {
  DCHECK(status != UserDemographicsStatus::kSuccess);
  return UserDemographicsResult(UserDemographics(), status);
}

bool UserDemographicsResult::IsSuccess() const {
  return status_ == UserDemographicsStatus::kSuccess;
}

UserDemographicsStatus UserDemographicsResult::status() const {
  return status_;
}

const UserDemographics& UserDemographicsResult::value() const {
  return value_;
}

UserDemographicsResult::UserDemographicsResult(UserDemographics value,
                                               UserDemographicsStatus status)
    : value_(std::move(value)), status_(status) {}

void RegisterDemographicsLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kUserDemographicsBirthYearOffsetPrefName,
      kUserDemographicsBirthYearNoiseOffsetDefaultValue,
      kUserDemographicsBirthYearOffsetPrefFlags);
}

void RegisterDemographicsProfilePrefs(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterDictionaryPref(kSyncOsDemographicsPrefName,
                                   kSyncOsDemographicsPrefFlags);
#endif
  registry->RegisterDictionaryPref(kSyncDemographicsPrefName,
                                   kSyncDemographicsPrefFlags);
  registry->RegisterIntegerPref(
      kDeprecatedDemographicsBirthYearOffsetPrefName,
      kUserDemographicsBirthYearNoiseOffsetDefaultValue,
      kDeprecatedDemographicsBirthYearOffsetPrefFlags);
}

void ClearDemographicsPrefs(PrefService* profile_prefs) {
  // Clear the dict holding the user's birth year and gender.
  //
  // Note: We never clear kUserDemographicsBirthYearOffset from local state.
  // The device should continue to use the *same* noise value as long as the
  // device's UMA client id remains the same. If the noise value were allowed
  // to change for a given user + client id, then the min/max noisy birth year
  // values could both be reported, revealing the true value in the middle.
  profile_prefs->ClearPref(kSyncDemographicsPrefName);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kSyncOsDemographicsPrefName);
#endif
}

UserDemographicsResult GetUserNoisedBirthYearAndGenderFromPrefs(
    base::Time now,
    PrefService* local_state,
    PrefService* profile_prefs) {
  // Verify that the now time is available. There are situations where the now
  // time cannot be provided.
  if (now.is_null()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kCannotGetTime);
  }

  // Get the synced user’s noised birth year and gender from synced profile
  // prefs. Only one error status code should be used to represent the case
  // where demographics are ineligible, see doc of UserDemographicsStatus in
  // user_demographics.h for more details.

  // Get the pref that contains the user's birth year and gender.
  const base::Value::Dict& demographics = GetDemographicsDict(profile_prefs);

  // Get the user's birth year.
  std::optional<int> birth_year = GetUserBirthYear(demographics);
  if (!birth_year.has_value()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Get the user's gender.
  std::optional<UserDemographicsProto_Gender> gender =
      GetUserGender(demographics);
  if (!gender.has_value()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Get the offset from local_state/profile_prefs and do one last check that
  // the birth year is eligible.
  // TODO(crbug.com/40240008): remove profile_prefs after 2023/09
  MigrateBirthYearOffset(local_state, profile_prefs);
  int offset = GetBirthYearOffset(local_state);
  if (!HasEligibleBirthYear(now, *birth_year, offset)) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Set gender and noised birth year in demographics.
  UserDemographics user_demographics;
  user_demographics.gender = *gender;
  user_demographics.birth_year = *birth_year + offset;

  return UserDemographicsResult::ForValue(std::move(user_demographics));
}

}  // namespace metrics
