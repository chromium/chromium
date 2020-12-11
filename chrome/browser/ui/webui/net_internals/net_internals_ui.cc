// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/net_export_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/net_internals_resources.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/log/net_log_util.h"
#include "services/network/expect_ct_reporter.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

using content::BrowserThread;

namespace {

content::WebUIDataSource* CreateNetInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetInternalsHost);

  source->SetDefaultResource(IDR_NET_INTERNALS_INDEX_HTML);
  source->AddResourcePath("index.js", IDR_NET_INTERNALS_INDEX_JS);
  source->UseStringsJs();
  return source;
}

void IgnoreBoolCallback(bool result) {}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class NetInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public base::SupportsWeakPtr<NetInternalsMessageHandler> {
 public:
  explicit NetInternalsMessageHandler(content::WebUI* web_ui);
  ~NetInternalsMessageHandler() override = default;

 protected:
  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

 private:
  network::mojom::NetworkContext* GetNetworkContext();

  // Calls g_browser.receive in the renderer, passing in |command| and |arg|.
  // If the renderer is displaying a log file, the message will be ignored.
  void SendJavascriptCommand(const std::string& command, base::Value arg);

  void OnExpectCTTestReportCallback(bool success);

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  void OnReloadProxySettings(const base::ListValue* list);
  void OnClearBadProxies(const base::ListValue* list);
  void OnClearHostResolverCache(const base::ListValue* list);
  void OnDomainSecurityPolicyDelete(const base::ListValue* list);
  void OnHSTSQuery(const base::ListValue* list);
  void OnHSTSAdd(const base::ListValue* list);
  void OnExpectCTQuery(const base::ListValue* list);
  void OnExpectCTAdd(const base::ListValue* list);
  void OnExpectCTTestReport(const base::ListValue* list);
  void OnCloseIdleSockets(const base::ListValue* list);
  void OnFlushSocketPools(const base::ListValue* list);

  content::WebUI* web_ui_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsMessageHandler);
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

void NetInternalsMessageHandler::SendJavascriptCommand(
    const std::string& command,
    base::Value arg) {
  std::unique_ptr<base::Value> command_value(new base::Value(command));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_ui()->CallJavascriptFunctionUnsafe("g_browser.receive",
                                         *command_value.get(), arg);
}

void NetInternalsMessageHandler::OnReloadProxySettings(
    const base::ListValue* list) {
  GetNetworkContext()->ForceReloadProxyConfig(base::NullCallback());
}

void NetInternalsMessageHandler::OnClearBadProxies(
    const base::ListValue* list) {
  GetNetworkContext()->ClearBadProxiesCache(base::NullCallback());
}

void NetInternalsMessageHandler::OnClearHostResolverCache(
    const base::ListValue* list) {
  GetNetworkContext()->ClearHostCache(/*filter=*/nullptr, base::NullCallback());
}

void NetInternalsMessageHandler::OnDomainSecurityPolicyDelete(
    const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  bool result = list->GetString(0, &domain);
  DCHECK(result);
  if (!base::IsStringASCII(domain)) {
    // There cannot be a unicode entry in the HSTS set.
    return;
  }
  GetNetworkContext()->DeleteDynamicDataForHost(
      domain, base::BindOnce(&IgnoreBoolCallback));
}

void NetInternalsMessageHandler::OnHSTSQuery(const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  bool get_domain_result = list->GetString(0, &domain);
  DCHECK(get_domain_result);

  GetNetworkContext()->GetHSTSState(
      domain, base::BindOnce(&NetInternalsMessageHandler::SendJavascriptCommand,
                             this->AsWeakPtr(), "receivedHSTSResult"));
}

void NetInternalsMessageHandler::OnHSTSAdd(const base::ListValue* list) {
  // |list| should be: [<domain to query>, <STS include subdomains>]
  std::string domain;
  bool result = list->GetString(0, &domain);
  DCHECK(result);
  if (!base::IsStringASCII(domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }
  bool sts_include_subdomains;
  result = list->GetBoolean(1, &sts_include_subdomains);
  DCHECK(result);

  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  GetNetworkContext()->AddHSTS(domain, expiry, sts_include_subdomains,
                               base::DoNothing());
}

void NetInternalsMessageHandler::OnExpectCTQuery(const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  bool result = list->GetString(0, &domain);
  DCHECK(result);

  url::Origin origin = url::Origin::Create(GURL("https://" + domain));

  GetNetworkContext()->GetExpectCTState(
      domain,
      net::NetworkIsolationKey(origin /* top_frame_site */,
                               origin /* frame_site */),
      base::BindOnce(&NetInternalsMessageHandler::SendJavascriptCommand,
                     this->AsWeakPtr(), "receivedExpectCTResult"));
}

void NetInternalsMessageHandler::OnExpectCTAdd(const base::ListValue* list) {
  // |list| should be: [<domain to add>, <report URI>, <enforce>].
  std::string domain;
  bool result = list->GetString(0, &domain);
  DCHECK(result);
  if (!base::IsStringASCII(domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }

  std::string report_uri_str;
  result = list->GetString(1, &report_uri_str);
  DCHECK(result);
  bool enforce;
  result = list->GetBoolean(2, &enforce);
  DCHECK(result);

  url::Origin origin = url::Origin::Create(GURL("https://" + domain));

  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  GetNetworkContext()->AddExpectCT(
      domain, expiry, enforce, GURL(report_uri_str),
      net::NetworkIsolationKey(origin /* top_frame_site */,
                               origin /* frame_site */),
      base::DoNothing());
}

void NetInternalsMessageHandler::OnExpectCTTestReport(
    const base::ListValue* list) {
  // |list| should be: [<report URI>].
  std::string report_uri_str;
  bool result = list->GetString(0, &report_uri_str);
  DCHECK(result);
  GURL report_uri(report_uri_str);
  if (!report_uri.is_valid())
    return;

  GetNetworkContext()->SetExpectCTTestReport(
      report_uri,
      base::BindOnce(&NetInternalsMessageHandler::OnExpectCTTestReportCallback,
                     this->AsWeakPtr()));
}

void NetInternalsMessageHandler::OnExpectCTTestReportCallback(bool success) {
  SendJavascriptCommand(
      "receivedExpectCTTestReportResult",
      success ? base::Value("success") : base::Value("failure"));
}

void NetInternalsMessageHandler::OnFlushSocketPools(
    const base::ListValue* list) {
  GetNetworkContext()->CloseAllConnections(base::NullCallback());
}

void NetInternalsMessageHandler::OnCloseIdleSockets(
    const base::ListValue* list) {
  GetNetworkContext()->CloseIdleConnections(base::NullCallback());
}

network::mojom::NetworkContext*
NetInternalsMessageHandler::GetNetworkContext() {
  return content::BrowserContext::GetDefaultStoragePartition(
             web_ui_->GetWebContents()->GetBrowserContext())
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
