// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_SERVICE_H_
#define CHROME_UPDATER_POLICY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/policy/manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

class PolicyFetcher;

// This class contains the aggregate status of a policy value. It determines
// whether a conflict exists when multiple policy providers set the same policy.
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

 private:
  absl::optional<Entry> effective_policy_;
  absl::optional<Entry> conflict_policy_;
};

// The PolicyService returns policies for enterprise managed machines from the
// source with the highest priority where the policy available.
// This class is sequence affine and its instance is bound to the main sequence.
// TODO(crbug.com/1358718) - modernize the public interface to return by value
// instead of two out-params.
class PolicyService : public base::RefCountedThreadSafe<PolicyService> {
 public:
  using PolicyManagerVector =
      std::vector<std::unique_ptr<PolicyManagerInterface>>;

  explicit PolicyService(PolicyManagerVector managers);
  explicit PolicyService(scoped_refptr<ExternalConstants> external_constants);
  PolicyService(const PolicyService&) = delete;
  PolicyService& operator=(const PolicyService&) = delete;

  // Fetches policies from device management and updates the PolicyService
  // instance. `callback` is passed a result that is `kErrorOk` on success,
  // `kErrorDMRegistrationFailed` if DM registration fails, or any other error.
  void FetchPolicies(base::OnceCallback<void(int)> callback);

  std::string source() const;

  bool GetLastCheckPeriodMinutes(PolicyStatus<int>* policy_status,
                                 int* minutes) const;
  bool GetUpdatesSuppressedTimes(
      PolicyStatus<UpdatesSuppressedTimes>* policy_status,
      UpdatesSuppressedTimes* suppressed_times) const;
  bool GetDownloadPreferenceGroupPolicy(
      PolicyStatus<std::string>* policy_status,
      std::string* download_preference) const;
  bool GetPackageCacheSizeLimitMBytes(PolicyStatus<int>* policy_status,
                                      int* cache_size_limit) const;
  bool GetPackageCacheExpirationTimeDays(PolicyStatus<int>* policy_status,
                                         int* cache_life_limit) const;
  bool GetEffectivePolicyForAppInstalls(const std::string& app_id,
                                        PolicyStatus<int>* policy_status,
                                        int* install_policy) const;
  bool GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                       PolicyStatus<int>* policy_status,
                                       int* update_policy) const;
  bool GetTargetChannel(const std::string& app_id,
                        PolicyStatus<std::string>* policy_status,
                        std::string* channel) const;
  bool GetTargetVersionPrefix(const std::string& app_id,
                              PolicyStatus<std::string>* policy_status,
                              std::string* target_version_prefix) const;
  bool IsRollbackToTargetVersionAllowed(const std::string& app_id,
                                        PolicyStatus<bool>* policy_status,
                                        bool* rollback_allowed) const;
  bool GetProxyMode(PolicyStatus<std::string>* policy_status,
                    std::string* proxy_mode) const;
  bool GetProxyPacUrl(PolicyStatus<std::string>* policy_status,
                      std::string* proxy_pac_url) const;
  bool GetProxyServer(PolicyStatus<std::string>* policy_status,
                      std::string* proxy_server) const;
  bool GetForceInstallApps(
      PolicyStatus<std::vector<std::string>>* policy_status,
      std::vector<std::string>* force_install_apps) const;

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
      base::OnceCallback<void(int)> callback,
      int result,
      std::unique_ptr<PolicyManagerInterface> dm_policy_manager);

  // List of policy providers in descending order of priority. All managed
  // providers should be ahead of non-managed providers.
  PolicyManagerVector policy_managers_;

  const scoped_refptr<ExternalConstants> external_constants_;
  const scoped_refptr<PolicyFetcher> policy_fetcher_;

  // Helper function to query the policy from the managed policy providers and
  // determines the policy status.
  template <typename T>
  bool QueryPolicy(
      const base::RepeatingCallback<bool(const PolicyManagerInterface*, T*)>&
          policy_query_callback,
      PolicyStatus<T>* policy_status,
      T* value) const;

  // Helper function to query app policy from the managed policy providers and
  // determines the policy status.
  template <typename T>
  bool QueryAppPolicy(
      const base::RepeatingCallback<bool(const PolicyManagerInterface*,
                                         const std::string& app_id,
                                         T*)>& policy_query_callback,
      const std::string& app_id,
      PolicyStatus<T>* policy_status,
      T* value) const;
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
