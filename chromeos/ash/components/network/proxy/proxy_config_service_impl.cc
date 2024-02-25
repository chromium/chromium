// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_profile.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// Writes the proxy config of |network| to |proxy_config|.  Set |onc_source| to
// the source of this configuration. Returns false if no proxy was configured
// for this network.
bool GetNetworkProxyConfig(const PrefService* profile_prefs,
                           const PrefService* local_state_prefs,
                           const NetworkState& network,
                           net::ProxyConfigWithAnnotation* proxy_config,
                           ::onc::ONCSource* onc_source) {
  std::unique_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(
          profile_prefs, local_state_prefs, network,
          NetworkHandler::Get()->network_profile_handler(), onc_source);
  if (!proxy_dict)
    return false;
  return PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(*proxy_dict,
                                                           proxy_config);
}

// When the CaptivePortalPopupWindow feature is enabled, this code is
// responsible for determining whether proxies should be disabled for
// captive portal signin windows.
bool ProxiesDisabledForCaptivePortalSignin(const PrefService* profile_prefs) {
  // If the CaptivePortalPopupWindow feature is not enabled, the logic for
  // avoiding proxies for captive portal signin is handled in
  // NetworkPortalSigninController.
  if (!chromeos::features::IsCaptivePortalPopupWindowEnabled()) {
    return false;
  }
  // Prior to login only the signin Profile can be used with no special
  // behavior.
  if (!profile_prefs) {
    return false;
  }
  // The kCaptivePortalSignin pref identifies signin windows. If this is not
  // a signin window, do not disable proxies here.
  const PrefService::Preference* const captive_portal_pref =
      profile_prefs->FindPreference(chromeos::prefs::kCaptivePortalSignin);
  if (!captive_portal_pref || !captive_portal_pref->GetValue()->GetBool()) {
    return false;
  }
  // If the CaptivePortalAuthenticationIgnoresProxy pref is set to false by
  // policy, proxies should not be disabled for captive portal signin.
  const PrefService::Preference* const ignore_proxy_pref =
      profile_prefs->FindPreference(
          chromeos::prefs::kCaptivePortalAuthenticationIgnoresProxy);
  if (ignore_proxy_pref && !ignore_proxy_pref->GetValue()->GetBool()) {
    return false;
  }
  // Otherwise, disable proxies for captive portal signin.
  return true;
}

}  // namespace

ProxyConfigServiceImpl::ProxyConfigServiceImpl(
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : PrefProxyConfigTrackerImpl(
          profile_prefs ? profile_prefs : local_state_prefs,
          io_task_runner),
      profile_prefs_(profile_prefs),
      local_state_prefs_(local_state_prefs) {
  const base::RepeatingClosure proxy_change_callback = base::BindRepeating(
      &ProxyConfigServiceImpl::OnProxyPrefChanged, base::Unretained(this));

  if (profile_prefs) {
    profile_pref_registrar_.Init(profile_prefs);
    profile_pref_registrar_.Add(::onc::prefs::kOpenNetworkConfiguration,
                                proxy_change_callback);
    profile_pref_registrar_.Add(::proxy_config::prefs::kUseSharedProxies,
                                proxy_change_callback);
    profile_pref_registrar_.Add(chromeos::prefs::kCaptivePortalSignin,
                                proxy_change_callback);
  }
  local_state_pref_registrar_.Init(local_state_prefs);
  local_state_pref_registrar_.Add(::onc::prefs::kDeviceOpenNetworkConfiguration,
                                  proxy_change_callback);

  if (NetworkHandler::IsInitialized()) {  // null in unit tests.
    // Register for changes to the default network.
    NetworkStateHandler* state_handler =
        NetworkHandler::Get()->network_state_handler();
    network_state_handler_observer_.Observe(state_handler);
    DefaultNetworkChanged(state_handler->DefaultNetwork());
  }
}

ProxyConfigServiceImpl::~ProxyConfigServiceImpl() = default;

void ProxyConfigServiceImpl::OnProxyConfigChanged(
    ProxyPrefs::ConfigState config_state,
    const net::ProxyConfigWithAnnotation& config) {
  VLOG(1) << "Got prefs change: "
          << ProxyPrefs::ConfigStateToDebugString(config_state)
          << ", mode=" << static_cast<int>(config.value().proxy_rules().type);
  DetermineEffectiveConfigFromDefaultNetwork();
}

void ProxyConfigServiceImpl::OnProxyPrefChanged() {
  DetermineEffectiveConfigFromDefaultNetwork();
}

void ProxyConfigServiceImpl::DefaultNetworkChanged(
    const NetworkState* new_network) {
  std::string new_network_path;
  if (new_network)
    new_network_path = new_network->path();

  VLOG(1) << "DefaultNetworkChanged to '" << new_network_path << "'.";
  VLOG_IF(1, new_network) << "New network: name=" << new_network->name()
                          << ", profile=" << new_network->profile_path();

  // Even if the default network is the same, its proxy config (e.g. if private
  // version of network replaces the shared version after login), or
  // use-shared-proxies setting (e.g. after login) may have changed, so
  // re-determine effective proxy config, and activate if different.
  DetermineEffectiveConfigFromDefaultNetwork();
}

void ProxyConfigServiceImpl::OnShuttingDown() {
  // Ownership of this class is complicated. Stop observing NetworkStateHandler
  // when the class shuts down.
  network_state_handler_observer_.Reset();
}

// static
bool ProxyConfigServiceImpl::IgnoreProxy(const PrefService* profile_prefs,
                                         const std::string network_profile_path,
                                         ::onc::ONCSource onc_source) {
  if (!profile_prefs) {
    // If the profile preference are not available, this must be the object
    // associated to local state used for system requests or login-profile. Make
    // sure that proxies are enabled.
    VLOG(1) << "Use proxy for system requests and sign-in screen.";
    return false;
  }

  if (network_profile_path.empty())
    return true;

  const NetworkProfile* profile =
      NetworkHandler::Get()->network_profile_handler()->GetProfileForPath(
          network_profile_path);
  if (!profile) {
    VLOG(1) << "Unknown profile_path '" << network_profile_path
            << "'. Ignoring proxy.";
    return true;
  }
  if (profile->type() == NetworkProfile::TYPE_USER) {
    VLOG(1) << "Respect proxy of not-shared networks.";
    return false;
  }
  if (onc_source == ::onc::ONC_SOURCE_USER_POLICY) {
    // Note that this case can occur if the network is shared (e.g. ethernet)
    // but the proxy is determined by user policy.
    // See https://crbug.com/454966 .
    VLOG(1) << "Respect proxy from user policy although network is shared.";
    return false;
  }
  if (onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY) {
    const user_manager::User* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    if (!primary_user || primary_user->IsAffiliated()) {
      VLOG(1) << "Respecting proxy for network, as the primary user belongs to "
              << "the domain the device is enrolled to.";
      return false;
    }
  }

  // This network is shared and not managed by the user's domain.
  bool use_shared_proxies =
      profile_prefs->GetBoolean(::proxy_config::prefs::kUseSharedProxies);
  VLOG(1) << "Use proxy of shared network: " << use_shared_proxies;
  return !use_shared_proxies;
}

// static
std::unique_ptr<ProxyConfigDictionary>
ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(
    const PrefService* profile_prefs,
    const PrefService* local_state_prefs) {
  if (ProxiesDisabledForCaptivePortalSignin(profile_prefs)) {
    return nullptr;
  }

  // Apply Pref Proxy configuration if available.
  net::ProxyConfigWithAnnotation pref_proxy_config;
  ProxyPrefs::ConfigState pref_state =
      PrefProxyConfigTrackerImpl::ReadPrefConfig(profile_prefs,
                                                 &pref_proxy_config);
  if (PrefProxyConfigTrackerImpl::PrefPrecedes(pref_state)) {
    const PrefService::Preference* const pref =
        profile_prefs->FindPreference(::proxy_config::prefs::kProxy);
    DCHECK(pref->GetValue() && pref->GetValue()->is_dict());
    return std::make_unique<ProxyConfigDictionary>(
        pref->GetValue()->GetDict().Clone());
  }

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  // No connected network.
  if (!network) {
    return nullptr;
  }

  // Apply network proxy configuration.
  ::onc::ONCSource onc_source;
  std::unique_ptr<ProxyConfigDictionary> proxy_config =
      proxy_config::GetProxyConfigForNetwork(
          profile_prefs, local_state_prefs, *network,
          NetworkHandler::Get()->network_profile_handler(), &onc_source);
  if (!ProxyConfigServiceImpl::IgnoreProxy(
          profile_prefs, network->profile_path(), onc_source)) {
    return proxy_config;
  }

  return std::make_unique<ProxyConfigDictionary>(
      ProxyConfigDictionary::CreateDirect());
}

void ProxyConfigServiceImpl::DetermineEffectiveConfigFromDefaultNetwork() {
  if (!NetworkHandler::IsInitialized()) {
    return;
  }

  if (ProxiesDisabledForCaptivePortalSignin(profile_prefs_)) {
    return;
  }

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network = handler->DefaultNetwork();

  // Get prefs proxy config if available.
  net::ProxyConfigWithAnnotation pref_config;
  ProxyPrefs::ConfigState pref_state = GetProxyConfig(&pref_config);

  // Get network proxy config if available.
  net::ProxyConfigWithAnnotation network_config;
  net::ProxyConfigService::ConfigAvailability network_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  bool ignore_proxy = true;
  if (network) {
    ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
    const bool network_proxy_configured = GetNetworkProxyConfig(
        prefs(), local_state_prefs_, *network, &network_config, &onc_source);
    ignore_proxy =
        IgnoreProxy(profile_prefs_, network->profile_path(), onc_source);

    // If network is shared but use-shared-proxies is off, use direct mode.
    if (ignore_proxy) {
      network_config = net::ProxyConfigWithAnnotation::CreateDirect();
      network_availability = net::ProxyConfigService::CONFIG_VALID;
    } else if (network_proxy_configured) {
      // Network is private or shared with user using shared proxies.
      VLOG(1) << this << ": using proxy of network " << network->path();
      network_availability = net::ProxyConfigService::CONFIG_VALID;
    }
  }

  // Determine effective proxy config, either from prefs or network.
  ProxyPrefs::ConfigState effective_config_state;
  net::ProxyConfigWithAnnotation effective_config;
  GetEffectiveProxyConfig(pref_state, pref_config, network_availability,
                          network_config, ignore_proxy, &effective_config_state,
                          &effective_config);

  // If effective config is from system (i.e. network), it's considered a
  // special kind of prefs that ranks below policy/extension but above
  // others, so bump it up to CONFIG_OTHER_PRECEDE to force its precedence
  // when PrefProxyConfigTrackerImpl pushes it to ChromeProxyConfigService.
  if (effective_config_state == ProxyPrefs::CONFIG_SYSTEM)
    effective_config_state = ProxyPrefs::CONFIG_OTHER_PRECEDE;
  // If config is manual, add rule to bypass local host.
  if (effective_config.value().proxy_rules().type !=
      net::ProxyConfig::ProxyRules::Type::EMPTY) {
    net::ProxyConfig proxy_config = effective_config.value();
    // TODO(https://crbug.com/902418): Is this rule still needed?
    proxy_config.proxy_rules()
        .bypass_rules.PrependRuleToBypassSimpleHostnames();
    effective_config = net::ProxyConfigWithAnnotation(
        proxy_config, effective_config.traffic_annotation());
  }
  PrefProxyConfigTrackerImpl::OnProxyConfigChanged(effective_config_state,
                                                   effective_config);
  if (VLOG_IS_ON(1)) {
    base::Value config_dict = effective_config.value().ToValue();
    VLOG(1) << this << ": Proxy changed: "
            << ProxyPrefs::ConfigStateToDebugString(effective_config_state)
            << ", " << config_dict;
  }
}

}  // namespace ash
