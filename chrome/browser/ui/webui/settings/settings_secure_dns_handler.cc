// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/default_dns_over_https_config_source.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/country_codes/country_codes.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/net/ash_dns_over_https_config_source.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/profiles/profile.h"
#endif

namespace secure_dns = chrome_browser_net::secure_dns;

namespace settings {

namespace {

base::Value::Dict CreateSecureDnsSettingDict(
    content::BrowserContext* browser_context) {
  // Fetch the current host resolver configuration. It is not sufficient to read
  // the secure DNS prefs directly since the host resolver configuration takes
  // other factors into account such as whether a managed environment or
  // parental controls have been detected.
  SecureDnsConfig config =
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              true /* force_check_parental_controls_for_automatic_mode */);

  base::Value::Dict dict;
  dict.Set("mode", SecureDnsConfig::ModeToString(config.mode()));
  dict.Set("config", config.doh_servers().ToString());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::SecureDnsManager* secure_dns_manager =
      g_browser_process->platform_part()->secure_dns_manager();
  dict.Set("osMode",
           SecureDnsConfig::ModeToString(secure_dns_manager->GetOsDohMode()));
  dict.Set("osConfig", secure_dns_manager->GetOsDohConfig().ToString());
  std::optional<std::string> doh_with_identifiers_servers_for_display =
      secure_dns_manager->GetDohWithIdentifiersDisplayServers();
  dict.Set("dohWithIdentifiersActive",
           doh_with_identifiers_servers_for_display.has_value());
  dict.Set("configForDisplay",
           doh_with_identifiers_servers_for_display.value_or(std::string()));
  dict.Set("dohDomainConfigSet", secure_dns_manager->IsDohDomainConfigSet());
#endif
  dict.Set("managementMode", static_cast<int>(config.management_mode()));
  return dict;
}

}  // namespace

SecureDnsHandler::SecureDnsHandler() = default;
SecureDnsHandler::~SecureDnsHandler() = default;

void SecureDnsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSecureDnsResolverList",
      base::BindRepeating(&SecureDnsHandler::HandleGetSecureDnsResolverList,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getSecureDnsSetting",
      base::BindRepeating(&SecureDnsHandler::HandleGetSecureDnsSetting,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "isValidConfig",
      base::BindRepeating(&SecureDnsHandler::HandleIsValidConfig,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "probeConfig", base::BindRepeating(&SecureDnsHandler::HandleProbeConfig,
                                         base::Unretained(this)));
}

void SecureDnsHandler::OnJavascriptAllowed() {
  // Register for updates to the underlying secure DNS prefs so that the
  // secure DNS setting can be updated to reflect the current host resolver
  // configuration.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  doh_source_ = std::make_unique<ash::AshDnsOverHttpsConfigSource>(
      g_browser_process->platform_part()->secure_dns_manager(),
      g_browser_process->local_state());
#else
  doh_source_ = std::make_unique<DefaultDnsOverHttpsConfigSource>(
      g_browser_process->local_state(), /*set_up_pref_defaults=*/false);
#endif
  doh_source_->SetDohChangeCallback(base::BindRepeating(
      &SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript,
      base::Unretained(this)));
}

void SecureDnsHandler::OnJavascriptDisallowed() {
  doh_source_.reset();
}

base::Value::List SecureDnsHandler::GetSecureDnsResolverList() {
  base::Value::List resolvers;
  for (const net::DohProviderEntry* entry : providers_) {
    net::DnsOverHttpsConfig doh_config({entry->doh_server_config});
    base::Value::Dict dict;
    dict.Set("name", entry->ui_name);
    dict.Set("value", doh_config.ToString());
    dict.Set("policy", entry->privacy_policy);
    resolvers.Append(std::move(dict));
  }

  base::RandomShuffle(resolvers.begin(), resolvers.end());
  return resolvers;
}

void SecureDnsHandler::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  network_context_getter_ = base::BindRepeating(
      [](network::mojom::NetworkContext* network_context) {
        return network_context;
      },
      network_context);
}

network::mojom::NetworkContext* SecureDnsHandler::GetNetworkContext() {
  return web_ui()
      ->GetWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext();
}

void SecureDnsHandler::SetProvidersForTesting(
    net::DohProviderEntry::List providers) {
  providers_ = std::move(providers);
}

void SecureDnsHandler::HandleGetSecureDnsResolverList(
    const base::Value::List& args) {
  AllowJavascript();
  std::string callback_id = args[0].GetString();

  ResolveJavascriptCallback(base::Value(callback_id),
                            GetSecureDnsResolverList());
}

void SecureDnsHandler::HandleGetSecureDnsSetting(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1u, args.size());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id, CreateSecureDnsSettingDict(
                       web_ui()->GetWebContents()->GetBrowserContext()));
}

void SecureDnsHandler::HandleIsValidConfig(const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  const std::string& custom_entry = args[1].GetString();

  bool valid = net::DnsOverHttpsConfig::FromString(custom_entry).has_value();
  secure_dns::UpdateValidationHistogram(valid);
  ResolveJavascriptCallback(callback_id, base::Value(valid));
}

void SecureDnsHandler::HandleProbeConfig(const base::Value::List& args) {
  AllowJavascript();

  if (!probe_callback_id_.empty()) {
    // Cancel the pending probe by destroying the probe runner (which does not
    // call the callback), and report a non-error response to avoid leaking the
    // callback.
    runner_.reset();
    ResolveJavascriptCallback(base::Value(probe_callback_id_),
                              base::Value(true));
  }

  probe_callback_id_ = args[0].GetString();
  const std::string& doh_config = args[1].GetString();
  DCHECK(!runner_);
  std::optional<net::DnsOverHttpsConfig> parsed =
      net::DnsOverHttpsConfig::FromString(doh_config);
  DCHECK(parsed.has_value());  // `doh_config` must be valid.
  runner_ =
      secure_dns::MakeProbeRunner(std::move(*parsed), network_context_getter_);
  runner_->RunProbe(base::BindOnce(&SecureDnsHandler::OnProbeComplete,
                                   base::Unretained(this)));
}

void SecureDnsHandler::OnProbeComplete() {
  bool success =
      runner_->result() == chrome_browser_net::DnsProbeRunner::CORRECT;
  runner_.reset();
  secure_dns::UpdateProbeHistogram(success);
  ResolveJavascriptCallback(base::Value(probe_callback_id_),
                            base::Value(success));
  probe_callback_id_.clear();
}

void SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript() {
  FireWebUIListener("secure-dns-setting-changed",
                    CreateSecureDnsSettingDict(
                        web_ui()->GetWebContents()->GetBrowserContext()));
}

// static
net::DohProviderEntry::List SecureDnsHandler::GetFilteredProviders() {
  // Note: Check whether each provider is enabled *after* filtering based on
  // country code so that if we are doing experimentation via Finch for a
  // regional provider, the experiment groups will be less likely to include
  // users from other regions unnecessarily (since a client will be included in
  // the experiment if the provider feature flag is checked).
  return secure_dns::SelectEnabledProviders(secure_dns::ProvidersForCountry(
      net::DohProviderEntry::GetList(), country_codes::GetCurrentCountryID()));
}

}  // namespace settings
