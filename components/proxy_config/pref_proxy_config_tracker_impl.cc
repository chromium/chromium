// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs_utils.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/enterprise/net/core/proxy_policy_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

namespace {

constexpr net::NetworkTrafficAnnotationTag
    kSettingsProxyConfigTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("proxy_config_settings", R"(
      semantics {
        sender: "Proxy Config"
        description:
          "Creates a proxy based on configuration received from settings."
        trigger:
          "On start up, or on any change of proxy settings."
        data:
          "Proxy configurations."
        destination: OTHER
        destination_other:
          "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can choose the proxy configurations in settings under "
          "'Advanced/Network/Change proxy settings...'."
        policy_exception_justification:
          "Using 'ProxySettings' policy can set Chrome to use specific "
          "proxy settings."
      })");

// Returns true if proxy override rules were written to `config`.
bool SetProxyOverrideRules(const PrefService* pref_service,
                           net::ProxyConfigWithAnnotation* config,
                           policy::PolicyService* policy_service) {
  const PrefService::Preference* pref =
      pref_service->FindPreference(proxy_config::prefs::kProxyOverrideRules);
  DCHECK(pref);

  if (!pref->GetValue() || !pref->GetValue()->is_list()) {
    return false;
  }

  const base::ListValue& rules_list =
      pref_service->GetList(proxy_config::prefs::kProxyOverrideRules);
  if (rules_list.empty() ||
      !base::FeatureList::IsEnabled(kEnableProxyOverrideRules) ||
      !proxy_config::ProxyOverrideRulesAllowed(pref_service, policy_service)) {
    return false;
  }

  std::vector<net::ProxyConfig::ProxyOverrideRule> proxy_override_rules =
      proxy_config::ParseProxyOverrideRules(rules_list);
  if (proxy_override_rules.empty()) {
    return false;
  }

  net::ProxyConfig new_config(config->value());
  new_config.set_proxy_override_rules(std::move(proxy_override_rules));
  *config =
      net::ProxyConfigWithAnnotation(new_config, config->traffic_annotation());
  return true;
}

// Returns true if the proxy config contains active dynamic routing rules or
// an in-progress update.
bool HasDynamicProxyRules(const net::ProxyConfig& config) {
  return !config.dynamic_routing_config().routing_rules.empty() ||
         config.dynamic_routing_config().is_update_in_progress;
}

// Returns true if the proxy config contains explicit proxy rules (Proxy
// Override Rules or PvD dynamic routing rules) that should overlay baseline
// configs.
bool HasExplicitProxyRules(const net::ProxyConfig& config) {
  return !config.proxy_override_rules().empty() || HasDynamicProxyRules(config);
}

// Returns true if `active_config` has dynamic routing rules that need to be
// attached to `config`.
bool ShouldAttachDynamicRoutingRules(
    const net::ProxyConfig& config,
    const net::ProxyConfig::DynamicRoutingConfig& active_config) {
  return !HasDynamicProxyRules(config) &&
         (!active_config.routing_rules.empty() ||
          active_config.is_update_in_progress);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
// Classifies whether currently active `kProxyOverrideRules` would be affected
// by enforcing affiliation on trusted platform-managed devices without CBCM.
proxy_config::ProxyPlatformAffiliationImpact ComputePlatformAffiliationImpact(
    const PrefService* pref_service,
    policy::PolicyService* policy_service) {
  if (!proxy_config::IsUserOrExtensionProxyOverrideRules(pref_service)) {
    return proxy_config::ProxyPlatformAffiliationImpact::kUnaffected;
  }

  const policy::PolicyMap& policies = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  if (!policies.GetDeviceAffiliationIds().empty() ||
      !enterprise_net::IsDevicePlatformManaged()) {
    return proxy_config::ProxyPlatformAffiliationImpact::kUnaffected;
  }

  return enterprise_net::IsUnaffiliatedUserProxyPolicyAllowed(policies)
             ? proxy_config::ProxyPlatformAffiliationImpact::
                   kAllowedByForAllUsers
             : proxy_config::ProxyPlatformAffiliationImpact::kWouldBeBlocked;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace

BASE_FEATURE(kEnableProxyOverrideRules, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kProxyOverrideRulesFixUnsetSystemProxy,
             base::FEATURE_ENABLED_BY_DEFAULT);

//============================= ProxyConfigServiceImpl =======================

ProxyConfigServiceImpl::ProxyConfigServiceImpl(
    std::unique_ptr<net::ProxyConfigService> base_service,
    ProxyPrefs::ConfigState initial_config_state,
    const net::ProxyConfigWithAnnotation& initial_config)
    : base_service_(std::move(base_service)),
      pref_config_state_(initial_config_state),
      pref_config_(initial_config),
      registered_observer_(false) {
  // ProxyConfigServiceImpl is created on the UI thread, but used on the network
  // thread.
  thread_checker_.DetachFromThread();
}

ProxyConfigServiceImpl::~ProxyConfigServiceImpl() {
  if (registered_observer_ && base_service_.get()) {
    base_service_->RemoveObserver(this);
  }
}

void ProxyConfigServiceImpl::AddObserver(
    net::ProxyConfigService::Observer* observer) {
  RegisterObserver();
  observers_.AddObserver(observer);
}

void ProxyConfigServiceImpl::RemoveObserver(
    net::ProxyConfigService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability
ProxyConfigServiceImpl::GetLatestProxyConfig(
    net::ProxyConfigWithAnnotation* config) {
  RegisterObserver();

  // Ask the base service if available.
  net::ProxyConfigWithAnnotation system_config;
  ConfigAvailability system_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  if (base_service_) {
    system_availability = base_service_->GetLatestProxyConfig(&system_config);
  }

  ProxyPrefs::ConfigState config_state;
  return PrefProxyConfigTrackerImpl::GetEffectiveProxyConfig(
      pref_config_state_, pref_config_, system_availability, system_config,
      false, &config_state, config);
}

void ProxyConfigServiceImpl::OnLazyPoll() {
  if (base_service_) {
    base_service_->OnLazyPoll();
  }
}

bool ProxyConfigServiceImpl::UsesPolling() {
  return base_service_ && base_service_->UsesPolling();
}

void ProxyConfigServiceImpl::UpdateProxyConfig(
    ProxyPrefs::ConfigState config_state,
    const net::ProxyConfigWithAnnotation& config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_config_state_ = config_state;
  pref_config_ = config;

  if (observers_.empty()) {
    return;
  }

  // Evaluate the proxy configuration. If GetLatestProxyConfig returns
  // CONFIG_PENDING, we are using the system proxy service, but it doesn't have
  // a valid configuration yet. Once it is ready, OnProxyConfigChanged() will be
  // called and broadcast the proxy configuration.
  // Note: If a switch between a preference proxy configuration and the system
  // proxy configuration occurs an unnecessary notification might get send if
  // the two configurations agree. This case should be rare however, so we don't
  // handle that case specially.
  net::ProxyConfigWithAnnotation new_config;
  ConfigAvailability availability = GetLatestProxyConfig(&new_config);
  if (availability != CONFIG_PENDING) {
    for (net::ProxyConfigService::Observer& observer : observers_) {
      observer.OnProxyConfigChanged(new_config, availability);
    }
  }
}

void ProxyConfigServiceImpl::OnProxyConfigChanged(
    const net::ProxyConfigWithAnnotation& config,
    ConfigAvailability availability) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check whether there is a proxy configuration defined by preferences. In
  // this case that proxy configuration takes precedence and the change event
  // from the delegate proxy config service can be disregarded.
  if (!PrefProxyConfigTrackerImpl::PrefPrecedes(pref_config_state_)) {
    net::ProxyConfigWithAnnotation actual_config;
    availability = GetLatestProxyConfig(&actual_config);
    for (net::ProxyConfigService::Observer& observer : observers_) {
      observer.OnProxyConfigChanged(actual_config, availability);
    }
  }
}

void ProxyConfigServiceImpl::RegisterObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!registered_observer_ && base_service_.get()) {
    base_service_->AddObserver(this);
    registered_observer_ = true;
  }
}

base::WeakPtr<ProxyConfigServiceImpl> ProxyConfigServiceImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

//========================= PrefProxyConfigTrackerImpl =========================

PrefProxyConfigTrackerImpl::PrefProxyConfigTrackerImpl(
    PrefService* pref_service,
    scoped_refptr<base::SingleThreadTaskRunner>
        proxy_config_service_task_runner,
    policy::PolicyService* policy_service,
    bool record_affiliation_metrics)
    : pref_service_(pref_service),
      policy_service_(policy_service),
      proxy_config_service_impl_(nullptr),
      proxy_config_service_task_runner_(proxy_config_service_task_runner)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
      ,
      record_affiliation_metrics_(record_affiliation_metrics)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
{
  pref_config_state_ =
      ReadPrefConfig(pref_service_, &pref_config_, policy_service_);
  active_config_state_ = pref_config_state_;
  active_config_ = pref_config_;

  proxy_prefs_.Init(pref_service);
  proxy_prefs_.Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&PrefProxyConfigTrackerImpl::OnProxyPrefChanged,
                          base::Unretained(this)));
  if (base::FeatureList::IsEnabled(kEnableProxyOverrideRules)) {
    proxy_prefs_.Add(
        proxy_config::prefs::kProxyOverrideRules,
        base::BindRepeating(&PrefProxyConfigTrackerImpl::OnProxyPrefChanged,
                            base::Unretained(this)));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
    proxy_prefs_.Add(
        proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers,
        base::BindRepeating(&PrefProxyConfigTrackerImpl::OnProxyPrefChanged,
                            base::Unretained(this)));
    proxy_prefs_.Add(
        proxy_config::prefs::kProxyOverrideRulesScope,
        base::BindRepeating(&PrefProxyConfigTrackerImpl::OnProxyPrefChanged,
                            base::Unretained(this)));
    if (policy_service_) {
      policy_service_observation_.Observe(policy_service_);
      if (record_affiliation_metrics_) {
        if (!policy_service_->IsInitializationComplete(
                policy::POLICY_DOMAIN_CHROME)) {
          policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
          is_observing_policy_service_init_ = true;
        } else {
          MaybeRecordProxyOverrideRulesAffiliationMetrics(pref_config_.value());
        }
      }
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
  }
}

#if BUILDFLAG(ENTERPRISE_PROXY)
PrefProxyConfigTrackerImpl::PrefProxyConfigTrackerImpl(
    PrefService* pref_service,
    scoped_refptr<base::SingleThreadTaskRunner>
        proxy_config_service_task_runner,
    policy::PolicyService* policy_service,
    enterprise_net::EnterpriseProxyService* enterprise_proxy_service,
    bool record_affiliation_metrics)
    : PrefProxyConfigTrackerImpl(pref_service,
                                 proxy_config_service_task_runner,
                                 policy_service,
                                 record_affiliation_metrics) {
  if (enterprise_proxy_service) {
    enterprise_proxy_observation_.Observe(enterprise_proxy_service);
    active_dynamic_routing_config_ =
        enterprise_proxy_observation_.GetSource()->GetDynamicRoutingConfig();
    OnProxyConfigChanged(pref_config_state_, pref_config_);
  }
}
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

PrefProxyConfigTrackerImpl::~PrefProxyConfigTrackerImpl() {
  DCHECK(pref_service_ == nullptr);
}

std::unique_ptr<net::ProxyConfigService>
PrefProxyConfigTrackerImpl::CreateTrackingProxyConfigService(
    std::unique_ptr<net::ProxyConfigService> base_service) {
  DCHECK(!proxy_config_service_impl_);
  auto* proxy_config_service_impl = new ProxyConfigServiceImpl(
      std::move(base_service), active_config_state_, active_config_);
  proxy_config_service_impl_ = proxy_config_service_impl->AsWeakPtr();
  VLOG(1) << this << ": set chrome proxy config service to "
          << proxy_config_service_impl;

  return base::WrapUnique<net::ProxyConfigService>(proxy_config_service_impl);
}

void PrefProxyConfigTrackerImpl::DetachFromPrefService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Stop notifications.
  proxy_prefs_.RemoveAll();
  pref_service_ = nullptr;
  proxy_config_service_impl_ = nullptr;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  if (is_observing_policy_service_init_ && policy_service_) {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    is_observing_policy_service_init_ = false;
  }
  // Stop observing policy provider updates for user affiliation changes.
  policy_service_observation_.Reset();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
  policy_service_ = nullptr;
#if BUILDFLAG(ENTERPRISE_PROXY)
  enterprise_proxy_observation_.Reset();
#endif  // BUILDFLAG(ENTERPRISE_PROXY)
}

// static
bool PrefProxyConfigTrackerImpl::PrefPrecedes(
    ProxyPrefs::ConfigState config_state) {
  return config_state == ProxyPrefs::CONFIG_POLICY ||
         config_state == ProxyPrefs::CONFIG_EXTENSION ||
         config_state == ProxyPrefs::CONFIG_OTHER_PRECEDE;
}

// static
net::ProxyConfigService::ConfigAvailability
PrefProxyConfigTrackerImpl::GetEffectiveProxyConfig(
    ProxyPrefs::ConfigState pref_state,
    const net::ProxyConfigWithAnnotation& pref_config,
    net::ProxyConfigService::ConfigAvailability system_availability,
    const net::ProxyConfigWithAnnotation& system_config,
    bool ignore_fallback_config,
    ProxyPrefs::ConfigState* effective_config_state,
    net::ProxyConfigWithAnnotation* effective_config) {
  *effective_config_state = pref_state;

  if (PrefPrecedes(pref_state)) {
    *effective_config = pref_config;
    return net::ProxyConfigService::CONFIG_VALID;
  }

  if (system_availability == net::ProxyConfigService::CONFIG_UNSET) {
    // If there's no system proxy config, fall back to prefs, explicit rules,
    // or default.
    if ((pref_state == ProxyPrefs::CONFIG_FALLBACK &&
         !ignore_fallback_config) ||
        (base::FeatureList::IsEnabled(kProxyOverrideRulesFixUnsetSystemProxy) &&
         HasExplicitProxyRules(pref_config.value()))) {
      *effective_config = pref_config;
    } else {
      *effective_config = net::ProxyConfigWithAnnotation::CreateDirect();
    }
    return net::ProxyConfigService::CONFIG_VALID;
  }

  *effective_config_state = ProxyPrefs::CONFIG_SYSTEM;
  if (!HasExplicitProxyRules(pref_config.value())) {
    *effective_config = system_config;
  } else {
    // When system proxy settings are active, overlay explicit rules (Proxy
    // Override Rules and PvD dynamic routing rules) onto the baseline system
    // configuration.
    net::ProxyConfig new_config = system_config.value();
    new_config.set_proxy_override_rules(
        pref_config.value().proxy_override_rules());
    new_config.set_dynamic_routing_config(
        pref_config.value().dynamic_routing_config());
    *effective_config = net::ProxyConfigWithAnnotation(
        new_config, system_config.traffic_annotation());
  }

  return system_availability;
}

// static
void PrefProxyConfigTrackerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(proxy_config::prefs::kProxy,
                                   ProxyConfigDictionary::CreateSystem());
  registry->RegisterListPref(proxy_config::prefs::kProxyOverrideRules);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  registry->RegisterIntegerPref(
      proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers, 0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(proxy_config::prefs::kProxyOverrideRulesScope,
                                0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
}

// static
void PrefProxyConfigTrackerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(proxy_config::prefs::kProxy,
                                   ProxyConfigDictionary::CreateSystem());
  registry->RegisterBooleanPref(proxy_config::prefs::kUseSharedProxies, false);
  registry->RegisterListPref(proxy_config::prefs::kProxyOverrideRules);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  registry->RegisterIntegerPref(
      proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers, 0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(proxy_config::prefs::kProxyOverrideRulesScope,
                                0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
}

// static
ProxyPrefs::ConfigState PrefProxyConfigTrackerImpl::ReadPrefConfig(
    const PrefService* pref_service,
    net::ProxyConfigWithAnnotation* config,
    policy::PolicyService* policy_service) {
  // Clear the configuration and source.
  *config = net::ProxyConfigWithAnnotation();
  const PrefService::Preference* pref =
      pref_service->FindPreference(proxy_config::prefs::kProxy);
  DCHECK(pref);

  const base::DictValue& dict =
      pref_service->GetDict(proxy_config::prefs::kProxy);
  ProxyConfigDictionary proxy_dict(dict.Clone());

  ProxyPrefs::ConfigState state = ProxyPrefs::CONFIG_OTHER_PRECEDE;
  if (!PrefConfigToNetConfig(proxy_dict, config)) {
    state = ProxyPrefs::CONFIG_UNSET;
  } else if (pref->IsUserModifiable() && !pref->HasUserSetting()) {
    state = ProxyPrefs::CONFIG_FALLBACK;
  } else if (pref->IsManaged()) {
    state = ProxyPrefs::CONFIG_POLICY;
  } else if (pref->IsExtensionControlled()) {
    state = ProxyPrefs::CONFIG_EXTENSION;
  }

  const PrefService::Preference* proxy_override_pref =
      pref_service->FindPreference(proxy_config::prefs::kProxyOverrideRules);
  if (SetProxyOverrideRules(pref_service, config, policy_service) &&
      state == ProxyPrefs::CONFIG_UNSET) {
    if (proxy_override_pref->IsManaged()) {
      state = ProxyPrefs::CONFIG_POLICY_OVERRIDE;
    } else {
      state = ProxyPrefs::CONFIG_EXTENSION_OVERRIDE;
    }
  }

  return state;
}

ProxyPrefs::ConfigState PrefProxyConfigTrackerImpl::GetProxyConfig(
    net::ProxyConfigWithAnnotation* config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pref_config_state_ != ProxyPrefs::CONFIG_UNSET) {
    *config = pref_config_;
  }
  return pref_config_state_;
}

void PrefProxyConfigTrackerImpl::OnProxyConfigChanged(
    ProxyPrefs::ConfigState config_state,
    const net::ProxyConfigWithAnnotation& config) {
  net::ProxyConfigWithAnnotation config_with_dynamic_routes = config;

  if (ShouldAttachDynamicRoutingRules(config.value(),
                                      active_dynamic_routing_config_)) {
    // When no other proxy rules (e.g. CONFIG_UNSET), create proxy config with
    // DIRECT and CONFIG_POLICY_DYNAMIC_ROUTING state. This ensures transmission
    // to the Network Process and fallback to DIRECT connection (or system OS
    // proxy if configured), in cases such as  the clearing of proxy
    // preferences.
    if (config_state == ProxyPrefs::CONFIG_UNSET) {
      config_with_dynamic_routes =
          net::ProxyConfigWithAnnotation::CreateDirect();
      config_state = ProxyPrefs::CONFIG_POLICY_DYNAMIC_ROUTING;
    }

    net::ProxyConfig pc = config_with_dynamic_routes.value();
    pc.set_dynamic_routing_config(active_dynamic_routing_config_);
    config_with_dynamic_routes = net::ProxyConfigWithAnnotation(
        pc, config_with_dynamic_routes.traffic_annotation());
  }

  // If the configuration hasn't changed, do nothing.
  if (active_config_state_ == config_state &&
      (active_config_state_ == ProxyPrefs::CONFIG_UNSET ||
       active_config_.value().Equals(config_with_dynamic_routes.value()))) {
    return;
  }

  active_config_state_ = config_state;
  if (active_config_state_ != ProxyPrefs::CONFIG_UNSET) {
    active_config_ = config_with_dynamic_routes;
  }

  if (!proxy_config_service_impl_) {
    return;
  }

  // If the ProxyConfigService lives on the current thread, just synchronously
  // tell it about the new configuration.
  // TODO(mmenke): When/if iOS is migrated to using the NetworkService, get rid
  // of |proxy_config_service_task_runner_|. Can also merge
  // ProxyConfigServiceImpl into the tracker, and make the class talk over the
  // Mojo pipe directly, at that point.
  if (!proxy_config_service_task_runner_) {
    proxy_config_service_impl_->UpdateProxyConfig(config_state,
                                                  config_with_dynamic_routes);
    return;
  }

  proxy_config_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProxyConfigServiceImpl::UpdateProxyConfig,
                                proxy_config_service_impl_, config_state,
                                config_with_dynamic_routes));
}

#if BUILDFLAG(ENTERPRISE_PROXY)
void PrefProxyConfigTrackerImpl::OnDynamicProxyConfigsStatusChanged() {
  if (!enterprise_proxy_observation_.GetSource()) {
    return;
  }
  active_dynamic_routing_config_ =
      enterprise_proxy_observation_.GetSource()->GetDynamicRoutingConfig();
  net::ProxyConfigWithAnnotation config;
  ProxyPrefs::ConfigState config_state = GetProxyConfig(&config);
  OnProxyConfigChanged(config_state, config);
}

void PrefProxyConfigTrackerImpl::OnEnterpriseProxyServiceDestroyed() {
  enterprise_proxy_observation_.Reset();
}
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
void PrefProxyConfigTrackerImpl::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (domain != policy::POLICY_DOMAIN_CHROME) {
    return;
  }
  if (is_observing_policy_service_init_ && policy_service_) {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    is_observing_policy_service_init_ = false;
  }
  OnProxyPrefChanged();
}

void PrefProxyConfigTrackerImpl::OnProviderUpdatePropagated(
    policy::ConfigurationPolicyProvider* provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  OnProxyPrefChanged();
}

std::optional<PrefProxyConfigTrackerImpl::AffiliationMetricsSource>
PrefProxyConfigTrackerImpl::GetAffiliationMetricsSource() const {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(proxy_config::prefs::kProxyOverrideRules);
  CHECK(pref);
  // `MANAGED_STORE` strictly shadows `EXTENSION_STORE` in `PrefValueStore`
  // (`IsExtensionControlled()` returns false while `IsManaged()` is true). Only
  // the active controlling source is reported so shadowed extension settings
  // are not counted unless policy is removed and the extension takes control.
  if (pref->IsManaged()) {
    return AffiliationMetricsSource::kPolicy;
  }
  if (pref->IsExtensionControlled()) {
    return AffiliationMetricsSource::kExtension;
  }
  return std::nullopt;
}

void PrefProxyConfigTrackerImpl::
    MaybeRecordProxyOverrideRulesAffiliationMetrics(
        const net::ProxyConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!record_affiliation_metrics_ || !pref_service_ || !policy_service_ ||
      !policy_service_->IsInitializationComplete(
          policy::POLICY_DOMAIN_CHROME) ||
      config.proxy_override_rules().empty()) {
    return;
  }

  const std::optional<AffiliationMetricsSource> source =
      GetAffiliationMetricsSource();
  if (!source) {
    return;
  }

  const proxy_config::ProxyPlatformAffiliationImpact impact =
      ComputePlatformAffiliationImpact(pref_service_, policy_service_);
  std::optional<proxy_config::ProxyPlatformAffiliationImpact>& last_recorded =
      *source == AffiliationMetricsSource::kPolicy
          ? last_recorded_policy_impact_
          : last_recorded_extension_impact_;
  if (last_recorded == impact) {
    return;
  }

  last_recorded = impact;
  const char* histogram_name =
      *source == AffiliationMetricsSource::kPolicy
          ? "Enterprise.ProxyOverrideRules.PlatformAffiliationImpact.Policy"
          : "Enterprise.ProxyOverrideRules.PlatformAffiliationImpact.Extension";
  base::UmaHistogramEnumeration(histogram_name, impact);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

bool PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(
    const ProxyConfigDictionary& proxy_dict,
    net::ProxyConfigWithAnnotation* config) {
  return proxy_config::PrefConfigToNetConfig(
      proxy_dict, kSettingsProxyConfigTrafficAnnotation, config);
}

void PrefProxyConfigTrackerImpl::OnProxyPrefChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyConfigWithAnnotation new_config;
  ProxyPrefs::ConfigState config_state =
      ReadPrefConfig(pref_service_, &new_config, policy_service_);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  MaybeRecordProxyOverrideRulesAffiliationMetrics(new_config.value());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
  if (pref_config_state_ != config_state ||
      (pref_config_state_ != ProxyPrefs::CONFIG_UNSET &&
       !pref_config_.value().Equals(new_config.value()))) {
    pref_config_state_ = config_state;
    if (pref_config_state_ != ProxyPrefs::CONFIG_UNSET) {
      pref_config_ = new_config;
    }
    OnProxyConfigChanged(config_state, new_config);
  }
}
