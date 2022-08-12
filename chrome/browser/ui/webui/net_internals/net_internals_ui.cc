// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/expect_ct_reporter.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/resources/grit/webui_generated_resources.h"

using content::BrowserThread;

namespace {

content::WebUIDataSource* CreateNetInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetInternalsHost);
  source->AddResourcePaths(
      base::make_span(kNetInternalsResources, kNetInternalsResourcesSize));
  source->SetDefaultResource(IDR_NET_INTERNALS_INDEX_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test chrome://webui-test "
      "'self';");
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->DisableTrustedTypesCSP();
  return source;
}

void IgnoreBoolCallback(bool result) {}

// This function converts net::AddressList to base::Value.
base::Value AddressListToBaseValue(
    const std::vector<net::IPEndPoint>& resolved_addresses) {
  base::Value::List resolved_addresses_base_val_list;
  for (const net::IPEndPoint& resolved_address : resolved_addresses) {
    resolved_addresses_base_val_list.Append(
        resolved_address.ToStringWithoutPort());
  }
  return base::Value(std::move(resolved_addresses_base_val_list));
}

using ResolveHostResult = base::expected<base::Value, std::string>;

// This class implements network::mojom::ResolveHostClient.
class NetInternalsResolveHostClient : public network::mojom::ResolveHostClient {
 public:
  using Callback =
      base::OnceCallback<void(const net::ResolveErrorInfo&,
                              const absl::optional<net::AddressList>&,
                              NetInternalsResolveHostClient*)>;

  NetInternalsResolveHostClient(
      mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver,
      Callback callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&NetInternalsResolveHostClient::OnComplete,
                       base::Unretained(this), net::ERR_FAILED,
                       net::ResolveErrorInfo(net::ERR_FAILED), absl::nullopt));
  }
  ~NetInternalsResolveHostClient() override = default;

  NetInternalsResolveHostClient(const NetInternalsResolveHostClient&) = delete;
  NetInternalsResolveHostClient& operator=(
      const NetInternalsResolveHostClient&) = delete;

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(
      int32_t error,
      const net::ResolveErrorInfo& resolve_error_info,
      const absl::optional<net::AddressList>& resolved_addresses) override {
    std::move(callback_).Run(resolve_error_info, resolved_addresses, this);
  }
  void OnTextResults(const std::vector<std::string>& text_results) override {
    NOTREACHED();
  }
  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override {
    NOTREACHED();
  }

  // This function converts net::AddressList to base::Value.
  static base::Value AddressListToBaseValue(
      const std::vector<net::IPEndPoint>& resolved_addresses) {
    base::Value::List resolved_addresses_base_val_list;
    for (const net::IPEndPoint& resolved_address : resolved_addresses) {
      resolved_addresses_base_val_list.Append(
          resolved_address.ToStringWithoutPort());
    }
    return base::Value(std::move(resolved_addresses_base_val_list));
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

  void OnExpectCTTestReportCallback(const std::string& callback_id,
                                    bool success);

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
  void OnExpectCTQuery(const base::Value::List& list);
  void OnExpectCTAdd(const base::Value::List& list);
  void OnExpectCTTestReport(const base::Value::List& list);
  void OnCloseIdleSockets(const base::Value::List& list);
  void OnFlushSocketPools(const base::Value::List& list);
  void OnResolveHostDone(const std::string& callback_id,
                         const net::ResolveErrorInfo&,
                         const absl::optional<net::AddressList>&,
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
      "expectCTQuery",
      base::BindRepeating(&NetInternalsMessageHandler::OnExpectCTQuery,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "expectCTAdd",
      base::BindRepeating(&NetInternalsMessageHandler::OnExpectCTAdd,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "expectCTTestReport",
      base::BindRepeating(&NetInternalsMessageHandler::OnExpectCTTestReport,
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

  const net::HostPortPair host_port_pair(*hostname, 0);
  const url::Origin origin = url::Origin::Create(GURL("https://" + *hostname));
  AllowJavascript();

  // When ResolveHost() in network process completes, OnResolveHostDone() method
  // is called.
  mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver;
  GetNetworkContext()->ResolveHost(host_port_pair, net::NetworkIsolationKey(),
                                   /*optional_parameters=*/nullptr,
                                   receiver.InitWithNewPipeAndPassRemote());

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

void NetInternalsMessageHandler::OnExpectCTQuery(
    const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  const std::string* domain = list[1].GetIfString();
  DCHECK(callback_id && domain);

  url::Origin origin = url::Origin::Create(GURL("https://" + *domain));
  AllowJavascript();

  GetNetworkContext()->GetExpectCTState(
      *domain,
      net::NetworkIsolationKey(origin /* top_frame_site */,
                               origin /* frame_site */),
      base::BindOnce(&NetInternalsMessageHandler::ResolveCallbackWithResult,
                     weak_factory_.GetWeakPtr(), *callback_id));
}

void NetInternalsMessageHandler::OnExpectCTAdd(const base::Value::List& list) {
  // |list| should be: [<domain to add>, <report URI>, <enforce>].
  const std::string* domain = list[0].GetIfString();
  DCHECK(domain);
  if (!base::IsStringASCII(*domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }

  const std::string* report_uri_str = list[1].GetIfString();
  absl::optional<bool> enforce = list[2].GetIfBool();
  DCHECK(report_uri_str && enforce);

  url::Origin origin = url::Origin::Create(GURL("https://" + *domain));

  base::Time expiry = base::Time::Now() + base::Days(1000);
  GetNetworkContext()->AddExpectCT(
      *domain, expiry, *enforce, GURL(*report_uri_str),
      net::NetworkIsolationKey(origin /* top_frame_site */,
                               origin /* frame_site */),
      base::DoNothing());
}

void NetInternalsMessageHandler::OnExpectCTTestReport(
    const base::Value::List& list) {
  const std::string* callback_id = list[0].GetIfString();
  const std::string* report_uri_str = list[1].GetIfString();
  DCHECK(callback_id && report_uri_str);
  GURL report_uri(*report_uri_str);
  AllowJavascript();
  if (!report_uri.is_valid()) {
    ResolveJavascriptCallback(base::Value(*callback_id),
                              base::Value("invalid"));
    return;
  }

  GetNetworkContext()->SetExpectCTTestReport(
      report_uri,
      base::BindOnce(&NetInternalsMessageHandler::OnExpectCTTestReportCallback,
                     weak_factory_.GetWeakPtr(), *callback_id));
}

void NetInternalsMessageHandler::OnExpectCTTestReportCallback(
    const std::string& callback_id,
    bool success) {
  ResolveJavascriptCallback(
      base::Value(callback_id),
      success ? base::Value("success") : base::Value("failure"));
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
    NetInternalsResolveHostClient* dns_lookup_client) {
  DCHECK_EQ(dns_lookup_clients_.count(dns_lookup_client), 1u);

  if (resolved_addresses) {
    const base::Value result =
        AddressListToBaseValue(resolved_addresses->endpoints());
    ResolveJavascriptCallback(base::Value(callback_id), result);
  } else {
    const base::Value result =
        base::Value(net::ErrorToString(resolve_error_info.error));
    RejectJavascriptCallback(base::Value(callback_id), result);
  }

  auto it = dns_lookup_clients_.find(dns_lookup_client);
  dns_lookup_clients_.erase(it);
}

network::mojom::NetworkContext*
NetInternalsMessageHandler::GetNetworkContext() {
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
