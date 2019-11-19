// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_
#define COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_

#include "third_party/metrics_proto/user_demographics.pb.h"

namespace syncer {

// Default value for user gender when no value has been set.
constexpr int kUserDemographicsGenderDefaultValue = -1;

// Default value for user gender enum when no value has been set.
constexpr metrics::UserDemographicsProto_Gender
    kUserDemographicGenderDefaultEnumValue =
        metrics::UserDemographicsProto_Gender_Gender_MIN;

// Default value for user gender when no value has been set.
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

// Container of user demographics.
struct UserDemographics {
  int birth_year = 0;
  metrics::UserDemographicsProto_Gender gender =
      metrics::UserDemographicsProto::Gender_MIN;
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
  // by the metrics::DemographicMetricsProvider client.
  kMoreThanOneProfile = 4,
  // There is no sync service available for the loaded Profile. This entry is
  // used by the metrics::DemographicMetricsProvider client.
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

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_USER_DEMOGRAPHICS_H_
