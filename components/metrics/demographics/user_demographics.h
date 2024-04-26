// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEMOGRAPHICS_USER_DEMOGRAPHICS_H_
#define COMPONENTS_METRICS_DEMOGRAPHICS_USER_DEMOGRAPHICS_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

// Default value for user gender when no value has been set.
constexpr int kUserDemographicsGenderDefaultValue = -1;

// Default value for user gender enum when no value has been set.
constexpr UserDemographicsProto_Gender kUserDemographicGenderDefaultEnumValue =
    UserDemographicsProto_Gender_Gender_MIN;

// Default value for user birth year when no value has been set.
constexpr int kUserDemographicsBirthYearDefaultValue = -1;

// Default value for birth year offset when no value has been set. Set to a
// really high value that |kUserDemographicsBirthYearNoiseOffsetRange| will
// never go over.
constexpr int kUserDemographicsBirthYearNoiseOffsetDefaultValue = 100;

// Boundary of the +/- range in years within witch to randomly pick an offset to
// add to the user birth year. This adds noise to the birth year to not allow
// someone to accurately know a user's age. Possible offsets range from -2 to 2.
constexpr int kUserDemographicsBirthYearNoiseOffsetRange = 2;

// Minimal user age in years to provide demographics for.
constexpr int kUserDemographicsMinAgeInYears = 20;

// Max user age to provide demopgrahics for.
constexpr int kUserDemographicsMaxAgeInYears = 85;

// Root dictionary pref to store the user's birth year and gender that are
// provided by the sync server. This is a read-only syncable priority pref on
// all platforms except ChromeOS Ash, where it is a syncable OS-level priority
// pref.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kSyncDemographicsPrefName[] = "sync.demographics";
#else
inline constexpr char kSyncOsDemographicsPrefName[] = "sync.os_demographics";
// TODO(crbug.com/40240008): Make this non-syncable (on Ash only) after full
// rollout of the syncable os priority pref; then delete it locally from Ash
// devices.
inline constexpr char kSyncDemographicsPrefName[] = "sync.demographics";
#endif

// Stores a "secret" offset that is used to randomize the birth year for metrics
// reporting. This value should not be logged to UMA directly; instead, it
// should be summed with the kSyncDemographicsBirthYear. This value is generated
// locally on the client the first time a user begins to merge birth year data
// into their UMA reports.
inline constexpr char kUserDemographicsBirthYearOffsetPrefName[] =
    "demographics_birth_year_offset";
// TODO(crbug.com/40240008): Delete after 2023/09
inline constexpr char kDeprecatedDemographicsBirthYearOffsetPrefName[] =
    "sync.demographics_birth_year_offset";

// These are not prefs, they are paths inside of kSyncDemographics.

// This pref value is subordinate to the kSyncDemographics dictionary pref and
// is synced to the client. It stores the self-reported birth year of the
// syncing user. as provided by the sync server. This value should not be logged
// to UMA directly; instead, it should be summed with the
// kSyncDemographicsBirthYearNoiseOffset.
inline constexpr char kSyncDemographicsBirthYearPath[] = "birth_year";

// This pref value is subordinate to the kSyncDemographics dictionary pref and
// is synced to the client. It stores the self-reported gender of the syncing
// user, as provided by the sync server. The gender is encoded using the Gender
// enum defined in UserDemographicsProto
// (see third_party/metrics_proto/user_demographics.proto).
inline constexpr char kSyncDemographicsGenderPath[] = "gender";

// Container of user demographics.
struct UserDemographics {
  int birth_year = 0;
  UserDemographicsProto_Gender gender = UserDemographicsProto::Gender_MIN;
};

// Represents the status of providing user demographics (noised birth year and
// gender) that are logged to UMA. Entries of the enum should not be renumbered
// and numeric values should never be reused. Please keep in sync with
// "UserDemographicsStatus" in src/tools/metrics/histograms/enums.xml.  There
// should only be one entry representing demographic data that is ineligible to
// be provided. A finer grained division of kIneligibleDemographicsData (e.g.,
// INVALID_BIRTH_YEAR) might help inferring categories of demographics that
// should not be exposed to protect privacy.
enum class UserDemographicsStatus {
  // Could get the user's noised birth year and gender.
  kSuccess = 0,
  // Sync is not enabled.
  kSyncNotEnabled = 1,
  // User's birth year and gender are ineligible to be provided either because
  // some of them don't exist (missing data) or some of them are not eligible to
  // be provided.
  kIneligibleDemographicsData = 2,
  // Could not get the time needed to compute the user's age.
  kCannotGetTime = 3,
  // There is more than one Profile for the Chrome browser. This entry is used
  // by the DemographicMetricsProvider client.
  kMoreThanOneProfile = 4,
  // There is no sync service available for the loaded Profile. This entry is
  // used by the DemographicMetricsProvider client.
  kNoSyncService = 5,
  // Upper boundary of the enum that is needed for the histogram.
  kMaxValue = kNoSyncService
};

// Container and handler for data related to the retrieval of user demographics.
// Holds either valid demographics data or an error code.
class UserDemographicsResult {
 public:
  // Builds a UserDemographicsResult that contains user demographics and has a
  // UserDemographicsStatus::kSuccess status.
  static UserDemographicsResult ForValue(UserDemographics value);

  // Builds a UserDemographicsResult that does not have user demographics (only
  // default values) and has a status other than
  // UserDemographicsStatus::kSuccess.
  static UserDemographicsResult ForStatus(UserDemographicsStatus status);

  // Determines whether demographics could be successfully retrieved.
  bool IsSuccess() const;

  // Gets the status of the result.
  UserDemographicsStatus status() const;

  // Gets the value of the result which will contain data when status() is
  // UserDemographicsStatus::kSuccess.
  const UserDemographics& value() const;

 private:
  UserDemographicsResult(UserDemographics value, UserDemographicsStatus status);

  UserDemographics value_;
  UserDemographicsStatus status_ = UserDemographicsStatus::kMaxValue;
};

// Registers the local state preferences that are needed to persist demographics
// information exposed via GetUserNoisedBirthYearAndGenderFromPrefs().
void RegisterDemographicsLocalStatePrefs(PrefRegistrySimple* registry);

// Registers the profile preferences that are needed to persist demographics
// information exposed via GetUserNoisedBirthYearAndGenderFromPrefs().
void RegisterDemographicsProfilePrefs(PrefRegistrySimple* registry);

// Clears the profile's demographics-related preferences containing user data.
// This excludes the internal birth year offset.
void ClearDemographicsPrefs(PrefService* profile_prefs);

// Gets the synced user’s birth year and gender from |profile_prefs|, and noise
// from |local_state|. See docs for metrics::DemographicMetricsProvider in
// components/metrics/demographic_metrics_provider.h for more details. Returns
// an error status with an empty value when the user's birth year or gender
// cannot be provided. You need to provide an accurate |now| time that
// represents the current time.
UserDemographicsResult GetUserNoisedBirthYearAndGenderFromPrefs(
    base::Time now,
    PrefService* local_state,
    PrefService* profile_prefs);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEMOGRAPHICS_USER_DEMOGRAPHICS_H_
