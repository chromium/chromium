// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/net/dns_over_https_config_source.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "net/dns/public/doh_provider_entry.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace settings {

// Handler for the Secure DNS setting.
class SecureDnsHandler : public SettingsPageUIHandler {
 public:
  SecureDnsHandler();

  SecureDnsHandler(const SecureDnsHandler&) = delete;
  SecureDnsHandler& operator=(const SecureDnsHandler&) = delete;

  ~SecureDnsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Get the list of dropdown resolver options. Each option is represented
  // as a dictionary with the following keys: "name" (the text to display in the
  // UI), "value" (the DoH template for this provider), and "policy" (the URL of
  // the provider's privacy policy).
  base::Value::List GetSecureDnsResolverList();

  void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context);

  // DohProviderEntry cannot be copied, so the entries referenced in |providers|
  // must be long-lived.
  void SetProvidersForTesting(net::DohProviderEntry::List providers);

 protected:
  // Retrieves all pre-approved secure resolvers and returns them to WebUI.
  void HandleGetSecureDnsResolverList(const base::Value::List& args);

  // Intended to be called once upon creation of the secure DNS setting.
  void HandleGetSecureDnsSetting(const base::Value::List& args);

  // Parses a custom entry and returns true if it is a fully valid config.
  void HandleIsValidConfig(const base::Value::List& args);

  // Returns whether or not a test query succeeds with the provided config.
  void HandleProbeConfig(const base::Value::List& args);

  // Retrieves the current host resolver configuration, computes the
  // corresponding UI representation, and sends it to javascript.
  void SendSecureDnsSettingUpdatesToJavascript();

 private:
  network::mojom::NetworkContext* GetNetworkContext();
  void OnProbeComplete();

  static net::DohProviderEntry::List GetFilteredProviders();

  net::DohProviderEntry::List providers_ = GetFilteredProviders();
  std::unique_ptr<chrome_browser_net::DnsProbeRunner> runner_;
  network::NetworkContextGetter network_context_getter_ =
      base::BindRepeating(&SecureDnsHandler::GetNetworkContext,
                          base::Unretained(this));
  // ID of the Javascript callback for the current pending probe, or "" if
  // there is no probe currently in progress.
  std::string probe_callback_id_;

  std::unique_ptr<DnsOverHttpsConfigSource> doh_source_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURE_DNS_HANDLER_H_
