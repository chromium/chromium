// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "chrome/browser/browser_process.h"
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
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/util.h"
#include "ui/base/l10n/l10n_util.h"

namespace secure_dns = chrome_browser_net::secure_dns;

namespace settings {

namespace {

std::unique_ptr<base::DictionaryValue> CreateSecureDnsSettingDict() {
  // Fetch the current host resolver configuration. It is not sufficient to read
  // the secure DNS prefs directly since the host resolver configuration takes
  // other factors into account such as whether a managed environment or
  // parental controls have been detected.
  SecureDnsConfig config =
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              true /* force_check_parental_controls_for_automatic_mode */);

  auto secure_dns_templates = std::make_unique<base::ListValue>();
  for (const auto& doh_server : config.servers()) {
    secure_dns_templates->Append(doh_server.server_template);
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString("mode", SecureDnsConfig::ModeToString(config.mode()));
  dict->SetList("templates", std::move(secure_dns_templates));
  dict->SetInteger("managementMode",
                   static_cast<int>(config.management_mode()));
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
      "parseCustomDnsEntry",
      base::BindRepeating(&SecureDnsHandler::HandleParseCustomDnsEntry,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "probeCustomDnsTemplate",
      base::BindRepeating(&SecureDnsHandler::HandleProbeCustomDnsTemplate,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "recordUserDropdownInteraction",
      base::BindRepeating(
          &SecureDnsHandler::HandleRecordUserDropdownInteraction,
          base::Unretained(this)));
}

void SecureDnsHandler::OnJavascriptAllowed() {
  // Register for updates to the underlying secure DNS prefs so that the
  // secure DNS setting can be updated to reflect the current host resolver
  // configuration.
  pref_registrar_.Init(g_browser_process->local_state());
  pref_registrar_.Add(
      prefs::kDnsOverHttpsMode,
      base::BindRepeating(
          &SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript,
          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kDnsOverHttpsTemplates,
      base::BindRepeating(
          &SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript,
          base::Unretained(this)));
}

void SecureDnsHandler::OnJavascriptDisallowed() {
  pref_registrar_.RemoveAll();
}

base::Value SecureDnsHandler::GetSecureDnsResolverList() {
  base::Value resolvers(base::Value::Type::LIST);
  for (const auto* entry : providers_) {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetStringKey("name", entry->ui_name);
    dict.SetStringKey("value", entry->dns_over_https_template);
    dict.SetStringKey("policy", entry->privacy_policy);
    resolvers.Append(std::move(dict));
  }

  // Randomize the order of the resolvers.
  base::RandomShuffle(resolvers.GetList().begin(), resolvers.GetList().end());

  // Add a custom option to the front of the list
  base::Value custom(base::Value::Type::DICTIONARY);
  custom.SetStringKey("name", l10n_util::GetStringUTF8(IDS_SETTINGS_CUSTOM));
  custom.SetStringKey("value", std::string());  // Empty value means custom.
  custom.SetStringKey("policy", std::string());
  resolvers.Insert(resolvers.GetList().begin(), std::move(custom));

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
  return content::BrowserContext::GetDefaultStoragePartition(
             web_ui()->GetWebContents()->GetBrowserContext())
      ->GetNetworkContext();
}

void SecureDnsHandler::SetProvidersForTesting(
    net::DohProviderEntry::List providers) {
  providers_ = std::move(providers);
}

void SecureDnsHandler::HandleGetSecureDnsResolverList(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id = args->GetList()[0].GetString();

  ResolveJavascriptCallback(base::Value(callback_id),
                            GetSecureDnsResolverList());
}

void SecureDnsHandler::HandleGetSecureDnsSetting(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1u, args->GetList().size());
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, *CreateSecureDnsSettingDict());
}

void SecureDnsHandler::HandleParseCustomDnsEntry(const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  std::string custom_entry;
  CHECK(args->Get(0, &callback_id));
  CHECK(args->GetString(1, &custom_entry));

  // Return all templates in the entry, or none if they are not all valid.
  base::Value templates(base::Value::Type::LIST);
  if (secure_dns::IsValidGroup(custom_entry)) {
    for (base::StringPiece t : secure_dns::SplitGroup(custom_entry)) {
      templates.Append(t);
    }
  }
  secure_dns::UpdateValidationHistogram(!templates.GetList().empty());
  ResolveJavascriptCallback(*callback_id, templates);
}

void SecureDnsHandler::HandleProbeCustomDnsTemplate(
    const base::ListValue* args) {
  AllowJavascript();

  if (!probe_callback_id_.empty()) {
    // Cancel the pending probe by destroying the probe runner (which does not
    // call the callback), and report a non-error response to avoid leaking the
    // callback.
    runner_.reset();
    ResolveJavascriptCallback(base::Value(probe_callback_id_),
                              base::Value(true));
  }

  std::string server_template;
  CHECK(args->GetString(0, &probe_callback_id_));
  CHECK(args->GetString(1, &server_template));

  net::DnsConfigOverrides overrides;
  overrides.search = std::vector<std::string>();
  overrides.attempts = 1;
  overrides.secure_dns_mode = net::SecureDnsMode::kSecure;
  secure_dns::ApplyTemplate(&overrides, server_template);
  DCHECK(!runner_);
  runner_ = std::make_unique<chrome_browser_net::DnsProbeRunner>(
      overrides, network_context_getter_);
  runner_->RunProbe(base::BindOnce(&SecureDnsHandler::OnProbeComplete,
                                   base::Unretained(this)));
}

void SecureDnsHandler::HandleRecordUserDropdownInteraction(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string old_provider;
  std::string new_provider;
  CHECK(args->GetString(0, &old_provider));
  CHECK(args->GetString(1, &new_provider));

  secure_dns::UpdateDropdownHistograms(providers_, old_provider, new_provider);
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
                    *CreateSecureDnsSettingDict());
}

// static
net::DohProviderEntry::List SecureDnsHandler::GetFilteredProviders() {
  const auto local_providers = secure_dns::ProvidersForCountry(
      net::DohProviderEntry::GetList(), country_codes::GetCurrentCountryID());
  return secure_dns::RemoveDisabledProviders(
      local_providers, secure_dns::GetDisabledProviders());
}

}  // namespace settings
