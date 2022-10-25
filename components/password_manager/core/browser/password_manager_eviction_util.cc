// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_eviction_util.h"

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

base::flat_set<int> ParseErrorList(const std::string& serialised_list) {
  base::flat_set<int> error_list;

  for (base::StringPiece error_str :
       base::SplitStringPiece(serialised_list, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    int error_code;
    bool is_converted = base::StringToInt(error_str, &error_code);
    DCHECK(is_converted);
    error_list.emplace(error_code);
  }

  return error_list;
}

}  // namespace

namespace password_manager_upm_eviction {

bool IsCurrentUserEvicted(const PrefService* prefs) {
  return prefs->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
}

void EvictCurrentUser(int api_error_code, PrefService* prefs) {
  if (prefs->GetBoolean(password_manager::prefs::
                            kUnenrolledFromGoogleMobileServicesDueToErrors))
    // User is already evicted, keep the original eviction reason.
    return;

  prefs->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  prefs->SetInteger(password_manager::prefs::
                        kUnenrolledFromGoogleMobileServicesAfterApiErrorCode,
                    api_error_code);
  prefs->SetInteger(password_manager::prefs::
                        kUnenrolledFromGoogleMobileServicesWithErrorListVersion,
                    password_manager::features::kGmsApiErrorListVersion.Get());

  // Reset migration prefs so when the user can join the experiment again,
  // non-syncable data and settings can be migrated to GMS Core.
  prefs->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      0);
  prefs->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt, 0.0);
  prefs->SetBoolean(password_manager::prefs::kSettingsMigratedToUPM, false);

  // Reset the counter for the auth error prompts, so that the user starts
  // with a fresh state when re-enrolling.
  prefs->SetInteger(password_manager::prefs::kTimesUPMAuthErrorShown, 0);

  base::UmaHistogramBoolean("PasswordManager.UnenrolledFromUPMDueToErrors",
                            true);
  base::UmaHistogramSparse("PasswordManager.UPMUnenrollmentReason",
                           api_error_code);
  LOG(ERROR) << "Unenrolled from UPM due to error with code: "
             << api_error_code;
}

bool ShouldInvalidateEviction(const PrefService* prefs) {
  if (!IsCurrentUserEvicted(prefs))
    return false;

  // Configured error versions are > 0, default stored version is 0.
  int stored_version = prefs->GetInteger(
      password_manager::prefs::
          kUnenrolledFromGoogleMobileServicesWithErrorListVersion);

  return stored_version <
         password_manager::features::kGmsApiErrorListVersion.Get();
}

void ReenrollCurrentUser(PrefService* prefs) {
  DCHECK(prefs->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));

  prefs->ClearPref(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
  prefs->ClearPref(password_manager::prefs::
                       kUnenrolledFromGoogleMobileServicesAfterApiErrorCode);
  prefs->ClearPref(password_manager::prefs::
                       kUnenrolledFromGoogleMobileServicesWithErrorListVersion);
  prefs->ClearPref(
      password_manager::prefs::kTimesReenrolledToGoogleMobileServices);
  prefs->ClearPref(
      password_manager::prefs::kTimesAttemptedToReenrollToGoogleMobileServices);
}

bool ShouldIgnoreOnApiError(int api_error_code) {
  base::flat_set<int> ignored_errors =
      ParseErrorList(password_manager::features::kIgnoredGmsApiErrors.Get());
  return ignored_errors.contains(api_error_code);
}

bool ShouldRetryOnApiError(int api_error_code) {
  base::flat_set<int> ignored_errors =
      ParseErrorList(password_manager::features::kRetriableGmsApiErrors.Get());
  return ignored_errors.contains(api_error_code);
}

}  // namespace password_manager_upm_eviction
