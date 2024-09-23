// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/to_value_list.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "net/base/schemeful_site.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/resources/grit/webui_resources.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

using content::BrowserThread;

namespace {

void CreateAndAddNetInternalsHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUINetInternalsHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kNetInternalsResources, kNetInternalsResourcesSize),
      IDR_NET_INTERNALS_INDEX_HTML);
  webui::EnableTrustedTypesCSP(source);
}

void IgnoreBoolCallback(bool result) {}

// This function converts std::vector<net::IPEndPoint> to base::Value::List.
base::Value::List IPEndpointsToBaseList(
    const std::vector<net::IPEndPoint>& resolved_addresses) {
  return base::ToValueList(resolved_addresses,
                           &net::IPEndPoint::ToStringWithoutPort);
}

// This function converts std::optional<net::HostResolverEndpointResults> to
// base::Value::List.
base::Value::List HostResolverEndpointResultsToBaseList(
    const std::optional<net::HostResolverEndpointResults>& endpoint_results) {
  base::Value::List endpoint_results_list;

  if (!endpoint_results) {
    return endpoint_results_list;
  }

  for (const auto& endpoint : *endpoint_results) {
    base::Value::Dict dict;
    dict.Set("ip_endpoints", IPEndpointsToBaseList(endpoint.ip_endpoints));

    base::Value::List alpns;
    for (const std::string& alpn : endpoint.metadata.supported_protocol_alpns) {
      alpns.Append(alpn);
    }
    dict.Set("alpns", std::move(alpns));

    if (!endpoint.metadata.ech_config_list.empty()) {
      dict.Set("ech_config_list",
               base::Base64Encode(endpoint.metadata.ech_config_list));
    }

    endpoint_results_list.Append(std::move(dict));
  }
  return endpoint_results_list;
}

base::Value::List GetMatchDestList(
    const std::vector<::network::mojom::RequestDestination>& match_dest) {
  base::Value::List result =
      base::Value::List::with_capacity(match_dest.size());
  for (const auto& item : match_dest) {
    result.Append(network::RequestDestinationToString(
        item, network::EmptyRequestDestinationOption::kUseTheEmptyString));
  }
  return result;
}

// This class implements network::mojom::ResolveHostClient.
class NetInternalsResolveHostClient : public network::mojom::ResolveHostClient {
 public:
  using Callback = base::OnceCallback<void(
      const net::ResolveErrorInfo&,
      const std::optional<net::AddressList>&,
      const std::optional<net::HostResolverEndpointResults>&,
      NetInternalsResolveHostClient*)>;

  NetInternalsResolveHostClient(
      mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver,
      Callback callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &NetInternalsResolveHostClient::OnComplete, base::Unretained(this),
        net::ERR_FAILED, net::ResolveErrorInfo(net::ERR_FAILED),
        /*resolved_addresses=*/std::nullopt,
        /*endpoint_results_with_metadata=*/std::nullopt));
  }
  ~NetInternalsResolveHostClient() override = default;

  NetInternalsResolveHostClient(const NetInternalsResolveHostClient&) = delete;
  NetInternalsResolveHostClient& operator=(
      const NetInternalsResolveHostClient&) = delete;

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(int32_t error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    std::move(callback_).Run(resolve_error_info, resolved_addresses,
                             endpoint_results_with_metadata, this);
  }
  void OnTextResults(const std::vector<std::string>& text_results) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override {
    NOTREACHED_IN_MIGRATION();
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
  void OnResolveHostDone(const std::string& callback_id,
                         const net::ResolveErrorInfo&,
                         const std::optional<net::AddressList>&,
                         const std::optional<net::HostResolverEndpointResults>&,
                         NetInternalsResolveHostClient* dns_lookup_client);
  void OnClearSharedDictionary(const base::Value::List& list);
  void OnClearSharedDictionaryCacheForIsolationKey(
      const base::Value::List& list);
  void OnGetSharedDictionaryUsageInfo(const base::Value::List& list);
  void OnGetSharedDictionaryInfo(const base::Value::List& list);

  void OnClearSharedDictionaryDone(const std::string& callback_id);
  void OnClearSharedDictionaryForIsolationKeyDone(
      const std::string& callback_id);
  void OnGetSharedDictionaryUsageInfoDone(
      const std::string& callback_id,
      const std::vector<net::SharedDictionaryUsageInfo>& usage_info);
  void OnGetSharedDictionaryInfoDone(
      const std::string& callback_id,
      std::vector<network::mojom::SharedDictionaryInfoPtr> usage_info);

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
  web_ui()->RegisterMessageCallback(
      "clearSharedDictionary",
      base::BindRepeating(&NetInternalsMessageHandler::OnClearSharedDictionary,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearSharedDictionaryCacheForIsolationKey",
      base::BindRepeating(&NetInternalsMessageHandler::
                              OnClearSharedDictionaryCacheForIsolationKey,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getSharedDictionaryUsageInfo",
      base::BindRepeating(
          &NetInternalsMessageHandler::OnGetSharedDictionaryUsageInfo,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSharedDictionaryInfo",
      base::BindRepeating(
          &NetInternalsMessageHandler::OnGetSharedDictionaryInfo,
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

void NetInternalsMessageHandler::OnClearSharedDictionary(
    const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  DCHECK(callback_id);

  GetNetworkContext()->ClearSharedDictionaryCache(
      base::Time::Min(), base::Time::Max(), /*filter=*/nullptr,
      base::BindOnce(&NetInternalsMessageHandler::OnClearSharedDictionaryDone,
                     weak_factory_.GetWeakPtr(), *callback_id));
}

void NetInternalsMessageHandler::OnClearSharedDictionaryCacheForIsolationKey(
    const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  const std::string* frame_origin = list[1].GetIfString();
  const std::string* top_frame_site = list[2].GetIfString();
  DCHECK(callback_id);
  DCHECK(frame_origin);
  DCHECK(top_frame_site);

  GetNetworkContext()->ClearSharedDictionaryCacheForIsolationKey(
      net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL(*frame_origin)),
          net::SchemefulSite(GURL(*top_frame_site))),
      base::BindOnce(&NetInternalsMessageHandler::
                         OnClearSharedDictionaryForIsolationKeyDone,
                     weak_factory_.GetWeakPtr(), *callback_id));
}

void NetInternalsMessageHandler::OnGetSharedDictionaryUsageInfo(
    const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  DCHECK(callback_id);
  GetNetworkContext()->GetSharedDictionaryUsageInfo(base::BindOnce(
      &NetInternalsMessageHandler::OnGetSharedDictionaryUsageInfoDone,
      weak_factory_.GetWeakPtr(), *callback_id));
}

void NetInternalsMessageHandler::OnGetSharedDictionaryInfo(
    const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  const std::string* frame_origin = list[1].GetIfString();
  const std::string* top_frame_site = list[2].GetIfString();
  DCHECK(callback_id);
  DCHECK(frame_origin);
  DCHECK(top_frame_site);
  GetNetworkContext()->GetSharedDictionaryInfo(
      net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL(*frame_origin)),
          net::SchemefulSite(GURL(*top_frame_site))),
      base::BindOnce(&NetInternalsMessageHandler::OnGetSharedDictionaryInfoDone,
                     weak_factory_.GetWeakPtr(), *callback_id));
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
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&
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

  // TODO(crbug.com/40256843): Rename `endpoint_results_with_metadata` in the
  // Mojo API to `alternative_endpoints`, to match the terminology used in the
  // specification.
  base::Value::List alternative_endpoints_list =
      HostResolverEndpointResultsToBaseList(endpoint_results_with_metadata);
  result.Set("alternative_endpoints", std::move(alternative_endpoints_list));

  ResolveJavascriptCallback(base::Value(callback_id), std::move(result));
}

void NetInternalsMessageHandler::OnGetSharedDictionaryUsageInfoDone(
    const std::string& callback_id,
    const std::vector<net::SharedDictionaryUsageInfo>& usage_info) {
  base::Value::List result_list;
  for (const auto& usage : usage_info) {
    base::Value::Dict dict;
    dict.Set("frame_origin", usage.isolation_key.frame_origin().Serialize());
    dict.Set("top_frame_site",
             usage.isolation_key.top_frame_site().Serialize());
    dict.Set("total_size_bytes",
             base::Value(base::NumberToString(usage.total_size_bytes)));
    result_list.Append(std::move(dict));
  }
  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), std::move(result_list));
}

void NetInternalsMessageHandler::OnGetSharedDictionaryInfoDone(
    const std::string& callback_id,
    std::vector<network::mojom::SharedDictionaryInfoPtr> dictionaries) {
  base::Value::List dict_list;
  for (const auto& item : dictionaries) {
    base::Value::Dict dict;
    dict.Set("match", item->match);
    dict.Set("match_dest", GetMatchDestList(item->match_dest));
    dict.Set("id", item->id);
    dict.Set("dictionary_url", item->dictionary_url.spec());
    dict.Set("last_fetch_time", base::TimeFormatHTTP(item->last_fetch_time));
    dict.Set("response_time", base::TimeFormatHTTP(item->response_time));
    dict.Set("expiration", base::NumberToString(item->expiration.InSeconds()));
    dict.Set("last_used_time", base::TimeFormatHTTP(item->last_used_time));
    dict.Set("size", base::NumberToString(item->size));
    dict.Set("hash", base::ToLowerASCII(base::HexEncode(
                         item->hash.data, sizeof(item->hash.data))));
    dict_list.Append(std::move(dict));
  }
  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), std::move(dict_list));
}

void NetInternalsMessageHandler::OnClearSharedDictionaryDone(
    const std::string& callback_id) {
  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), true);
}

void NetInternalsMessageHandler::OnClearSharedDictionaryForIsolationKeyDone(
    const std::string& callback_id) {
  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), true);
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
  CreateAndAddNetInternalsHTMLSource(Profile::FromWebUI(web_ui));
}

// static
void NetInternalsUI::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context_for_testing) {
  g_network_context_for_testing = network_context_for_testing;
}
