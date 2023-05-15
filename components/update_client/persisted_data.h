// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_
#define COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Version;
}  // namespace base

namespace update_client {

extern const char kPersistedDataPreference[];

class ActivityDataService;

// A PersistedData is a wrapper layer around a PrefService, designed to maintain
// update data that outlives the browser process and isn't exposed outside of
// update_client.
//
// The public methods of this class should be called only on the sequence that
// initializes it - which also has to match the sequence the PrefService has
// been initialized on.
class PersistedData {
 public:
  // Constructs a provider using the specified |pref_service| and
  // |activity_data_service|.
  // The associated preferences are assumed to already be registered.
  // The |pref_service| and |activity_data_service| must outlive the entire
  // update_client.
  PersistedData(PrefService* pref_service,
                ActivityDataService* activity_data_service);

  PersistedData(const PersistedData&) = delete;
  PersistedData& operator=(const PersistedData&) = delete;

  ~PersistedData();

  // Returns the DateLastRollCall (the server-localized calendar date number the
  // |id| was last checked by this client on) for the specified |id|.
  // -2 indicates that there is no recorded date number for the |id|.
  int GetDateLastRollCall(const std::string& id) const;

  // Returns the DateLastActive (the server-localized calendar date number the
  // |id| was last active by this client on) for the specified |id|.
  // -1 indicates that there is no recorded date for the |id| (i.e. this is the
  // first time the |id| is active).
  // -2 indicates that the |id| has an unknown value of last active date.
  int GetDateLastActive(const std::string& id) const;

  // Returns the PingFreshness (a random token that is written into the profile
  // data whenever the DateLastRollCall it is modified) for the specified |id|.
  // "" indicates that there is no recorded freshness value for the |id|.
  std::string GetPingFreshness(const std::string& id) const;

  // Records the DateLastRollcall for the specified `ids`. Also records
  // DateLastActive, if the ids have active bits currently set, and then clears
  // those bits. Rotates PingFreshness. Then, calls `callback` on the calling
  // sequence. Calls with a negative `datenum` or that occur prior to the
  // initialization of the persisted data store will simply post the callback
  // immediately.
  void SetDateLastData(const std::vector<std::string>& ids,
                       int datenum,
                       base::OnceClosure callback);

  // This is called only via update_client's RegisterUpdateClientPreferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the install date for the specified |id|.
  // "InstallDate" refers to the initial date that the given |id| was first
  // installed on the machine. Date information is returned by the server. If
  // "InstallDate" is not known, -2 is returned.
  int GetInstallDate(const std::string& id) const;

  // These functions return cohort data for the specified |id|. "Cohort"
  // indicates the membership of the client in any release channels components
  // have set up in a machine-readable format, while "CohortName" does so in a
  // human-readable form. "CohortHint" indicates the client's channel selection
  // preference.
  std::string GetCohort(const std::string& id) const;
  std::string GetCohortHint(const std::string& id) const;
  std::string GetCohortName(const std::string& id) const;

  // These functions set cohort data for the specified |id|.
  void SetCohort(const std::string& id, const std::string& cohort);
  void SetCohortHint(const std::string& id, const std::string& cohort_hint);
  void SetCohortName(const std::string& id, const std::string& cohort_name);

  // Calls `callback` with the subset of `ids` that are active. The callback
  // is called on the calling sequence.
  void GetActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback) const;

  // The following two functions returns the number of days since the last
  // time the client checked for update/was active.
  // -1 indicates that this is the first time the client reports
  // an update check/active for the specified |id|.
  // -2 indicates that the client has no information about the
  // update check/last active of the specified |id|.
  int GetDaysSinceLastRollCall(const std::string& id) const;
  int GetDaysSinceLastActive(const std::string& id) const;

  // These functions access |pv| data for the specified |id|. Returns an empty
  // version, if the version is not found.
  base::Version GetProductVersion(const std::string& id) const;
  void SetProductVersion(const std::string& id, const base::Version& pv);

  // These functions access the fingerprint for the specified |id|.
  std::string GetFingerprint(const std::string& id) const;
  void SetFingerprint(const std::string& id, const std::string& fingerprint);

 private:
  // Returns nullptr if the app key does not exist.
  const base::Value::Dict* GetAppKey(const std::string& id) const;

  // Returns an existing or newly created app key under a root pref.
  base::Value::Dict* GetOrCreateAppKey(const std::string& id,
                                       base::Value::Dict& root);

  // Returns fallback if the key does not exist.
  int GetInt(const std::string& id, const std::string& key, int fallback) const;

  // Returns the empty string if the key does not exist.
  std::string GetString(const std::string& id, const std::string& key) const;

  void SetString(const std::string& id,
                 const std::string& key,
                 const std::string& value);

  void SetDateLastDataHelper(const std::vector<std::string>& ids,
                             int datenum,
                             base::OnceClosure callback,
                             const std::set<std::string>& active_ids);

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  raw_ptr<ActivityDataService> activity_data_service_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PERSISTED_DATA_H_
