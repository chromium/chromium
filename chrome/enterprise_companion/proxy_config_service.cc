// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/proxy_config_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "services/network/public/cpp/mutable_network_traffic_annotation_tag_mojom_traits.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"
#include "base/win/windows_types.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#define UPDATER_POLICIES_KEY \
  L"Software\\Policies\\" COMPANY_SHORTNAME_STRING L"\\Update\\"
#endif

namespace enterprise_companion {

namespace {

constexpr char kGoogleUpdatePolicyType[] = "google/machine-level-omaha";

constexpr net::NetworkTrafficAnnotationTag kPolicyProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("policy_proxy_config", R"(
      semantics {
        sender: "Proxy Config"
        description:
          "Establishing a connection through a proxy server using "
          "administrator-provided settings."
        trigger:
          "Whenever a network request is made."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other:
          "The proxy server specified in the configuration."
        internal {
          contacts {
            email: "noahrose@google.com"
          }
          contacts {
            email: "chrome-updates-dev@chromium.org"
          }
        }
        last_reviewed: "2024-09-24"
        user_data {
          type: ARBITRARY_DATA
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "Proxy settings can be configured by a combination of Group Policy on"
          " Windows and Chrome Browser Enterprise Cloud Management policies."
        policy_exception_justification:
          "Proxy settings are controlled by OmahaSettingsClientProto, which is "
          "external to ChromeDeviceSettingsProto."
      })");

std::optional<net::ProxyConfig> GetProxyConfigFromPolicyValues(
    const std::string& proxy_mode,
    const std::string& pac_url,
    const std::string& proxy_server) {
  if (proxy_mode == "direct") {
    return net::ProxyConfig::CreateDirect();
  } else if (proxy_mode == "auto_detect") {
    return net::ProxyConfig::CreateAutoDetect();
  } else if (proxy_mode == "pac_script") {
    return net::ProxyConfig::CreateFromCustomPacURL(GURL(pac_url));
  } else if (proxy_mode == "fixed_servers") {
    net::ProxyConfig config;
    config.proxy_rules().type = net::ProxyConfig::ProxyRules::Type::PROXY_LIST;
    config.proxy_rules().single_proxies.SetSingleProxyServer(
        net::ProxyUriToProxyServer(
            proxy_server,
            /*default_scheme=*/net::ProxyServer::SCHEME_HTTP));
    return config;
  } else if (proxy_mode == "system") {
    // Return a nullopt so that the system configuration can be fallen back to.
    return std::nullopt;
  }

  VLOG(1) << "Unknown proxy mode: " << proxy_mode;
  return std::nullopt;
}

// Determine the proxy configuration from Omaha's cloud policies.
std::optional<ProxyConfigAndOverridePrecedence> GetProxyConfigFromCloudPolicy(
    scoped_refptr<device_management_storage::DMStorage> dm_storage) {
  std::optional<enterprise_management::PolicyData> policy_data =
      dm_storage->ReadPolicyData(kGoogleUpdatePolicyType);
  if (!policy_data) {
    return std::nullopt;
  }
  wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;
  if (!omaha_settings.ParseFromString(policy_data->policy_value())) {
    VLOG(1) << "Failed to parse OmahaSettingsClientProto";
    return std::nullopt;
  }

  if (!omaha_settings.has_proxy_mode()) {
    VLOG(1) << "OmahaSettingsClientProto does not contain a ProxyMode.";
    return std::nullopt;
  }

  std::optional<net::ProxyConfig> config = GetProxyConfigFromPolicyValues(
      omaha_settings.proxy_mode(), omaha_settings.proxy_pac_url(),
      omaha_settings.proxy_server());
  if (!config) {
    return std::nullopt;
  }

  ProxyConfigAndOverridePrecedence result;
  result.config = *config;
  if (omaha_settings.has_cloud_policy_overrides_platform_policy()) {
    result.cloud_policy_overrides_platform_policy =
        omaha_settings.cloud_policy_overrides_platform_policy();
  }
  return result;
}

std::optional<ProxyConfigAndOverridePrecedence>
GetProxyConfigFromSystemPolicy() {
#if BUILDFLAG(IS_WIN)
  std::string proxy_mode;
  std::string pac_url;
  std::string proxy_server;
  std::optional<bool> cloud_policy_overrides_platform_policy;
  for (base::win::RegistryValueIterator it(HKEY_LOCAL_MACHINE,
                                           UPDATER_POLICIES_KEY);
       it.Valid(); ++it) {
    const std::string key_name =
        base::ToLowerASCII(base::SysWideToUTF8(it.Name()));
    if (it.Type() == REG_DWORD &&
        key_name == "cloudpolicyoverridesplatformpolicy") {
      cloud_policy_overrides_platform_policy =
          *reinterpret_cast<const int*>(it.Value());
      continue;
    } else if (it.Type() != REG_SZ) {
      continue;
    }

    const std::string value = base::SysWideToUTF8(it.Value());

    if (key_name == "proxymode") {
      proxy_mode = value;
    } else if (key_name == "proxypacurl") {
      pac_url = value;
    } else if (key_name == "proxyserver") {
      proxy_server = value;
    }
  }

  std::optional<net::ProxyConfig> config =
      GetProxyConfigFromPolicyValues(proxy_mode, pac_url, proxy_server);
  return config ? std::make_optional(ProxyConfigAndOverridePrecedence{
                      .config = *config,
                      .cloud_policy_overrides_platform_policy =
                          cloud_policy_overrides_platform_policy})
                : std::nullopt;

#else
  // Proxy configuration is not supported via system policy on Mac or Linux.
  return std::nullopt;
#endif
}

std::optional<net::ProxyConfig> GetProxyConfigFromPolicy(
    scoped_refptr<device_management_storage::DMStorage> dm_storage,
    std::optional<ProxyConfigAndOverridePrecedence> system_policy_config) {
  std::optional<ProxyConfigAndOverridePrecedence> cloud_policy_config =
      GetProxyConfigFromCloudPolicy(dm_storage);

  if (!cloud_policy_config && !system_policy_config) {
    return std::nullopt;
  } else if (cloud_policy_config.has_value() !=
             system_policy_config.has_value()) {
    return cloud_policy_config ? cloud_policy_config->config
                               : system_policy_config->config;
  }

  bool cloud_policy_overrides_platform_policy = false;
  if (cloud_policy_config &&
      cloud_policy_config->cloud_policy_overrides_platform_policy) {
    cloud_policy_overrides_platform_policy =
        *cloud_policy_config->cloud_policy_overrides_platform_policy;
  } else if (system_policy_config &&
             system_policy_config->cloud_policy_overrides_platform_policy) {
    cloud_policy_overrides_platform_policy =
        *system_policy_config->cloud_policy_overrides_platform_policy;
  }

  return cloud_policy_overrides_platform_policy ? cloud_policy_config->config
                                                : system_policy_config->config;
}

class ProxyConfigService final : public net::ProxyConfigService,
                                 net::ProxyConfigService::Observer {
 public:
  ProxyConfigService(
      scoped_refptr<device_management_storage::DMStorage> dm_storage,
      base::RepeatingCallback<std::optional<ProxyConfigAndOverridePrecedence>()>
          system_policy_proxy_config_provider,
      std::unique_ptr<net::ProxyConfigService> fallback)
      : dm_storage_(dm_storage),
        system_policy_proxy_config_provider_(
            system_policy_proxy_config_provider),
        fallback_(std::move(fallback)) {
    fallback_->AddObserver(this);
  }

  ~ProxyConfigService() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    fallback_->RemoveObserver(this);
  }

  // Overrides for net::ProxyConfigService.
  void AddObserver(Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_list_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observer_list_.RemoveObserver(observer);
  }

  bool UsesPolling() override { return true; }

  void OnLazyPoll() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    fallback_->OnLazyPoll();
    std::optional<net::ProxyConfig> new_config = GetEffectiveConfig();

    if ((last_config_.has_value() != new_config.has_value()) ||
        (last_config_ && new_config &&
         !last_config_.value().Equals(new_config.value()))) {
      last_config_ = new_config;
      observer_list_.Notify(
          &Observer::OnProxyConfigChanged,
          net::ProxyConfigWithAnnotation(*last_config_,
                                         kPolicyProxyConfigTrafficAnnotation),
          new_config ? ConfigAvailability::CONFIG_VALID
                     : ConfigAvailability::CONFIG_PENDING);
    }
  }

  ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    OnLazyPoll();
    if (last_config_) {
      *config = net::ProxyConfigWithAnnotation(
          *last_config_, kPolicyProxyConfigTrafficAnnotation);
      return ConfigAvailability::CONFIG_VALID;
    }
    return ConfigAvailability::CONFIG_PENDING;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<device_management_storage::DMStorage> dm_storage_;
  const base::RepeatingCallback<
      std::optional<ProxyConfigAndOverridePrecedence>()>
      system_policy_proxy_config_provider_;
  const std::unique_ptr<net::ProxyConfigService> fallback_;
  base::ObserverList<Observer>::Unchecked observer_list_;
  std::optional<net::ProxyConfig> last_config_;

  // Overrides for Observer:
  void OnProxyConfigChanged(const net::ProxyConfigWithAnnotation&,
                            ConfigAvailability) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    net::ProxyConfigWithAnnotation config;
    ConfigAvailability availability = GetLatestProxyConfig(&config);
    observer_list_.Notify(&Observer::OnProxyConfigChanged, config,
                          availability);
  }

  std::optional<net::ProxyConfig> GetEffectiveConfig() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::optional<net::ProxyConfig> policy_config = GetProxyConfigFromPolicy(
        dm_storage_, system_policy_proxy_config_provider_.Run());
    if (policy_config) {
      VLOG(1) << "Determined proxy configuration from policies: "
              << policy_config->ToValue();
      return policy_config;
    }

    net::ProxyConfigWithAnnotation fallback_config;
    ConfigAvailability fallback_availability =
        fallback_->GetLatestProxyConfig(&fallback_config);
    if (fallback_availability == ConfigAvailability::CONFIG_VALID) {
      VLOG(1) << "Determined proxy configuration from fallback: "
              << fallback_config.value().ToValue();
      return fallback_config.value();
    }
    VLOG(1) << "No proxy configuration from policies or the fallback is "
               "available.";
    return std::nullopt;
  }
};

}  // namespace

std::unique_ptr<net::ProxyConfigService> CreatePolicyProxyConfigService(
    scoped_refptr<device_management_storage::DMStorage> dm_storage,
    base::RepeatingCallback<std::optional<ProxyConfigAndOverridePrecedence>()>
        system_policy_proxy_config_provider,
    std::unique_ptr<net::ProxyConfigService> fallback) {
  return std::make_unique<ProxyConfigService>(
      dm_storage, system_policy_proxy_config_provider, std::move(fallback));
}

base::RepeatingCallback<std::optional<ProxyConfigAndOverridePrecedence>()>
GetDefaultSystemPolicyProxyConfigProvider() {
  return base::BindRepeating(&GetProxyConfigFromSystemPolicy);
}

}  // namespace enterprise_companion
