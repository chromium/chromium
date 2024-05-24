// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_migrator.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyMap;

#if BUILDFLAG(IS_ANDROID)
namespace android {
class PolicyServiceAndroid;
}
#endif

class POLICY_EXPORT PolicyServiceImpl
    : public PolicyService,
      public ConfigurationPolicyProvider::Observer {
 public:
  using Providers =
      std::vector<raw_ptr<ConfigurationPolicyProvider, VectorExperimental>>;
  using Migrators = std::vector<std::unique_ptr<PolicyMigrator>>;

  static constexpr char kInitTimeHistogramPrefix[] =
      "Enterprise.PolicyServiceInitTime";
  static constexpr char kMachineHistogramSuffix[] = ".Machine";
  static constexpr char kUserHistogramSuffix[] = ".User";
  static constexpr char kWithoutPoliciesHistogramSuffix[] = ".WithoutPolicies";
  static constexpr char kWithPoliciesHistogramSuffix[] = ".WithPolicies";
  static constexpr char kWith1to50PoliciesHistogramSuffix[] =
      ".With_1_to_50_Policies";
  static constexpr char kWith51to100PoliciesHistogramSuffix[] =
      ".With_1_to_50_Policies";
  static constexpr char kWith101PlusPoliciesHistogramSuffix[] =
      ".With_101_Plus_Policies";

  // Represents the scope for the policy service to be logged via histograms.
  enum class ScopeForMetrics {
    // For cases when there is no need to collect metrics, e.g. in unit tests.
    kUnspecified,
    // Policy applicable to all profiles.
    kMachine,
    // Policy applicable only to the current signed-in user.
    kUser,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CloudUserAffiliationStatus {
    kUserOnly = 0,
    kDeviceAndUserUnaffiliated = 1,
    kDeviceAndUserAffiliated = 2,
    kMaxValue = kDeviceAndUserAffiliated,
  };

  // Creates a new PolicyServiceImpl with the list of
  // ConfigurationPolicyProviders, in order of decreasing priority.
  explicit PolicyServiceImpl(
      Providers providers,
      ScopeForMetrics scope_for_metrics = ScopeForMetrics::kUnspecified,
      Migrators migrators = std::vector<std::unique_ptr<PolicyMigrator>>());

  // Creates a new PolicyServiceImpl with the list of
  // ConfigurationPolicyProviders, in order of decreasing priority.
  // The created PolicyServiceImpl will only notify observers that
  // initialization has completed (for any domain) after
  // |UnthrottleInitialization| has been called.
  static std::unique_ptr<PolicyServiceImpl> CreateWithThrottledInitialization(
      Providers providers,
      ScopeForMetrics scope_for_metrics,
      Migrators migrators = std::vector<std::unique_ptr<PolicyMigrator>>());

  PolicyServiceImpl(const PolicyServiceImpl&) = delete;
  PolicyServiceImpl& operator=(const PolicyServiceImpl&) = delete;

  ~PolicyServiceImpl() override;

  // PolicyService overrides:
  void AddObserver(PolicyDomain domain,
                   PolicyService::Observer* observer) override;
  void RemoveObserver(PolicyDomain domain,
                      PolicyService::Observer* observer) override;
  void AddProviderUpdateObserver(ProviderUpdateObserver* observer) override;
  void RemoveProviderUpdateObserver(ProviderUpdateObserver* observer) override;
  bool HasProvider(ConfigurationPolicyProvider* provider) const override;
  const PolicyMap& GetPolicies(const PolicyNamespace& ns) const override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;
  void RefreshPolicies(base::OnceClosure callback,
                       PolicyFetchReason reason) override;
#if BUILDFLAG(IS_ANDROID)
  android::PolicyServiceAndroid* GetPolicyServiceAndroid() override;
#endif

  // If this PolicyServiceImpl has been created using
  // |CreateWithThrottledInitialization|, calling UnthrottleInitialization will
  // allow notification of observers that initialization has completed. If
  // initialization has actually completed previously but observers were not
  // notified yet because it was throttled, will notify observers synchronously.
  // Has no effect if initialization was not throttled.
  void UnthrottleInitialization();
  void UseLocalTestPolicyProvider(
      ConfigurationPolicyProvider* provider) override;

  // Precedence policies cannot be set at the user cloud level regardless of
  // affiliation status. This is done to prevent cloud users from potentially
  // giving themselves increased priority, causing a security issue.
  static void IgnoreUserCloudPrecedencePolicies(PolicyMap* policies);

  // Merges the policies from `bundles` into one bundle while respecting the
  // policy priorities and applying the appropriate `migrators`.
  static PolicyBundle MergePolicyBundles(
      std::vector<const policy::PolicyBundle*>& bundles,
      Migrators& migrators);

  // Records histograms for tracking policy service initialization time.
  // Visible for testing.
  static void RecordInitializationTime(ScopeForMetrics scope_for_metrics,
                                       size_t policy_count,
                                       base::TimeDelta initialization_time);

 private:
  enum class PolicyDomainStatus { kUninitialized, kInitialized, kPolicyReady };

  // This constructor is not publicly visible so callers that want a
  // PolicyServiceImpl with throttled initialization use
  // |CreateWithInitializationThrottled| for clarity.
  // If |initialization_throttled| is true, this PolicyServiceImpl will only
  // notify observers that initialization has completed (for any domain) after
  // |UnthrottleInitialization| has been called.
  PolicyServiceImpl(Providers providers,
                    ScopeForMetrics scope_for_metrics,
                    Migrators migrators,
                    bool initialization_throttled);

  // ConfigurationPolicyProvider::Observer overrides:
  void OnUpdatePolicy(ConfigurationPolicyProvider* provider) override;

  // Posts a task to notify observers of |ns| that its policies have changed,
  // passing along the |previous| and the |current| policies.
  void NotifyNamespaceUpdated(const PolicyNamespace& ns,
                              const PolicyMap& previous,
                              const PolicyMap& current);

  void NotifyProviderUpdatesPropagated();

  void NotifyPoliciesUpdated(const PolicyBundle& old_bundle);

  // Combines the policies from all the providers, and notifies the observers
  // of namespaces whose policies have been modified.
  void MergeAndTriggerUpdates();

  // Checks if all providers are initialized or have loaded their policies and
  // sets |policy_domain_status_| accordingly.
  // Returns the updated domains. The returned domains should be passed to
  // MaybeNotifyPolicyDomainStatusChange.
  std::vector<PolicyDomain> UpdatePolicyDomainStatus();

  // If initialization is not throttled, observers of |updated_domains| of the
  // initialization will be notified of the domains' initialization and of the
  // first policies being loaded. This function should only be called when
  // |updated_domains| just became initialized, just got its first policies or
  // when initialization has been unthrottled.
  void MaybeNotifyPolicyDomainStatusChange(
      const std::vector<PolicyDomain>& updated_domains);

  // Invokes all the refresh callbacks if there are no more refreshes pending.
  void CheckRefreshComplete();

  // Records histogram for the user's affiliation status determined by the
  // presence and contents of the device and user affiliation IDs.
  void RecordUserAffiliationStatus();

  // The providers, in order of decreasing priority.
  Providers providers_;
  raw_ptr<ConfigurationPolicyProvider> local_test_policy_provider_ = nullptr;

  Migrators migrators_;

  // Maps each policy namespace to its current policies.
  PolicyBundle policy_bundle_;

  // Maps each policy domain to its observer list.
  std::map<PolicyDomain,
           base::ObserverList<PolicyService::Observer, /*check_empty=*/true>>
      observers_;

  // The status of all the providers for the indexed policy domain.
  PolicyDomainStatus policy_domain_status_[POLICY_DOMAIN_SIZE];

  // Set of providers that have a pending update that was triggered by a
  // call to RefreshPolicies().
  std::set<raw_ptr<ConfigurationPolicyProvider, SetExperimental>>
      refresh_pending_;

  // List of callbacks to invoke once all providers refresh after a
  // RefreshPolicies() call.
  std::vector<base::OnceClosure> refresh_callbacks_;

  // Observers for propagation of policy updates by
  // ConfigurationPolicyProviders.
  base::ObserverList<ProviderUpdateObserver> provider_update_observers_;

  // Contains all ConfigurationPolicyProviders that have signaled a policy
  // update which is still being processed (i.e. for which a notification to
  // |provider_update_observers_| has not been sent out yet).
  // Note that this is intentionally a set - if multiple updates from the same
  // provider come in faster than they can be processed, they should only
  // trigger one notification to |provider_update_observers_|.
  std::set<raw_ptr<ConfigurationPolicyProvider, SetExperimental>>
      provider_update_pending_;

  // If this is true, IsInitializationComplete should be returning false for all
  // policy domains because the owner of this PolicyService is delaying the
  // initialization signal.
  bool initialization_throttled_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<android::PolicyServiceAndroid> policy_service_android_;
#endif

  // The time when the policy service was created, used for logging a histogram
  // that indicates how long it takes for the service's initialization to be
  // completed.
  base::Time creation_time_ = base::Time::Now();
  // The scope of the current policy service, to be used only for metrics
  // reporting.
  ScopeForMetrics scope_for_metrics_;

  // Used to verify usage in correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Used to create tasks to delay new policy updates while we may be already
  // processing previous policy updates.
  // All WeakPtrs will be reset in |RefreshPolicies| and |OnUpdatePolicy|.
  base::WeakPtrFactory<PolicyServiceImpl> update_task_ptr_factory_{this};

  // Used to protect against crbug.com/747817 until crbug.com/1221454 is done.
  base::WeakPtrFactory<PolicyServiceImpl> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_
