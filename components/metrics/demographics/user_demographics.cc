// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographics/user_demographics.h"

#include <utility>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {

// Root dictionary pref to store the user's birth year and gender that are
// provided by the sync server. This is a read-only syncable priority pref, sent
// from the sync server to the client.
const char kSyncDemographicsPrefName[] = "sync.demographics";

// Stores a "secret" offset that is used to randomize the birth year for metrics
// reporting. This value should not be logged to UMA directly; instead, it
// should be summed with the kSyncDemographicsBirthYear. This value is generated
// locally on the client the first time a user begins to merge birth year data
// into their UMA reports. The value is synced to the user's other devices so
// that the user consistently uses the same offset across login/logout events
// and after clearing their other browser data.
const char kSyncDemographicsBirthYearOffsetPrefName[] =
    "sync.demographics_birth_year_offset";

// This pref value is subordinate to the kSyncDemographics dictionary pref and
// is synced to the client. It stores the self-reported birth year of the
// syncing user. as provided by the sync server. This value should not be logged
// to UMA directly; instead, it should be summed with the
// kSyncDemographicsBirthYearNoiseOffset.
const char kSyncDemographicsBirthYearPath[] = "birth_year";

// This pref value is subordinate to the kSyncDemographics dictionary pref and
// is synced to the client. It stores the self-reported gender of the syncing
// user, as provided by the sync server. The gender is encoded using the Gender
// enum defined in UserDemographicsProto
// (see third_party/metrics_proto/user_demographics.proto).
const char kSyncDemographicsGenderPath[] = "gender";

namespace {

// Gets an offset to add noise to the birth year. If not present in prefs, the
// offset will be randomly generated within the offset range and cached in
// syncable prefs.
int GetBirthYearOffset(PrefService* pref_service) {
  int offset =
      pref_service->GetInteger(kSyncDemographicsBirthYearOffsetPrefName);
  if (offset == kUserDemographicsBirthYearNoiseOffsetDefaultValue) {
    // Generate a random offset when not cached in prefs.
    offset = base::RandInt(-kUserDemographicsBirthYearNoiseOffsetRange,
                           kUserDemographicsBirthYearNoiseOffsetRange);
    pref_service->SetInteger(kSyncDemographicsBirthYearOffsetPrefName, offset);
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
absl::optional<int> GetUserBirthYear(const base::Value::Dict& demographics) {
  return demographics.FindInt(kSyncDemographicsBirthYearPath);
}

// Gets the synced user's gender from synced prefs, see doc of
// DemographicMetricsProvider in demographic_metrics_provider.h for more
// details.
absl::optional<UserDemographicsProto_Gender> GetUserGender(
    const base::Value::Dict& demographics) {
  const absl::optional<int> gender_int =
      demographics.FindInt(kSyncDemographicsGenderPath);

  // Verify that the gender is unset.
  if (!gender_int)
    return absl::nullopt;

  // Verify that the gender number is a valid UserDemographicsProto_Gender
  // encoding.
  if (!UserDemographicsProto_Gender_IsValid(*gender_int))
    return absl::nullopt;

  const auto gender = UserDemographicsProto_Gender(*gender_int);

  // Verify that the gender is in a large enough population set to preserve
  // anonymity.
  if (gender != UserDemographicsProto::GENDER_FEMALE &&
      gender != UserDemographicsProto::GENDER_MALE) {
    return absl::nullopt;
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

void RegisterDemographicsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      kSyncDemographicsPrefName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      kSyncDemographicsBirthYearOffsetPrefName,
      kUserDemographicsBirthYearNoiseOffsetDefaultValue,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void ClearDemographicsPrefs(PrefService* pref_service) {
  // Clear user's birth year and gender.
  // Note that we retain kSyncDemographicsBirthYearOffset. If the user resumes
  // syncing, causing these prefs to be recreated, we don't want them to start
  // reporting a different randomized birth year as this could narrow down or
  // even reveal their true birth year.
  pref_service->ClearPref(kSyncDemographicsPrefName);
}

UserDemographicsResult GetUserNoisedBirthYearAndGenderFromPrefs(
    base::Time now,
    PrefService* pref_service) {
  // Verify that the now time is available. There are situations where the now
  // time cannot be provided.
  if (now.is_null()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kCannotGetTime);
  }

  // Get the synced user’s noised birth year and gender from synced prefs. Only
  // one error status code should be used to represent the case where
  // demographics are ineligible, see doc of UserDemographicsStatus in
  // user_demographics.h for more details.

  // Get the pref that contains the user's birth year and gender.
  const base::Value::Dict& demographics =
      pref_service->GetDict(kSyncDemographicsPrefName);

  // Get the user's birth year.
  absl::optional<int> birth_year = GetUserBirthYear(demographics);
  if (!birth_year.has_value()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Get the user's gender.
  absl::optional<UserDemographicsProto_Gender> gender =
      GetUserGender(demographics);
  if (!gender.has_value()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Get the offset and do one last check that the birth year is eligible.
  int offset = GetBirthYearOffset(pref_service);
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
