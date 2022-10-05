// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_EVICTION_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_EVICTION_UTIL_H_

#include "components/prefs/pref_service.h"

namespace password_manager_upm_eviction {

// Checks whether the current user is currently evicted from the UPM experiment.
bool IsCurrentUserEvicted(const PrefService* prefs);

// Evicts the current user and saves the provided error as eviction reason.
// The current error list version is also saved.
void EvictCurrentUser(int api_error_code, PrefService* prefs);

// Checks whether the eviction status for the current user should be invalidated
// and the user should be re-enrolled to the UPM experiment.
// Returns true if the user is evicted and currently configured error list
// version is greater than the saved one.
bool ShouldInvalidateEviction(const PrefService* prefs);

// Resets the eviction status for the current user.
// Also clears the stored eviction reason and error list version.
void ReenrollCurrentUser(PrefService* prefs);

// Checks whether the occurred GMS Core API error should be ignored.
// Returns true if the error is in the ignored error list, false otherwise.
// Ignored errors are not handled by the backend and are returned to the caller.
bool ShouldIgnoreOnApiError(int api_error_code);

// Checks whether the occurred GMS Core API error should be retried.
// Returns true if the error is in the retriable error list, false otherwise.
// Certain backend operations may be retried if the returned error is retriable.
bool ShouldRetryOnApiError(int api_error_code);

}  // namespace password_manager_upm_eviction

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_EVICTION_UTIL_H_
