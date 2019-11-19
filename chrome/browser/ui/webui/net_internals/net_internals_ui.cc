// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/net/net_export_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/net_internals_resources.h"
#include "components/onc/onc_constants.h"
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
#include "services/network/public/mojom/network_context.mojom.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system_logs/debug_log_writer.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "chrome/common/logging_chrome.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "chromeos/network/onc/onc_utils.h"
#endif

using content::BrowserThread;

namespace {

content::WebUIDataSource* CreateNetInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetInternalsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

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

#if defined(OS_CHROMEOS)
  // Callback to |GetNSSCertDatabaseForProfile| used to retrieve the database
  // to which user's ONC defined certificates should be imported.
  // It parses and imports |onc_blob|.
  void ImportONCFileToNSSDB(const std::string& onc_blob,
                            const std::string& passcode,
                            net::NSSCertDatabase* nssdb);

  // Called back by the CertificateImporter when a certificate import finished.
  // |previous_error| contains earlier errors during this import.
  void OnCertificatesImported(const std::string& previous_error,
                              bool cert_import_success);
#endif

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
#if defined(OS_CHROMEOS)
  void OnDumpPolicyLogsCompleted(const base::FilePath& path,
                                 bool should_compress,
                                 bool combined,
                                 const char* received_event);
  void OnImportONCFile(const base::ListValue* list);
  void OnStoreDebugLogs(bool combined,
                        const char* received_event,
                        const base::ListValue* list);
  void OnStoreDebugLogsCompleted(const char* received_event,
                                 const base::FilePath& log_path,
                                 bool succeeded);
  void OnSetNetworkDebugMode(const base::ListValue* list);
  void OnSetNetworkDebugModeCompleted(const std::string& subsystem,
                                      bool succeeded);
#endif

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
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "importONCFile",
      base::BindRepeating(&NetInternalsMessageHandler::OnImportONCFile,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "storeDebugLogs",
      base::BindRepeating(&NetInternalsMessageHandler::OnStoreDebugLogs,
                          base::Unretained(this), false /* combined */,
                          "receivedStoreDebugLogs"));
  web_ui()->RegisterMessageCallback(
      "storeCombinedDebugLogs",
      base::BindRepeating(&NetInternalsMessageHandler::OnStoreDebugLogs,
                          base::Unretained(this), true /* combined */,
                          "receivedStoreCombinedDebugLogs"));
  web_ui()->RegisterMessageCallback(
      "setNetworkDebugMode",
      base::BindRepeating(&NetInternalsMessageHandler::OnSetNetworkDebugMode,
                          base::Unretained(this)));
#endif
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
  bool domain_result = list->GetString(0, &domain);
  DCHECK(domain_result);

  GetNetworkContext()->GetExpectCTState(
      domain, base::BindOnce(&NetInternalsMessageHandler::SendJavascriptCommand,
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

  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  GetNetworkContext()->AddExpectCT(domain, expiry, enforce,
                                   GURL(report_uri_str), base::DoNothing());
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

#if defined(OS_CHROMEOS)
void NetInternalsMessageHandler::ImportONCFileToNSSDB(
    const std::string& onc_blob,
    const std::string& passcode,
    net::NSSCertDatabase* nssdb) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromWebUI(web_ui()));

  if (!user) {
    std::string error = "User not found.";
    SendJavascriptCommand("receivedONCFileParse", base::Value(error));
    return;
  }

  std::string error;
  onc::ONCSource onc_source = onc::ONC_SOURCE_USER_IMPORT;
  base::ListValue network_configs;
  base::DictionaryValue global_network_config;
  base::ListValue certificates;
  if (!chromeos::onc::ParseAndValidateOncForImport(onc_blob,
                                                   onc_source,
                                                   passcode,
                                                   &network_configs,
                                                   &global_network_config,
                                                   &certificates)) {
    error = "Errors occurred during the ONC parsing. ";
  }

  std::string network_error;
  chromeos::onc::ImportNetworksForUser(user, network_configs, &network_error);
  if (!network_error.empty())
    error += network_error;

  chromeos::onc::CertificateImporterImpl cert_importer(
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}), nssdb);
  auto certs =
      std::make_unique<chromeos::onc::OncParsedCertificates>(certificates);
  if (certs->has_error())
    error += "Some certificates couldn't be parsed. ";
  cert_importer.ImportAllCertificatesUserInitiated(
      certs->server_or_authority_certificates(), certs->client_certificates(),
      base::Bind(&NetInternalsMessageHandler::OnCertificatesImported,
                 AsWeakPtr(), error /* previous_error */));
}

void NetInternalsMessageHandler::OnCertificatesImported(
    const std::string& previous_error,
    bool cert_import_success) {
  std::string error = previous_error;
  if (!cert_import_success)
    error += "Some certificates couldn't be imported. ";

  SendJavascriptCommand("receivedONCFileParse", base::Value(error));
}

void NetInternalsMessageHandler::OnImportONCFile(
    const base::ListValue* list) {
  std::string onc_blob;
  std::string passcode;
  if (list->GetSize() != 2 ||
      !list->GetString(0, &onc_blob) ||
      !list->GetString(1, &passcode)) {
    NOTREACHED();
  }

  GetNSSCertDatabaseForProfile(
      Profile::FromWebUI(web_ui()),
      base::Bind(&NetInternalsMessageHandler::ImportONCFileToNSSDB, AsWeakPtr(),
                 onc_blob, passcode));
}

void DumpPolicyLogs(base::FilePath file_path, std::string json_policies) {
  file_path = logging::GenerateTimestampedName(file_path, base::Time::Now());
  base::WriteFile(file_path, json_policies.data(), json_policies.size());
}

void NetInternalsMessageHandler::OnStoreDebugLogs(bool combined,
                                                  const char* received_event,
                                                  const base::ListValue* list) {
  DCHECK(list);

  SendJavascriptCommand(received_event, base::Value("Creating log file..."));
  Profile* profile = Profile::FromWebUI(web_ui());
  const DownloadPrefs* const prefs = DownloadPrefs::FromBrowserContext(profile);
  base::FilePath path = prefs->DownloadPath();
  if (file_manager::util::IsUnderNonNativeLocalPath(profile, path))
    path = prefs->GetDefaultDownloadDirectoryForProfile();
  base::FilePath policies_path = path.Append("policies.json");
  std::string json_policies =
      policy::DictionaryPolicyConversions()
          .WithBrowserContext(web_ui()->GetWebContents()->GetBrowserContext())
          .ToJSON();
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(DumpPolicyLogs, policies_path, json_policies),
      base::BindOnce(&NetInternalsMessageHandler::OnDumpPolicyLogsCompleted,
                     AsWeakPtr(), path, true /* should_compress */, combined,
                     received_event));
}

void NetInternalsMessageHandler::OnDumpPolicyLogsCompleted(
    const base::FilePath& path,
    bool should_compress,
    bool combined,
    const char* received_event) {
  if (combined) {
    chromeos::DebugLogWriter::StoreCombinedLogs(
        path,
        base::BindOnce(&NetInternalsMessageHandler::OnStoreDebugLogsCompleted,
                       AsWeakPtr(), received_event));
  } else {
    chromeos::DebugLogWriter::StoreLogs(
        path, should_compress,
        base::BindOnce(&NetInternalsMessageHandler::OnStoreDebugLogsCompleted,
                       AsWeakPtr(), received_event));
  }
}

void NetInternalsMessageHandler::OnStoreDebugLogsCompleted(
    const char* received_event,
    const base::FilePath& log_path,
    bool succeeded) {
  std::string status;
  if (succeeded)
    status = "Created log file: " + log_path.BaseName().AsUTF8Unsafe();
  else
    status = "Failed to create log file";
  SendJavascriptCommand(received_event, base::Value(status));
}

void NetInternalsMessageHandler::OnSetNetworkDebugMode(
    const base::ListValue* list) {
  std::string subsystem;
  if (list->GetSize() != 1 || !list->GetString(0, &subsystem))
    NOTREACHED();
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
      SetDebugMode(
          subsystem,
          base::Bind(
              &NetInternalsMessageHandler::OnSetNetworkDebugModeCompleted,
              AsWeakPtr(),
              subsystem));
}

void NetInternalsMessageHandler::OnSetNetworkDebugModeCompleted(
    const std::string& subsystem,
    bool succeeded) {
  std::string status = succeeded ? "Debug mode is changed to "
                                 : "Failed to change debug mode to ";
  status += subsystem;
  SendJavascriptCommand("receivedSetNetworkDebugMode", base::Value(status));
}
#endif  // defined(OS_CHROMEOS)

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
