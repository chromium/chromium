// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
#include "services/network/expect_ct_reporter.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
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
      "script-src chrome://resources chrome://test 'self';");
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->DisableTrustedTypesCSP();
  return source;
}

void IgnoreBoolCallback(bool result) {}

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
                                 base::Value result);

  void OnExpectCTTestReportCallback(const std::string& callback_id,
                                    bool success);

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  void OnReloadProxySettings(const base::Value::List& list);
  void OnClearBadProxies(const base::Value::List& list);
  void OnClearHostResolverCache(const base::Value::List& list);
  void OnDomainSecurityPolicyDelete(const base::Value::List& list);
  void OnHSTSQuery(const base::Value::List& list);
  void OnHSTSAdd(const base::Value::List& list);
  void OnExpectCTQuery(const base::Value::List& list);
  void OnExpectCTAdd(const base::Value::List& list);
  void OnExpectCTTestReport(const base::Value::List& list);
  void OnCloseIdleSockets(const base::Value::List& list);
  void OnFlushSocketPools(const base::Value::List& list);

  raw_ptr<content::WebUI> web_ui_;
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
    base::Value result) {
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
    ResolveCallbackWithResult(*callback_id, base::Value("invalid"));
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
  ResolveCallbackWithResult(
      callback_id, success ? base::Value("success") : base::Value("failure"));
}

void NetInternalsMessageHandler::OnFlushSocketPools(
    const base::Value::List& list) {
  GetNetworkContext()->CloseAllConnections(base::NullCallback());
}

void NetInternalsMessageHandler::OnCloseIdleSockets(
    const base::Value::List& list) {
  GetNetworkContext()->CloseIdleConnections(base::NullCallback());
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
