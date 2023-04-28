// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_SERVICE_H_
#define CHROME_UPDATER_POLICY_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/policy/manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    if (conflict_policy_)
      return;  // We already have enough policies.

    if (!effective_policy_ && is_managed) {
      effective_policy_ = absl::make_optional<Entry>(source, policy);
    } else if (effective_policy_ &&
               policy != effective_policy_.value().policy) {
      conflict_policy_ = absl::make_optional<Entry>(source, policy);
    }
  }

  const absl::optional<Entry>& effective_policy() const {
    return effective_policy_;
  }
  const absl::optional<Entry>& conflict_policy() const {
    return conflict_policy_;
  }

  explicit operator bool() const { return effective_policy_.has_value(); }
  // Convenience method to extract the effective policy's value.
  const T& policy() {
    CHECK(effective_policy_);
    return effective_policy_->policy;
  }
  const T& policy_or(const T& fallback) {
    return effective_policy_ ? policy() : fallback;
  }

 private:
  absl::optional<Entry> effective_policy_;
  absl::optional<Entry> conflict_policy_;
};

// The PolicyService returns policies for enterprise managed machines from the
// source with the highest priority where the policy available.
// This class is sequence affine and its instance is bound to the main sequence.
class PolicyService : public base::RefCountedThreadSafe<PolicyService> {
 public:
  using PolicyManagerVector =
      std::vector<scoped_refptr<PolicyManagerInterface>>;
  using PolicyManagerNameMap =
      base::flat_map<std::string, scoped_refptr<PolicyManagerInterface>>;
  struct PolicyManagers {
    PolicyManagers(PolicyManagerVector manager_vector,
                   PolicyManagerNameMap manager_name_map);
    ~PolicyManagers();

    PolicyManagerVector vector;
    PolicyManagerNameMap name_map;
  };

  explicit PolicyService(PolicyManagerVector managers);
  explicit PolicyService(scoped_refptr<ExternalConstants> external_constants);
  PolicyService(const PolicyService&) = delete;
  PolicyService& operator=(const PolicyService&) = delete;

  // Fetches policies from device management and updates the PolicyService
  // instance. `callback` is passed a result that is `kErrorOk` on success,
  // `kErrorDMRegistrationFailed` if DM registration fails, or any other error.
  void FetchPolicies(base::OnceCallback<void(int)> callback);

  std::string source() const;

  PolicyStatus<base::TimeDelta> GetLastCheckPeriod() const;
  PolicyStatus<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes() const;
  PolicyStatus<std::string> GetDownloadPreferenceGroupPolicy() const;
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

  std::string GetAllPoliciesAsString() const;

 protected:
  virtual ~PolicyService();

 private:
  friend class base::RefCountedThreadSafe<PolicyService>;

  SEQUENCE_CHECKER(sequence_checker_);

  // Called when `FetchPolicies` has completed. If `dm_policy_manager` is valid,
  // the policy managers within the policy service are reloaded/reset with the
  // provided DM policy manager. The DM policy manager is preloaded separately
  // in a blocking sequence since it needs to do I/O to load policies.
  void FetchPoliciesDone(
      scoped_refptr<PolicyFetcher> fetcher,
      base::OnceCallback<void(int)> callback,
      int result,
      scoped_refptr<PolicyManagerInterface> dm_policy_manager);

  // List of policy providers in descending order of priority. All managed
  // providers should be ahead of non-managed providers.
  // Also contains a named map indexed by `source()` for all the policy
  // managers.
  PolicyManagers policy_managers_;

  const scoped_refptr<ExternalConstants> external_constants_;

  // Helper function to query the policy from the managed policy providers and
  // determines the policy status.
  template <typename T>
  PolicyStatus<T> QueryPolicy(
      const base::RepeatingCallback<absl::optional<T>(
          const PolicyManagerInterface*)>& policy_query_callback) const;

  // Helper function to query app policy from the managed policy providers and
  // determines the policy status.
  template <typename T>
  PolicyStatus<T> QueryAppPolicy(
      const base::RepeatingCallback<
          absl::optional<T>(const PolicyManagerInterface*,
                            const std::string& app_id)>& policy_query_callback,
      const std::string& app_id) const;

  std::set<std::string> GetAppsWithPolicy() const;
};

// Decouples the proxy configuration from `PolicyService`.
struct PolicyServiceProxyConfiguration {
  PolicyServiceProxyConfiguration();
  ~PolicyServiceProxyConfiguration();
  PolicyServiceProxyConfiguration(const PolicyServiceProxyConfiguration&);
  PolicyServiceProxyConfiguration& operator=(
      const PolicyServiceProxyConfiguration&);

  static absl::optional<PolicyServiceProxyConfiguration> Get(
      scoped_refptr<PolicyService> policy_service);

  absl::optional<bool> proxy_auto_detect;
  absl::optional<std::string> proxy_pac_url;
  absl::optional<std::string> proxy_url;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_SERVICE_H_
