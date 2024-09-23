// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_
#define COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/values.h"

namespace base {
class Time;
class Version;
}  // namespace base

class PrefService;
class PrefRegistrySimple;

namespace update_client {

extern const char kPersistedDataPreference[];
extern const char kLastUpdateCheckErrorPreference[];
extern const char kLastUpdateCheckErrorCategoryPreference[];
extern const char kLastUpdateCheckErrorExtraCode1Preference[];

class ActivityDataService;
struct CategorizedError;

// A PersistedData is a wrapper layer around a PrefService, designed to maintain
// update data that outlives the browser process and isn't exposed outside of
// update_client.
//
// The public methods of this class should be called only on the sequence that
// initializes it - which also has to match the sequence the PrefService has
// been initialized on.
class PersistedData {
 public:
  virtual ~PersistedData() = default;

  // Records the last update check error code (0 for success).
  virtual void SetLastUpdateCheckError(const CategorizedError& error) = 0;

  // Returns the DateLastRollCall (the server-localized calendar date number the
  // |id| was last checked by this client on) for the specified |id|.
  // -2 indicates that there is no recorded date number for the |id|.
  virtual int GetDateLastRollCall(const std::string& id) const = 0;

  // Returns the DateLastActive (the server-localized calendar date number the
  // |id| was last active by this client on) for the specified |id|.
  // -1 indicates that there is no recorded date for the |id| (i.e. this is the
  // first time the |id| is active).
  // -2 indicates that the |id| has an unknown value of last active date.
  virtual int GetDateLastActive(const std::string& id) const = 0;

  // Returns the PingFreshness (a random token that is written into the profile
  // data whenever the DateLastRollCall it is modified) for the specified |id|.
  // "" indicates that there is no recorded freshness value for the |id|.
  virtual std::string GetPingFreshness(const std::string& id) const = 0;

  // Records the DateLastRollcall for the specified `ids`. Also records
  // DateLastActive, if the ids have active bits currently set, and then clears
  // those bits. Rotates PingFreshness. Then, calls `callback` on the calling
  // sequence. Calls with a negative `datenum` or that occur prior to the
  // initialization of the persisted data store will simply post the callback
  // immediately.
  virtual void SetDateLastData(const std::vector<std::string>& ids,
                               int datenum,
                               base::OnceClosure callback) = 0;

  // Sets DateLastActive. Prefer to use SetDateLastData. This method should
  // only be used for importing data from other data stores.
  virtual void SetDateLastActive(const std::string& id, int dla) = 0;

  // Sets DateLastRollCall. Prefer to use SetDateLastData. This method should
  // only be used for importing data from other data stores.
  virtual void SetDateLastRollCall(const std::string& id, int dlrc) = 0;

  // Returns the install date for the specified |id|.
  // "InstallDate" refers to the initial date that the given |id| was first
  // installed on the machine. Date information is returned by the server. If
  // "InstallDate" is not known, -2 is returned.
  virtual int GetInstallDate(const std::string& id) const = 0;

  // Sets InstallDate. This method should only be used for importing data from
  // other data stores.
  virtual void SetInstallDate(const std::string& id, int install_date) = 0;

  // These functions return cohort data for the specified |id|. "Cohort"
  // indicates the membership of the client in any release channels components
  // have set up in a machine-readable format, while "CohortName" does so in a
  // human-readable form. "CohortHint" indicates the client's channel selection
  // preference.
  virtual std::string GetCohort(const std::string& id) const = 0;
  virtual std::string GetCohortHint(const std::string& id) const = 0;
  virtual std::string GetCohortName(const std::string& id) const = 0;

  // These functions set cohort data for the specified |id|.
  virtual void SetCohort(const std::string& id, const std::string& cohort) = 0;
  virtual void SetCohortHint(const std::string& id,
                             const std::string& cohort_hint) = 0;
  virtual void SetCohortName(const std::string& id,
                             const std::string& cohort_name) = 0;

  // Calls `callback` with the subset of `ids` that are active. The callback
  // is called on the calling sequence.
  virtual void GetActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback)
      const = 0;

  // The following two functions returns the number of days since the last
  // time the client checked for update/was active.
  // -1 indicates that this is the first time the client reports
  // an update check/active for the specified |id|.
  // -2 indicates that the client has no information about the
  // update check/last active of the specified |id|.
  virtual int GetDaysSinceLastRollCall(const std::string& id) const = 0;
  virtual int GetDaysSinceLastActive(const std::string& id) const = 0;

  // These functions access |pv| data for the specified |id|. Returns an empty
  // version, if the version is not found.
  virtual base::Version GetProductVersion(const std::string& id) const = 0;
  virtual void SetProductVersion(const std::string& id,
                                 const base::Version& pv) = 0;

  // These functions access the maximum previous product version for the
  // specified |id|. Returns an empty version if the version is not found.
  // It will only be set if |max_version| exceeds the the existing value.
  virtual base::Version GetMaxPreviousProductVersion(
      const std::string& id) const = 0;
  virtual void SetMaxPreviousProductVersion(
      const std::string& id,
      const base::Version& max_version) = 0;

  // These functions access the fingerprint for the specified |id|.
  virtual std::string GetFingerprint(const std::string& id) const = 0;
  virtual void SetFingerprint(const std::string& id,
                              const std::string& fingerprint) = 0;

  // These functions get and set the time after which update checks are allowed.
  // To clear the throttle, pass base::Time().
  virtual base::Time GetThrottleUpdatesUntil() const = 0;
  virtual void SetThrottleUpdatesUntil(const base::Time& time) = 0;
};

// Creates a PersistedData instance. Passing null for either or both parameters
// is safe and will disable functionality that relies on them.
std::unique_ptr<PersistedData> CreatePersistedData(
    PrefService* pref_service,
    std::unique_ptr<ActivityDataService> activity_data_service);

// Register prefs for a PersistedData returned by CreatePersistedData.
void RegisterPersistedDataPrefs(PrefRegistrySimple* registry);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_
