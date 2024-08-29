// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PERSISTED_DATA_H_
#define CHROME_UPDATER_PERSISTED_DATA_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/updater/updater_scope.h"
#include "components/update_client/persisted_data.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

class PrefService;
class PrefRegistrySimple;

namespace base {
class FilePath;
class Time;
class Version;
}  // namespace base

namespace update_client {
class ActivityDataService;
struct CategorizedError;
}  // namespace update_client

namespace updater {

struct RegistrationRequest;

// PersistedData uses the PrefService to persist updater data that outlives
// the updater processes.
class PersistedData : public base::RefCountedThreadSafe<PersistedData>,
                      public update_client::PersistedData {
 public:
  // Constructs a provider using the specified |pref_service|.
  // The associated preferences are assumed to already be registered.
  // The |pref_service| must outlive the instance of this class.
  PersistedData(
      UpdaterScope scope,
      PrefService* pref_service,
      std::unique_ptr<update_client::ActivityDataService> activity_service);
  PersistedData(const PersistedData&) = delete;
  PersistedData& operator=(const PersistedData&) = delete;

  // These functions access the version path for the specified id.
  base::FilePath GetProductVersionPath(const std::string& id) const;
  void SetProductVersionPath(const std::string& id, const base::FilePath& path);

  // These functions access the version key for the specified id.
  std::string GetProductVersionKey(const std::string& id) const;
  void SetProductVersionKey(const std::string& id, const std::string& value);

  // These functions access the existence checker path for the specified id.
  base::FilePath GetExistenceCheckerPath(const std::string& id) const;
  void SetExistenceCheckerPath(const std::string& id,
                               const base::FilePath& ecp);

  // These functions access the brand code for the specified id.
  std::string GetBrandCode(const std::string& id);
  void SetBrandCode(const std::string& id, const std::string& bc);

  // These functions access the brand path for the specified id.
  base::FilePath GetBrandPath(const std::string& id) const;
  void SetBrandPath(const std::string& id, const base::FilePath& bp);

  // These functions access the AP for the specified id.
  std::string GetAP(const std::string& id);
  void SetAP(const std::string& id, const std::string& ap);

  // These functions access the AP path for the specified id.
  base::FilePath GetAPPath(const std::string& id) const;
  void SetAPPath(const std::string& id, const base::FilePath& path);

  // These functions access the AP key for the specified id.
  std::string GetAPKey(const std::string& id) const;
  void SetAPKey(const std::string& id, const std::string& value);

  // This function sets any non-empty field in the registration request object
  // into the persistent data store.
  void RegisterApp(const RegistrationRequest& rq);

  // This function removes a registered application from the persistent store.
  bool RemoveApp(const std::string& id);

  // Returns the app ids of the applications registered in prefs, if the
  // application has a valid version.
  std::vector<std::string> GetAppIds() const;

  // HadApps is set when the updater processes a registration for an app other
  // than itself, and is never unset, even if the app is uninstalled.
  bool GetHadApps() const;
  void SetHadApps();

  // UsageStatsEnabled reflects whether the updater as a whole is allowed to
  // send usage stats, and is set or reset periodically based on the usage
  // stats opt-in state of each product.
  bool GetUsageStatsEnabled() const;
  void SetUsageStatsEnabled(bool usage_stats_enabled);

  // EulaRequired reflects whether some user responsible for this system has
  // accepted a EULA that covers the updater's operation or not. EulaRequired
  // defaults to false; refer to functional_spec.md for details.
  bool GetEulaRequired() const;
  void SetEulaRequired(bool eula_required);

  // LastChecked is set when the updater completed successfully a call to
  // `UpdateService::UpdateAll` as indicated by the `UpdateService::Result`
  // argument of the completion callback. This means that the execution path
  // for updating all applications works end to end, including communicating
  // with the backend.
  base::Time GetLastChecked() const;
  void SetLastChecked(const base::Time& time);

  // LastStarted is set when `UpdateService::RunPeriodicTasks` is called. This
  // indicates that the mechanism to initiate automated update checks is
  // working.
  base::Time GetLastStarted() const;
  void SetLastStarted(const base::Time& time);

#if BUILDFLAG(IS_WIN)
  // Retrieves the previously stored OS version.
  std::optional<OSVERSIONINFOEX> GetLastOSVersion() const;

  // Stores the current os version.
  void SetLastOSVersion();
#endif

  // update_client::PersistedData overrides:
  base::Version GetProductVersion(const std::string& id) const override;
  void SetProductVersion(const std::string& id,
                         const base::Version& pv) override;
  base::Version GetMaxPreviousProductVersion(
      const std::string& id) const override;
  void SetMaxPreviousProductVersion(const std::string& id,
                                    const base::Version& max_version) override;
  std::string GetFingerprint(const std::string& id) const override;
  void SetFingerprint(const std::string& id, const std::string& fp) override;
  int GetDateLastActive(const std::string& id) const override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  void SetDateLastActive(const std::string& id, int dla) override;
  int GetDateLastRollCall(const std::string& id) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;
  void SetDateLastRollCall(const std::string& id, int dlrc) override;
  std::string GetCohort(const std::string& id) const override;
  void SetCohort(const std::string& id, const std::string& cohort) override;
  std::string GetCohortName(const std::string& id) const override;
  void SetCohortName(const std::string& id,
                     const std::string& cohort_name) override;
  std::string GetCohortHint(const std::string& id) const override;
  void SetCohortHint(const std::string& id,
                     const std::string& cohort_hint) override;
  std::string GetPingFreshness(const std::string& id) const override;
  void SetDateLastData(const std::vector<std::string>& ids,
                       int datenum,
                       base::OnceClosure callback) override;
  int GetInstallDate(const std::string& id) const override;
  void SetInstallDate(const std::string& id, int install_date) override;
  void GetActiveBits(const std::vector<std::string>& ids,
                     base::OnceCallback<void(const std::set<std::string>&)>
                         callback) const override;
  base::Time GetThrottleUpdatesUntil() const override;
  void SetThrottleUpdatesUntil(const base::Time& time) override;
  void SetLastUpdateCheckError(
      const update_client::CategorizedError& error) override;

 private:
  friend class base::RefCountedThreadSafe<PersistedData>;
  ~PersistedData() override;

  // Returns nullptr if the app key does not exist.
  const base::Value::Dict* GetAppKey(const std::string& id) const;

  // Returns an existing or newly created app key under a root pref.
  base::Value::Dict* GetOrCreateAppKey(const std::string& id,
                                       base::Value::Dict& root);

  std::optional<int> GetInteger(const std::string& id,
                                const std::string& key) const;
  void SetInteger(const std::string& id, const std::string& key, int value);
  std::string GetString(const std::string& id, const std::string& key) const;
  void SetString(const std::string& id,
                 const std::string& key,
                 const std::string& value);

  SEQUENCE_CHECKER(sequence_checker_);

  const UpdaterScope scope_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_ = nullptr;
  std::unique_ptr<update_client::PersistedData> delegate_;
};

void RegisterPersistedDataPrefs(scoped_refptr<PrefRegistrySimple> registry);

}  // namespace updater

#endif  // CHROME_UPDATER_PERSISTED_DATA_H_
