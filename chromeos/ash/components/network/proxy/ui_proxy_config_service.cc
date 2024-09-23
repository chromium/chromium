// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/proxy_resolution/proxy_config.h"

namespace ash {

namespace {

// Writes the proxy config of |network| to |proxy_config|.  Sets |onc_source| to
// the source of this configuration. Returns false if no proxy was configured
// for this network.
bool GetProxyConfig(const PrefService* profile_prefs,
                    const PrefService* local_state_prefs,
                    const NetworkState& network,
                    const NetworkProfileHandler* network_profile_handler,
                    net::ProxyConfigWithAnnotation* proxy_config,
                    onc::ONCSource* onc_source) {
  std::unique_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(profile_prefs, local_state_prefs,
                                             network, network_profile_handler,
                                             onc_source);
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

template <typename T>
base::Value::Dict CreateEffectiveValue(const std::string& source, T value) {
  base::Value::Dict dict;
  dict.Set(::onc::kAugmentationEffectiveSetting, source);
  // ActiveExtension is a special source type indicating that the Effective
  // value is the Active value and was set by an extension. It does not provide
  // a separate value.
  if (source != ::onc::kAugmentationActiveExtension) {
    dict.Set(source, value.Clone());
  }
  dict.Set(::onc::kAugmentationActiveSetting, std::move(value));
  dict.Set(::onc::kAugmentationUserEditable, false);
  return dict;
}

void SetManualProxy(base::Value::Dict* manual,
                    const std::string& source,
                    const std::string& key,
                    const net::ProxyList& proxy_list) {
  if (proxy_list.IsEmpty()) {
    manual->SetByDottedPath(base::JoinString({key, ::onc::proxy::kHost}, "."),
                            CreateEffectiveValue(source, base::Value("")));
    manual->SetByDottedPath(base::JoinString({key, ::onc::proxy::kPort}, "."),
                            CreateEffectiveValue(source, base::Value(0)));
    return;
  }

  const net::ProxyChain& chain = proxy_list.First();
  CHECK(chain.is_single_proxy());
  const net::ProxyServer& proxy = chain.First();
  manual->SetByDottedPath(
      base::JoinString({key, ::onc::proxy::kHost}, "."),
      CreateEffectiveValue(source, base::Value(proxy.host_port_pair().host())));
  manual->SetByDottedPath(
      base::JoinString({key, ::onc::proxy::kPort}, "."),
      CreateEffectiveValue(source, base::Value(proxy.host_port_pair().port())));
}

base::Value::Dict OncValueWithMode(const std::string& source,
                                   const std::string& mode) {
  return base::Value::Dict().Set(
      ::onc::network_config::kType,
      CreateEffectiveValue(source, base::Value(mode)));
}

std::optional<base::Value::Dict> OncValueForManualProxyList(
    const std::string& source,
    const net::ProxyList& for_http,
    const net::ProxyList& for_https,
    const net::ProxyList& fallback,
    const net::ProxyBypassRules& bypass_rules) {
  if (for_http.IsEmpty() && for_https.IsEmpty() && fallback.IsEmpty()) {
    return std::nullopt;
  }
  base::Value::Dict result = OncValueWithMode(source, ::onc::proxy::kManual);

  base::Value::Dict* manual =
      result.Set(::onc::proxy::kManual, base::Value::Dict())->GetIfDict();
  SetManualProxy(manual, source, ::onc::proxy::kHttp, for_http);
  SetManualProxy(manual, source, ::onc::proxy::kHttps, for_https);
  SetManualProxy(manual, source, ::onc::proxy::kSocks, fallback);

  base::Value::List exclude_domains;
  for (const auto& rule : bypass_rules.rules())
    exclude_domains.Append(rule->ToString());
  result.Set(::onc::proxy::kExcludeDomains,
             CreateEffectiveValue(source, std::move(exclude_domains)));

  return result;
}

std::optional<base::Value::Dict> OncValueForEmptyProxyRules(
    const net::ProxyConfig& net_config,
    const std::string& source) {
  if (!net_config.HasAutomaticSettings()) {
    return OncValueWithMode(source, ::onc::proxy::kDirect);
  }

  if (net_config.auto_detect()) {
    return OncValueWithMode(source, ::onc::proxy::kWPAD);
  }

  if (net_config.has_pac_url()) {
    base::Value::Dict result = OncValueWithMode(source, ::onc::proxy::kPAC);
    result.Set(
        ::onc::proxy::kPAC,
        CreateEffectiveValue(source, base::Value(net_config.pac_url().spec())));
    return result;
  }

  return std::nullopt;
}

std::optional<base::Value::Dict> NetProxyConfigAsOncValue(
    const net::ProxyConfig& net_config,
    const std::string& source) {
  switch (net_config.proxy_rules().type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return OncValueForEmptyProxyRules(net_config, source);
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      return OncValueForManualProxyList(source,
                                        net_config.proxy_rules().single_proxies,
                                        net_config.proxy_rules().single_proxies,
                                        net_config.proxy_rules().single_proxies,
                                        net_config.proxy_rules().bypass_rules);
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      return OncValueForManualProxyList(
          source, net_config.proxy_rules().proxies_for_http,
          net_config.proxy_rules().proxies_for_https,
          net_config.proxy_rules().fallback_proxies,
          net_config.proxy_rules().bypass_rules);
  }
  return std::nullopt;
}

ProxyPrefs::ProxyMode OncStringToProxyMode(const std::string& onc_proxy_type) {
  if (onc_proxy_type == ::onc::proxy::kDirect) {
    return ProxyPrefs::ProxyMode::MODE_DIRECT;
  }
  if (onc_proxy_type == ::onc::proxy::kWPAD) {
    return ProxyPrefs::ProxyMode::MODE_AUTO_DETECT;
  }
  if (onc_proxy_type == ::onc::proxy::kPAC) {
    return ProxyPrefs::ProxyMode::MODE_PAC_SCRIPT;
  }
  if (onc_proxy_type == ::onc::proxy::kManual) {
    return ProxyPrefs::ProxyMode::MODE_FIXED_SERVERS;
  }
  NOTREACHED_IN_MIGRATION() << "Unsupported ONC proxy type: " << onc_proxy_type;
  return ProxyPrefs::ProxyMode::MODE_DIRECT;
}

}  // namespace

UIProxyConfigService::UIProxyConfigService(
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler)
    : profile_prefs_(profile_prefs), local_state_prefs_(local_state_prefs) {
  if (profile_prefs_) {
    profile_registrar_.Init(profile_prefs_);
    profile_registrar_.Add(
        ::proxy_config::prefs::kProxy,
        base::BindRepeating(&UIProxyConfigService::OnPreferenceChanged,
                            base::Unretained(this)));
    profile_registrar_.Add(
        ::proxy_config::prefs::kUseSharedProxies,
        base::BindRepeating(&UIProxyConfigService::OnPreferenceChanged,
                            base::Unretained(this)));
  }

  DCHECK(local_state_prefs_);
  local_state_registrar_.Init(local_state_prefs_);
  local_state_registrar_.Add(
      ::proxy_config::prefs::kProxy,
      base::BindRepeating(&UIProxyConfigService::OnPreferenceChanged,
                          base::Unretained(this)));
  network_state_handler_ = network_state_handler;
  network_profile_handler_ = network_profile_handler;
}

UIProxyConfigService::~UIProxyConfigService() = default;

bool UIProxyConfigService::MergeEnforcedProxyConfig(
    const std::string& network_guid,
    base::Value::Dict* proxy_settings) {
  current_ui_network_guid_ = network_guid;
  const NetworkState* network = nullptr;
  DCHECK(!network_guid.empty());
  DCHECK(proxy_settings);
  DCHECK(network_state_handler_);

  network = network_state_handler_->GetNetworkStateFromGuid(network_guid);
  if (!network) {
    NET_LOG(ERROR) << "No NetworkState for guid: " << network_guid;
    current_ui_network_guid_.clear();
    return false;
  }

  if (!network->IsNonProfileType() && !network->IsInProfile()) {
    NET_LOG(ERROR) << "Network not in profile: " << NetworkId(network);
    current_ui_network_guid_.clear();
    return false;
  }

  // The pref service to read proxy settings that apply to all networks.
  // Settings from the profile overrule local state.
  DCHECK(local_state_prefs_);
  DCHECK(network_profile_handler_);
  PrefService* top_pref_service =
      profile_prefs_ ? profile_prefs_.get() : local_state_prefs_.get();

  // Get prefs proxy config if available.
  net::ProxyConfigWithAnnotation pref_config;
  ProxyPrefs::ConfigState pref_state =
      ProxyConfigServiceImpl::ReadPrefConfig(top_pref_service, &pref_config);

  // Get network proxy config if available.
  net::ProxyConfigWithAnnotation network_config;
  net::ProxyConfigService::ConfigAvailability network_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  if (GetProxyConfig(profile_prefs_, local_state_prefs_, *network,
                     network_profile_handler_, &network_config, &onc_source)) {
    // Network is private or shared with user using shared proxies.
    // Note: This is a common occurrence so we don't spam NET_LOG.
    VLOG(2) << "UIProxyConfigService for "
            << (profile_prefs_ ? "user" : "login")
            << ": using proxy of network: " << NetworkId(network);
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

  std::optional<base::Value::Dict> enforced_settings =
      NetProxyConfigAsOncValue(effective_config.value(), source);
  if (!enforced_settings)
    return false;

  proxy_settings->Merge(std::move(*enforced_settings));
  return true;
}

ProxyPrefs::ProxyMode UIProxyConfigService::ProxyModeForNetwork(
    const NetworkState* network) {
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  std::unique_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(nullptr, local_state_prefs_,
                                             *network, network_profile_handler_,
                                             &onc_source);
  // On the OOBE screen and/or tests.
  if (!network->IsInProfile()) {
    ProxyPrefs::ProxyMode mode;
    if (!proxy_dict || !proxy_dict->GetMode(&mode)) {
      return ProxyPrefs::MODE_DIRECT;
    }
    return mode;
  }

  base::Value::Dict proxy_settings;
  if (proxy_dict) {
    proxy_settings = proxy_dict->GetDictionary().Clone();
  }
  // Check for managed proxy settings.
  MergeEnforcedProxyConfig(network->guid(), &proxy_settings);
  if (!proxy_settings.empty()) {
    base::Value::Dict* proxy_type =
        proxy_settings.FindDict(::onc::network_config::kType);
    if (proxy_type) {
      std::string* proxy_active =
          proxy_type->FindString(::onc::kAugmentationActiveSetting);
      if (proxy_active) {
        return OncStringToProxyMode(*proxy_active);
      }
    }
  }

  if (!proxy_dict) {
    return ProxyPrefs::MODE_DIRECT;
  }

  // Check for user set proxy settings.
  ProxyPrefs::ProxyMode mode;
  if (proxy_dict->GetMode(&mode)) {
    return mode;
  }
  return ProxyPrefs::ProxyMode::MODE_DIRECT;
}

void UIProxyConfigService::OnPreferenceChanged(const std::string& pref_name) {
  DCHECK(network_state_handler_);
  // TODO(tbarzic): Send network update notifications for all networks that
  //     might be affected by the proxy pref change, not just the last network
  //     whose properties were fetched.
  if (current_ui_network_guid_.empty())
    return;
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(current_ui_network_guid_);
  if (network)
    network_state_handler_->SendUpdateNotificationForNetwork(network->path());
}

}  // namespace ash
