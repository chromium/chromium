// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_SERVICE_H_
#define CHROME_UPDATER_POLICY_SERVICE_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/manager.h"

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace updater {

class PolicyFetcher;

// This class contains the aggregate status of a policy value. It determines
// whether a conflict exists when multiple policy providers set the same policy.
// Instances are logically true if an effective policy is set.
template <typename T>
class PolicyStatus {
 public:
  struct Entry {
    Entry(const std::string& s, T p) : source(s), policy(p) {}
    std::string source;
    T policy{};
  };

  PolicyStatus() = default;
  PolicyStatus(const PolicyStatus&) = default;
  PolicyStatus& operator=(const PolicyStatus&) = default;

  void AddPolicyIfNeeded(bool is_managed,
                         const std::string& source,
                         const T& policy) {
    if (conflict_policy_) {
      return;  // We already have enough policies.
    }

    if (!effective_policy_ && is_managed) {
      effective_policy_ = std::make_optional<Entry>(source, policy);
    } else if (effective_policy_ &&
               policy != effective_policy_.value().policy) {
      conflict_policy_ = std::make_optional<Entry>(source, policy);
    }
  }

  const std::optional<Entry>& effective_policy() const {
    return effective_policy_;
  }
  const std::optional<Entry>& conflict_policy() const {
    return conflict_policy_;
  }

  explicit operator bool() const { return effective_policy_.has_value(); }
  // Convenience method to extract the effective policy's value.
  const T& policy() const {
    CHECK(effective_policy_);
    return effective_policy_->policy;
  }
  const T& policy_or(const T& fallback) const {
    return effective_policy_ ? policy() : fallback;
  }

 private:
  std::optional<Entry> effective_policy_;
  std::optional<Entry> conflict_policy_;
};

// The PolicyService returns policies for enterprise managed machines from the
// source with the highest priority where the policy available.
// This class is sequence affine and its instance is bound to the main sequence.
class PolicyService : public base::RefCountedThreadSafe<PolicyService> {
 public:
  class PolicyManagers {
   public:
    explicit PolicyManagers(
        scoped_refptr<ExternalConstants> external_constants);
    ~PolicyManagers();

    void ResetDeviceManagementManager(
        scoped_refptr<PolicyManagerInterface> dm_manager);

    std::vector<scoped_refptr<PolicyManagerInterface>> managers() const {
      return managers_;
    }

    void SetManagersForTesting(
        std::vector<scoped_refptr<PolicyManagerInterface>> managers);

   private:
    void CreateManagers(scoped_refptr<ExternalConstants> external_constants);
    void InitializeManagersVector();
    void SortManagersVector();
    static bool CloudPolicyOverridesPlatformPolicy(
        const std::vector<scoped_refptr<PolicyManagerInterface>>& providers);

    std::vector<scoped_refptr<PolicyManagerInterface>> managers_;
    scoped_refptr<PolicyManagerInterface> dm_policy_manager_;
    scoped_refptr<PolicyManagerInterface> external_constants_policy_manager_;
    scoped_refptr<PolicyManagerInterface> platform_policy_manager_;
    scoped_refptr<PolicyManagerInterface> default_policy_manager_;
  };

  PolicyService(scoped_refptr<ExternalConstants> external_constants,
                scoped_refptr<PersistedData> persisted_data,
                bool is_ceca_experiment_enabled);
  PolicyService(const PolicyService&) = delete;
  PolicyService& operator=(const PolicyService&) = delete;

  // Fetches policies from device management and updates the PolicyService
  // instance. `callback` is passed a result that is `kErrorOk` on success,
  // `kErrorDMRegistrationFailed` if DM registration fails, or any other error.
  // While a call to FetchPolicies is outstanding (i.e. has not invoked the
  // callback), concurrent calls to FetchPolicies will reuse the results of the
  // outstanding request.
  void FetchPolicies(policy::PolicyFetchReason reason,
                     base::OnceCallback<void(int)> callback);

  std::string source() const;

  // These methods call and aggregate the results from the policy managers.
  PolicyStatus<bool> CloudPolicyOverridesPlatformPolicy() const;
  PolicyStatus<base::TimeDelta> GetLastCheckPeriod() const;
  PolicyStatus<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes() const;
  PolicyStatus<std::string> GetDownloadPreference() const;
  PolicyStatus<int> GetPackageCacheSizeLimitMBytes() const;
  PolicyStatus<int> GetPackageCacheExpirationTimeDays() const;
  PolicyStatus<int> GetPolicyForAppInstalls(const std::string& app_id) const;
  PolicyStatus<int> GetPolicyForAppUpdates(const std::string& app_id) const;
  PolicyStatus<std::string> GetTargetChannel(const std::string& app_id) const;
  PolicyStatus<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const;
  PolicyStatus<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const;
  PolicyStatus<std::string> GetProxyMode() const;
  PolicyStatus<std::string> GetProxyPacUrl() const;
  PolicyStatus<std::string> GetProxyServer() const;
  PolicyStatus<std::vector<std::string>> GetForceInstallApps() const;

  // DEPRECATED: Prefer |GetLastCheckPeriod|. This function should only be used
  // in legacy interfaces where a PolicyStatus<int> is required.
  PolicyStatus<int> DeprecatedGetLastCheckPeriodMinutes() const;

  // Helper methods.
  base::Value GetAllPolicies() const;
  std::string GetAllPoliciesAsString() const;
  bool AreUpdatesSuppressedNow(base::Time now = base::Time::Now()) const;

  // Returns whether the Chrome Enterprise Companion App experiment is enabled.
  bool IsCecaExperimentEnabled() const { return is_ceca_experiment_enabled_; }

  // Queries whether the machine appears to be cloud managed by Chrome
  // Enterprise Core (formerly Chrome Enterprise Cloud Management).
  void IsCloudManaged(base::OnceCallback<void(bool)> callback) const;

  void SetManagersForTesting(
      std::vector<scoped_refptr<PolicyManagerInterface>> managers);

 protected:
  virtual ~PolicyService();

 private:
  friend class base::RefCountedThreadSafe<PolicyService>;

  template <typename T>
  using PolicyQueryFunction =
      std::optional<T> (PolicyManagerInterface::*)() const;
  template <typename T>
  using AppPolicyQueryFunction =
      std::optional<T> (PolicyManagerInterface::*)(const std::string&) const;

  void DoFetchPolicies(policy::PolicyFetchReason reason,
                       base::OnceCallback<void(int)> callback,
                       bool has_enrollment_token);

  // Called when `FetchPolicies` has completed. If `dm_policy_manager` is valid,
  // the policy managers within the policy service are reloaded/reset with the
  // provided DM policy manager.
  void FetchPoliciesDone(
      scoped_refptr<PolicyFetcher> fetcher,
      int result,
      scoped_refptr<PolicyManagerInterface> dm_policy_manager);

  // Queries the policy from the managed policy providers and determines the
  // policy status. The provided `transform` can be used to modify the queried
  // value to be a different type, or to nullify it when invalid.
  template <typename T, typename U = T>
  PolicyStatus<U> QueryPolicy(
      PolicyQueryFunction<T> policy_query_function,
      const base::RepeatingCallback<std::optional<U>(std::optional<T>)>&
          transform = base::NullCallback()) const;

  // Queries app policy from the managed policy providers and determines the
  // policy status.
  template <typename T>
  PolicyStatus<T> QueryAppPolicy(
      AppPolicyQueryFunction<T> policy_query_function,
      const std::string& app_id) const;

  std::set<std::string> GetAppsWithPolicy() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // List of policy providers in descending order of priority. All managed
  // providers should be ahead of non-managed providers.
  // Also contains a named map indexed by `source()` for all the policy
  // managers.
  PolicyManagers policy_managers_;
  const scoped_refptr<ExternalConstants> external_constants_;

  base::OnceCallback<void(int)> fetch_policies_callback_;
  scoped_refptr<PersistedData> persisted_data_;
  const bool is_ceca_experiment_enabled_;
};

// Decouples the proxy configuration from `PolicyService`.
struct PolicyServiceProxyConfiguration {
  PolicyServiceProxyConfiguration();
  ~PolicyServiceProxyConfiguration();
  PolicyServiceProxyConfiguration(const PolicyServiceProxyConfiguration&);
  PolicyServiceProxyConfiguration(PolicyServiceProxyConfiguration&&);
  PolicyServiceProxyConfiguration& operator=(
      const PolicyServiceProxyConfiguration&);
  PolicyServiceProxyConfiguration& operator=(PolicyServiceProxyConfiguration&&);

  // Note `Get()` returns a nullopt when there's no proxy policies. Otherwise
  // `proxy_auto_detect` must have a value, and is only set to true when the
  // policy chooses "auto-detect".
  static std::optional<PolicyServiceProxyConfiguration> Get(
      scoped_refptr<PolicyService> policy_service);

  bool proxy_auto_detect = false;
  std::optional<std::string> proxy_pac_url;
  std::optional<std::string> proxy_url;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_SERVICE_H_
