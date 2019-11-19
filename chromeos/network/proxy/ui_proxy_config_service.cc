// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/proxy/ui_proxy_config_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/network/proxy/proxy_config_service_impl.h"
#include "chromeos/network/tether_constants.h"
#include "components/device_event_log/device_event_log.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/proxy_resolution/proxy_config.h"

namespace chromeos {

namespace {

// Writes the proxy config of |network| to |proxy_config|.  Sets |onc_source| to
// the source of this configuration. Returns false if no proxy was configured
// for this network.
bool GetProxyConfig(const PrefService* profile_prefs,
                    const PrefService* local_state_prefs,
                    const NetworkState& network,
                    net::ProxyConfigWithAnnotation* proxy_config,
                    onc::ONCSource* onc_source) {
  std::unique_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(profile_prefs, local_state_prefs,
                                             network, onc_source);
  if (!proxy_dict)
    return false;
  return PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(*proxy_dict,
                                                           proxy_config);
}

std::string EffectiveConfigStateToOncSourceString(
    ProxyPrefs::ConfigState effective_config_state,
    bool is_local_state_config,
    onc::ONCSource onc_source) {
  // If source precedes prefs, the config came from prefs - set by either policy
  // or extensions.
  if (ProxyConfigServiceImpl::PrefPrecedes(effective_config_state)) {
    if (effective_config_state == ProxyPrefs::CONFIG_EXTENSION)
      return ::onc::kAugmentationActiveExtension;
    return is_local_state_config ? ::onc::kAugmentationDevicePolicy
                                 : ::onc::kAugmentationUserPolicy;
  }

  // If network is managed, proxy settings should be marked as policy managed,
  // even if the proxy settings are not set by policy - this reports the
  // default proxy settings as non-user modifiable.
  if (onc_source == onc::ONC_SOURCE_USER_POLICY)
    return ::onc::kAugmentationUserPolicy;
  if (onc_source == onc::ONC_SOURCE_DEVICE_POLICY)
    return ::onc::kAugmentationDevicePolicy;

  return std::string();
}

base::Value CreateEffectiveValue(const std::string& source, base::Value value) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(::onc::kAugmentationEffectiveSetting, base::Value(source));
  // ActiveExtension is a special source type indicating that the Effective
  // value is the Active value and was set by an extension. It does not provide
  // a separate value.
  if (source != ::onc::kAugmentationActiveExtension) {
    dict.SetKey(source, value.Clone());
  }
  dict.SetKey(::onc::kAugmentationActiveSetting, std::move(value));
  dict.SetKey(::onc::kAugmentationUserEditable, base::Value(false));
  return dict;
}

void SetManualProxy(base::Value* manual,
                    const std::string& source,
                    const std::string& key,
                    const net::ProxyList& proxy_list) {
  if (proxy_list.IsEmpty()) {
    manual->SetPath({key, ::onc::proxy::kHost},
                    CreateEffectiveValue(source, base::Value("")));
    manual->SetPath({key, ::onc::proxy::kPort},
                    CreateEffectiveValue(source, base::Value(0)));
    return;
  }

  const net::ProxyServer& proxy = proxy_list.Get();
  manual->SetPath(
      {key, ::onc::proxy::kHost},
      CreateEffectiveValue(source, base::Value(proxy.host_port_pair().host())));
  manual->SetPath(
      {key, ::onc::proxy::kPort},
      CreateEffectiveValue(source, base::Value(proxy.host_port_pair().port())));
}

base::Value OncValueWithMode(const std::string& source,
                             const std::string& mode) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetKey(::onc::network_config::kType,
                CreateEffectiveValue(source, base::Value(mode)));
  return result;
}

base::Value OncValueForManualProxyList(
    const std::string& source,
    const net::ProxyList& for_http,
    const net::ProxyList& for_https,
    const net::ProxyList& for_ftp,
    const net::ProxyList& fallback,
    const net::ProxyBypassRules& bypass_rules) {
  if (for_http.IsEmpty() && for_https.IsEmpty() && for_ftp.IsEmpty() &&
      fallback.IsEmpty()) {
    return base::Value();
  }
  base::Value result = OncValueWithMode(source, ::onc::proxy::kManual);

  base::Value* manual = result.SetKey(
      ::onc::proxy::kManual, base::Value(base::Value::Type::DICTIONARY));
  SetManualProxy(manual, source, ::onc::proxy::kHttp, for_http);
  SetManualProxy(manual, source, ::onc::proxy::kHttps, for_https);
  SetManualProxy(manual, source, ::onc::proxy::kFtp, for_ftp);
  SetManualProxy(manual, source, ::onc::proxy::kSocks, fallback);

  base::Value exclude_domains(base::Value::Type::LIST);
  for (const auto& rule : bypass_rules.rules())
    exclude_domains.Append(rule->ToString());
  result.SetKey(::onc::proxy::kExcludeDomains,
                CreateEffectiveValue(source, std::move(exclude_domains)));

  return result;
}

base::Value OncValueForEmptyProxyRules(const net::ProxyConfig& net_config,
                                       const std::string& source) {
  if (!net_config.HasAutomaticSettings()) {
    return OncValueWithMode(source, ::onc::proxy::kDirect);
  }

  if (net_config.auto_detect()) {
    return OncValueWithMode(source, ::onc::proxy::kWPAD);
  }

  if (net_config.has_pac_url()) {
    base::Value result = OncValueWithMode(source, ::onc::proxy::kPAC);
    result.SetKey(
        ::onc::proxy::kPAC,
        CreateEffectiveValue(source, base::Value(net_config.pac_url().spec())));
    return result;
  }

  return base::Value();
}

base::Value NetProxyConfigAsOncValue(const net::ProxyConfig& net_config,
                                     const std::string& source) {
  switch (net_config.proxy_rules().type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return OncValueForEmptyProxyRules(net_config, source);
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      return OncValueForManualProxyList(source,
                                        net_config.proxy_rules().single_proxies,
                                        net_config.proxy_rules().single_proxies,
                                        net_config.proxy_rules().single_proxies,
                                        net_config.proxy_rules().single_proxies,
                                        net_config.proxy_rules().bypass_rules);
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      return OncValueForManualProxyList(
          source, net_config.proxy_rules().proxies_for_http,
          net_config.proxy_rules().proxies_for_https,
          net_config.proxy_rules().proxies_for_ftp,
          net_config.proxy_rules().fallback_proxies,
          net_config.proxy_rules().bypass_rules);
  }
  return base::Value();
}

}  // namespace

UIProxyConfigService::UIProxyConfigService(PrefService* profile_prefs,
                                           PrefService* local_state_prefs)
    : profile_prefs_(profile_prefs), local_state_prefs_(local_state_prefs) {
  if (profile_prefs_) {
    profile_registrar_.Init(profile_prefs_);
    profile_registrar_.Add(
        ::proxy_config::prefs::kProxy,
        base::Bind(&UIProxyConfigService::OnPreferenceChanged,
                   base::Unretained(this)));
    profile_registrar_.Add(
        ::proxy_config::prefs::kUseSharedProxies,
        base::Bind(&UIProxyConfigService::OnPreferenceChanged,
                   base::Unretained(this)));
  }

  DCHECK(local_state_prefs_);
  local_state_registrar_.Init(local_state_prefs_);
  local_state_registrar_.Add(
      ::proxy_config::prefs::kProxy,
      base::Bind(&UIProxyConfigService::OnPreferenceChanged,
                 base::Unretained(this)));
}

UIProxyConfigService::~UIProxyConfigService() = default;

bool UIProxyConfigService::MergeEnforcedProxyConfig(
    const std::string& network_guid,
    base::Value* proxy_settings) {
  current_ui_network_guid_ = network_guid;
  const NetworkState* network = nullptr;
  DCHECK(!network_guid.empty());
  DCHECK(proxy_settings->is_dict());

  network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          network_guid);
  if (!network) {
    NET_LOG(ERROR) << "No NetworkState for guid: " << network_guid;
    current_ui_network_guid_.clear();
    return false;
  }

  if (!network->IsNonProfileType() && !network->IsInProfile()) {
    NET_LOG(ERROR) << "Network not in profile: " << network_guid;
    current_ui_network_guid_.clear();
    return false;
  }

  // The pref service to read proxy settings that apply to all networks.
  // Settings from the profile overrule local state.
  DCHECK(local_state_prefs_);
  PrefService* top_pref_service =
      profile_prefs_ ? profile_prefs_ : local_state_prefs_;

  // Get prefs proxy config if available.
  net::ProxyConfigWithAnnotation pref_config;
  ProxyPrefs::ConfigState pref_state =
      ProxyConfigServiceImpl::ReadPrefConfig(top_pref_service, &pref_config);

  // Get network proxy config if available.
  net::ProxyConfigWithAnnotation network_config;
  net::ProxyConfigService::ConfigAvailability network_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  if (chromeos::GetProxyConfig(profile_prefs_, local_state_prefs_, *network,
                               &network_config, &onc_source)) {
    // Network is private or shared with user using shared proxies.
    NET_LOG(EVENT) << "UIProxyConfigService for "
                   << (profile_prefs_ ? "user" : "login")
                   << ": using proxy of network: " << network->path();
    network_availability = net::ProxyConfigService::CONFIG_VALID;
  }

  // Determine effective proxy config, either from prefs or network.
  ProxyPrefs::ConfigState effective_config_state;
  net::ProxyConfigWithAnnotation effective_config;
  ProxyConfigServiceImpl::GetEffectiveProxyConfig(
      pref_state, pref_config, network_availability, network_config, false,
      &effective_config_state, &effective_config);

  const std::string source = EffectiveConfigStateToOncSourceString(
      effective_config_state, !profile_prefs_, onc_source);
  if (source.empty())
    return false;

  base::Value enforced_settings =
      NetProxyConfigAsOncValue(effective_config.value(), source);
  if (enforced_settings.is_none())
    return false;

  proxy_settings->MergeDictionary(&enforced_settings);
  return true;
}

bool UIProxyConfigService::HasDefaultNetworkProxyConfigured() {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!network)
    return false;
  return ProxyModeForNetwork(network) == ProxyPrefs::MODE_FIXED_SERVERS;
}

ProxyPrefs::ProxyMode UIProxyConfigService::ProxyModeForNetwork(
    const NetworkState* network) {
  // TODO(919691): Include proxies set by an extension and per-user proxies.
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  std::unique_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(nullptr, local_state_prefs_,
                                             *network, &onc_source);
  ProxyPrefs::ProxyMode mode;
  if (!proxy_dict || !proxy_dict->GetMode(&mode))
    return ProxyPrefs::MODE_DIRECT;
  return mode;
}

void UIProxyConfigService::OnPreferenceChanged(const std::string& pref_name) {
  // TODO(tbarzic): Send network update notifications for all networks that
  //     might be affected by the proxy pref change, not just the last network
  //     whose properties were fetched.
  if (current_ui_network_guid_.empty())
    return;
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          current_ui_network_guid_);
  if (!network)
    return;
  NetworkHandler::Get()
      ->network_state_handler()
      ->SendUpdateNotificationForNetwork(network->path());
}

}  // namespace chromeos
