// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/net_internals_resources.h"
#include "chrome/grit/net_internals_resources_map.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "url/scheme_host_port.h"

using content::BrowserThread;

namespace {

content::WebUIDataSource* CreateNetInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetInternalsHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kNetInternalsResources, kNetInternalsResourcesSize),
      IDR_NET_INTERNALS_INDEX_HTML);
  webui::EnableTrustedTypesCSP(source);

  return source;
}

void IgnoreBoolCallback(bool result) {}

// This function converts std::vector<net::IPEndPoint> to base::Value::List.
base::Value::List IPEndpointsToBaseList(
    const std::vector<net::IPEndPoint>& resolved_addresses) {
  base::Value::List resolved_addresses_list;
  for (const net::IPEndPoint& resolved_address : resolved_addresses) {
    resolved_addresses_list.Append(resolved_address.ToStringWithoutPort());
  }
  return resolved_addresses_list;
}

// This function converts net::ConnectionEndpointMetadata to base::Value::Dict.
base::Value::Dict ConnectionEndpointMetadataToBaseDict(
    const net::ConnectionEndpointMetadata& metadata) {
  base::Value::Dict connection_endpoint_metadata;

  base::Value::List supported_protocol_alpns;
  base::Value::List ech_config_list;
  for (const std::string& supported_protocol_alpn :
       metadata.supported_protocol_alpns) {
    supported_protocol_alpns.Append(supported_protocol_alpn);
  }

  for (uint8_t ech_config : metadata.ech_config_list) {
    ech_config_list.Append(ech_config);
  }

  connection_endpoint_metadata.Set("supported_protocol_alpns",
                                   std::move(supported_protocol_alpns));
  connection_endpoint_metadata.Set("ech_config_list",
                                   std::move(ech_config_list));
  connection_endpoint_metadata.Set("target_name", metadata.target_name);

  return connection_endpoint_metadata;
}

// This function converts
// absl::optional<net::HostResolverEndpointResults> to
// base::Value::List.
base::Value::List HostResolverEndpointResultsToBaseList(
    const absl::optional<net::HostResolverEndpointResults>& endpoint_results) {
  base::Value::List endpoint_results_list;

  if (!endpoint_results) {
    return endpoint_results_list;
  }

  for (const auto& endpoint_result : *endpoint_results) {
    base::Value::Dict endpoint_results_dict;
    endpoint_results_dict.Set(
        "ip_endpoints", IPEndpointsToBaseList(endpoint_result.ip_endpoints));
    endpoint_results_dict.Set("metadata", ConnectionEndpointMetadataToBaseDict(
                                              endpoint_result.metadata));
    endpoint_results_list.Append(std::move(endpoint_results_dict));
  }
  return endpoint_results_list;
}

using ResolveHostResult = base::expected<base::Value, std::string>;

// This class implements network::mojom::ResolveHostClient.
class NetInternalsResolveHostClient : public network::mojom::ResolveHostClient {
 public:
  using Callback = base::OnceCallback<void(
      const net::ResolveErrorInfo&,
      const absl::optional<net::AddressList>&,
      const absl::optional<net::HostResolverEndpointResults>&,
      NetInternalsResolveHostClient*)>;

  NetInternalsResolveHostClient(
      mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver,
      Callback callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &NetInternalsResolveHostClient::OnComplete, base::Unretained(this),
        net::ERR_FAILED, net::ResolveErrorInfo(net::ERR_FAILED),
        /*resolved_addresses=*/absl::nullopt,
        /*endpoint_results_with_metadata=*/absl::nullopt));
  }
  ~NetInternalsResolveHostClient() override = default;

  NetInternalsResolveHostClient(const NetInternalsResolveHostClient&) = delete;
  NetInternalsResolveHostClient& operator=(
      const NetInternalsResolveHostClient&) = delete;

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(int32_t error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    std::move(callback_).Run(resolve_error_info, resolved_addresses,
                             endpoint_results_with_metadata, this);
  }
  void OnTextResults(const std::vector<std::string>& text_results) override {
    NOTREACHED();
  }
  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override {
    NOTREACHED();
  }

 private:
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_;
  Callback callback_;
};

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class NetInternalsMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit NetInternalsMessageHandler(content::WebUI* web_ui);

  NetInternalsMessageHandler(const NetInternalsMessageHandler&) = delete;
  NetInternalsMessageHandler& operator=(const NetInternalsMessageHandler&) =
      delete;

  ~NetInternalsMessageHandler() override = default;

 protected:
  // WebUIMessageHandler implementation:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  network::mojom::NetworkContext* GetNetworkContext();

  // Resolve JS |callback_id| with |result|.
  // If the renderer is displaying a log file, the message will be ignored.
  void ResolveCallbackWithResult(const std::string& callback_id,
                                 base::Value::Dict result);

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  void OnReloadProxySettings(const base::Value::List& list);
  void OnClearBadProxies(const base::Value::List& list);
  void OnResolveHost(const base::Value::List& list);
  void OnClearHostResolverCache(const base::Value::List& list);
  void OnDomainSecurityPolicyDelete(const base::Value::List& list);
  void OnHSTSQuery(const base::Value::List& list);
  void OnHSTSAdd(const base::Value::List& list);
  void OnCloseIdleSockets(const base::Value::List& list);
  void OnFlushSocketPools(const base::Value::List& list);
  void OnResolveHostDone(
      const std::string& callback_id,
      const net::ResolveErrorInfo&,
      const absl::optional<net::AddressList>&,
      const absl::optional<net::HostResolverEndpointResults>&,
      NetInternalsResolveHostClient* dns_lookup_client);

  raw_ptr<content::WebUI> web_ui_;
  std::set<std::unique_ptr<NetInternalsResolveHostClient>,
           base::UniquePtrComparator>
      dns_lookup_clients_;
  base::WeakPtrFactory<NetInternalsMessageHandler> weak_factory_{this};
};

NetInternalsMessageHandler::NetInternalsMessageHandler(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

void NetInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "reloadProxySettings",
      base::BindRepeating(&NetInternalsMessageHandler::OnReloadProxySettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearBadProxies",
      base::BindRepeating(&NetInternalsMessageHandler::OnClearBadProxies,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resolveHost",
      base::BindRepeating(&NetInternalsMessageHandler::OnResolveHost,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearHostResolverCache",
      base::BindRepeating(&NetInternalsMessageHandler::OnClearHostResolverCache,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "domainSecurityPolicyDelete",
      base::BindRepeating(
          &NetInternalsMessageHandler::OnDomainSecurityPolicyDelete,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "hstsQuery", base::BindRepeating(&NetInternalsMessageHandler::OnHSTSQuery,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "hstsAdd", base::BindRepeating(&NetInternalsMessageHandler::OnHSTSAdd,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "closeIdleSockets",
      base::BindRepeating(&NetInternalsMessageHandler::OnCloseIdleSockets,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "flushSocketPools",
      base::BindRepeating(&NetInternalsMessageHandler::OnFlushSocketPools,
                          base::Unretained(this)));
}

void NetInternalsMessageHandler::OnJavascriptDisallowed() {
  weak_factory_.InvalidateWeakPtrs();
}

void NetInternalsMessageHandler::OnReloadProxySettings(
    const base::Value::List& list) {
  GetNetworkContext()->ForceReloadProxyConfig(base::NullCallback());
}

void NetInternalsMessageHandler::OnClearBadProxies(
    const base::Value::List& list) {
  GetNetworkContext()->ClearBadProxiesCache(base::NullCallback());
}

void NetInternalsMessageHandler::OnResolveHost(const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  const std::string* hostname = list[1].GetIfString();
  DCHECK(callback_id);
  DCHECK(hostname);

  // Intentionally using https scheme to trigger a HTTPS DNS resource record
  // query.
  auto scheme_host_port = url::SchemeHostPort("https", *hostname, 443);
  const url::Origin origin = url::Origin::Create(GURL("https://" + *hostname));
  AllowJavascript();

  // When ResolveHost() in network process completes, OnResolveHostDone() method
  // is called.
  mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver;
  GetNetworkContext()->ResolveHost(
      network::mojom::HostResolverHost::NewSchemeHostPort(scheme_host_port),
      net::NetworkAnonymizationKey(),
      /*optional_parameters=*/nullptr, receiver.InitWithNewPipeAndPassRemote());

  auto callback = base::BindOnce(&NetInternalsMessageHandler::OnResolveHostDone,
                                 weak_factory_.GetWeakPtr(), *callback_id);
  auto dns_lookup_client = std::make_unique<NetInternalsResolveHostClient>(
      std::move(receiver), std::move(callback));
  dns_lookup_clients_.insert(std::move(dns_lookup_client));
}

void NetInternalsMessageHandler::OnClearHostResolverCache(
    const base::Value::List& list) {
  GetNetworkContext()->ClearHostCache(/*filter=*/nullptr, base::NullCallback());
}

void NetInternalsMessageHandler::OnDomainSecurityPolicyDelete(
    const base::Value::List& list) {
  // |list| should be: [<domain to query>].
  const std::string* domain = list[0].GetIfString();
  DCHECK(domain);
  if (!base::IsStringASCII(*domain)) {
    // There cannot be a unicode entry in the HSTS set.
    return;
  }
  GetNetworkContext()->DeleteDynamicDataForHost(
      *domain, base::BindOnce(&IgnoreBoolCallback));
}

void NetInternalsMessageHandler::OnHSTSQuery(const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  const std::string* domain = list[1].GetIfString();
  DCHECK(callback_id && domain);

  AllowJavascript();
  GetNetworkContext()->GetHSTSState(
      *domain,
      base::BindOnce(&NetInternalsMessageHandler::ResolveCallbackWithResult,
                     weak_factory_.GetWeakPtr(), *callback_id));
}

void NetInternalsMessageHandler::ResolveCallbackWithResult(
    const std::string& callback_id,
    base::Value::Dict result) {
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void NetInternalsMessageHandler::OnHSTSAdd(const base::Value::List& list) {
  DCHECK_GE(2u, list.size());

  // |list| should be: [<domain to query>, <STS include subdomains>]
  const std::string* domain = list[0].GetIfString();
  DCHECK(domain);
  if (!base::IsStringASCII(*domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }
  const bool sts_include_subdomains = list[1].GetBool();

  base::Time expiry = base::Time::Now() + base::Days(1000);
  GetNetworkContext()->AddHSTS(*domain, expiry, sts_include_subdomains,
                               base::DoNothing());
}

void NetInternalsMessageHandler::OnFlushSocketPools(
    const base::Value::List& list) {
  GetNetworkContext()->CloseAllConnections(base::NullCallback());
}

void NetInternalsMessageHandler::OnCloseIdleSockets(
    const base::Value::List& list) {
  GetNetworkContext()->CloseIdleConnections(base::NullCallback());
}

void NetInternalsMessageHandler::OnResolveHostDone(
    const std::string& callback_id,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata,
    NetInternalsResolveHostClient* dns_lookup_client) {
  DCHECK_EQ(dns_lookup_clients_.count(dns_lookup_client), 1u);
  auto it = dns_lookup_clients_.find(dns_lookup_client);
  dns_lookup_clients_.erase(it);

  if (!resolved_addresses) {
    RejectJavascriptCallback(
        base::Value(callback_id),
        base::Value(net::ErrorToString(resolve_error_info.error)));
    return;
  }

  base::Value::Dict result;

  base::Value::List resolved_addresses_list =
      IPEndpointsToBaseList(resolved_addresses->endpoints());
  result.Set("resolved_addresses", std::move(resolved_addresses_list));

  base::Value::List endpoint_result_with_metadata =
      HostResolverEndpointResultsToBaseList(endpoint_results_with_metadata);
  result.Set("endpoint_results_with_metadata",
             std::move(endpoint_result_with_metadata));

  ResolveJavascriptCallback(base::Value(callback_id), std::move(result));
}

// g_network_context_for_testing is used only for testing.
network::mojom::NetworkContext* g_network_context_for_testing = nullptr;

network::mojom::NetworkContext*
NetInternalsMessageHandler::GetNetworkContext() {
  if (g_network_context_for_testing) {
    return g_network_context_for_testing;
  }
  return web_ui_->GetWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext();
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsUI::NetInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<NetInternalsMessageHandler>(web_ui));

  // Set up the chrome://net-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetInternalsHTMLSource());
}

// static
void NetInternalsUI::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context_for_testing) {
  g_network_context_for_testing = network_context_for_testing;
}
