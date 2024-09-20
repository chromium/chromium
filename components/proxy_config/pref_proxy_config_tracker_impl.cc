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
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/base/proxy_server.h"
#include "net/net_buildflags.h"
#include "url/gurl.h"

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
}  // namespace

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

//========================= PrefProxyConfigTrackerImpl =========================

PrefProxyConfigTrackerImpl::PrefProxyConfigTrackerImpl(
    PrefService* pref_service,
    scoped_refptr<base::SingleThreadTaskRunner>
        proxy_config_service_task_runner)
    : pref_service_(pref_service),
      proxy_config_service_impl_(nullptr),
      proxy_config_service_task_runner_(proxy_config_service_task_runner) {
  pref_config_state_ = ReadPrefConfig(pref_service_, &pref_config_);
  active_config_state_ = pref_config_state_;
  active_config_ = pref_config_;

  proxy_prefs_.Init(pref_service);
  proxy_prefs_.Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&PrefProxyConfigTrackerImpl::OnProxyPrefChanged,
                          base::Unretained(this)));
}

PrefProxyConfigTrackerImpl::~PrefProxyConfigTrackerImpl() {
  DCHECK(pref_service_ == nullptr);
}

std::unique_ptr<net::ProxyConfigService>
PrefProxyConfigTrackerImpl::CreateTrackingProxyConfigService(
    std::unique_ptr<net::ProxyConfigService> base_service) {
  DCHECK(!proxy_config_service_impl_);
  proxy_config_service_impl_ = new ProxyConfigServiceImpl(
      std::move(base_service), active_config_state_, active_config_);
  VLOG(1) << this << ": set chrome proxy config service to "
          << proxy_config_service_impl_;

  return std::unique_ptr<net::ProxyConfigService>(proxy_config_service_impl_);
}

void PrefProxyConfigTrackerImpl::DetachFromPrefService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Stop notifications.
  proxy_prefs_.RemoveAll();
  pref_service_ = nullptr;
  proxy_config_service_impl_ = nullptr;
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
    // If there's no system proxy config, fall back to prefs or default.
    if (pref_state == ProxyPrefs::CONFIG_FALLBACK && !ignore_fallback_config) {
      *effective_config = pref_config;
    } else {
      *effective_config = net::ProxyConfigWithAnnotation::CreateDirect();
    }
    return net::ProxyConfigService::CONFIG_VALID;
  }

  *effective_config_state = ProxyPrefs::CONFIG_SYSTEM;
  *effective_config = system_config;
  return system_availability;
}

// static
void PrefProxyConfigTrackerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(proxy_config::prefs::kProxy,
                                   ProxyConfigDictionary::CreateSystem());
}

// static
void PrefProxyConfigTrackerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(proxy_config::prefs::kProxy,
                                   ProxyConfigDictionary::CreateSystem());
  registry->RegisterBooleanPref(proxy_config::prefs::kUseSharedProxies, false);
}

// static
ProxyPrefs::ConfigState PrefProxyConfigTrackerImpl::ReadPrefConfig(
    const PrefService* pref_service,
    net::ProxyConfigWithAnnotation* config) {
  // Clear the configuration and source.
  *config = net::ProxyConfigWithAnnotation();
  const PrefService::Preference* pref =
      pref_service->FindPreference(proxy_config::prefs::kProxy);
  DCHECK(pref);

  const base::Value::Dict& dict =
      pref_service->GetDict(proxy_config::prefs::kProxy);
  ProxyConfigDictionary proxy_dict(dict.Clone());

  if (!PrefConfigToNetConfig(proxy_dict, config)) {
    return ProxyPrefs::CONFIG_UNSET;
  }
  if (pref->IsUserModifiable() && !pref->HasUserSetting()) {
    return ProxyPrefs::CONFIG_FALLBACK;
  }
  if (pref->IsManaged()) {
    return ProxyPrefs::CONFIG_POLICY;
  }
  if (pref->IsExtensionControlled()) {
    return ProxyPrefs::CONFIG_EXTENSION;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (pref->IsStandaloneBrowserControlled()) {
    // The proxy config is controlled by an extension active in the Lacros
    // primary profile.
    return ProxyPrefs::CONFIG_EXTENSION;
  }
#endif
  return ProxyPrefs::CONFIG_OTHER_PRECEDE;
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
  // If the configuration hasn't changed, do nothing.
  if (active_config_state_ == config_state &&
      (active_config_state_ == ProxyPrefs::CONFIG_UNSET ||
       active_config_.value().Equals(config.value()))) {
    return;
  }

  active_config_state_ = config_state;
  if (active_config_state_ != ProxyPrefs::CONFIG_UNSET) {
    active_config_ = config;
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
    proxy_config_service_impl_->UpdateProxyConfig(config_state, config);
    return;
  }

  proxy_config_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProxyConfigServiceImpl::UpdateProxyConfig,
                                base::Unretained(proxy_config_service_impl_),
                                config_state, config));
}

bool PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(
    const ProxyConfigDictionary& proxy_dict,
    net::ProxyConfigWithAnnotation* config) {
  ProxyPrefs::ProxyMode mode;
  if (!proxy_dict.GetMode(&mode)) {
    // Fall back to system settings if the mode preference is invalid.
    return false;
  }
  net::ProxyConfig proxy_config = config->value();
  switch (mode) {
    case ProxyPrefs::MODE_SYSTEM:
      // Use system settings.
      return false;
    case ProxyPrefs::MODE_DIRECT:
      // Ignore all the other proxy config preferences if the use of a proxy
      // has been explicitly disabled.
      return true;
    case ProxyPrefs::MODE_AUTO_DETECT:
      proxy_config.set_auto_detect(true);
      *config = net::ProxyConfigWithAnnotation(
          proxy_config, kSettingsProxyConfigTrafficAnnotation);
      return true;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string proxy_pac;
      if (!proxy_dict.GetPacUrl(&proxy_pac)) {
        LOG(ERROR) << "Proxy settings request PAC script but do not specify "
                   << "its URL. Falling back to direct connection.";
        return true;
      }
      GURL proxy_pac_url(proxy_pac);
      if (!proxy_pac_url.is_valid()) {
        LOG(ERROR) << "Invalid proxy PAC url: " << proxy_pac;
        return true;
      }
      proxy_config.set_pac_url(proxy_pac_url);
      bool pac_mandatory = false;
      proxy_dict.GetPacMandatory(&pac_mandatory);
      proxy_config.set_pac_mandatory(pac_mandatory);
      *config = net::ProxyConfigWithAnnotation(
          proxy_config, kSettingsProxyConfigTrafficAnnotation);
      return true;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      if (!proxy_dict.GetProxyServer(&proxy_server)) {
        LOG(ERROR) << "Proxy settings request fixed proxy servers but do not "
                   << "specify their URLs. Falling back to direct connection.";
        return true;
      }

      bool allow_bracketed_proxy_chains = false;
      bool allow_quic_proxy_support = false;

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
      allow_bracketed_proxy_chains = true;
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
      allow_quic_proxy_support = true;
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

      proxy_config.proxy_rules().ParseFromString(
          proxy_server, allow_bracketed_proxy_chains, allow_quic_proxy_support);

      std::string proxy_bypass;
      if (proxy_dict.GetBypassList(&proxy_bypass)) {
        proxy_config.proxy_rules().bypass_rules.ParseFromString(proxy_bypass);
      }
      *config = net::ProxyConfigWithAnnotation(
          proxy_config, kSettingsProxyConfigTrafficAnnotation);
      return true;
    }
    case ProxyPrefs::kModeCount: {
      // Fall through to NOTREACHED().
    }
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown proxy mode, falling back to system settings.";
  return false;
}

void PrefProxyConfigTrackerImpl::OnProxyPrefChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyConfigWithAnnotation new_config;
  ProxyPrefs::ConfigState config_state =
      ReadPrefConfig(pref_service_, &new_config);
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
