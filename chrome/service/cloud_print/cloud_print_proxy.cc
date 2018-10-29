// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/service/cloud_print/print_system.h"
#include "chrome/service/service_process.h"
#include "chrome/service/service_process_prefs.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "url/gurl.h"

namespace cloud_print {

CloudPrintProxy::CloudPrintProxy() = default;

CloudPrintProxy::~CloudPrintProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShutdownBackend();
}

void CloudPrintProxy::Initialize(
    ServiceProcessPrefs* service_prefs,
    Client* client,
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_connection_tracker);
  service_prefs_ = service_prefs;
  client_ = client;
  network_connection_tracker_ = network_connection_tracker;
}

void CloudPrintProxy::EnableForUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CreateBackend())
    return;
  DCHECK(backend_.get());
  // Read persisted robot credentials because we may decide to reuse it if the
  // passed in LSID belongs the same user.
  std::string robot_refresh_token = service_prefs_->GetString(
      prefs::kCloudPrintRobotRefreshToken, std::string());
  std::string robot_email =
      service_prefs_->GetString(prefs::kCloudPrintRobotEmail, std::string());
  user_email_ = service_prefs_->GetString(prefs::kCloudPrintEmail, user_email_);

  // See if we have persisted robot credentials.
  if (!robot_refresh_token.empty()) {
    DCHECK(!robot_email.empty());
    backend_->InitializeWithRobotToken(robot_refresh_token, robot_email);
  } else {
    // Finally see if we have persisted user credentials (legacy case).
    std::string cloud_print_token =
        service_prefs_->GetString(prefs::kCloudPrintAuthToken, std::string());
    DCHECK(!cloud_print_token.empty());
    backend_->InitializeWithToken(cloud_print_token);
  }
  if (client_) {
    client_->OnCloudPrintProxyEnabled(true);
  }
}

void CloudPrintProxy::EnableForUserWithRobot(const std::string& robot_auth_code,
                                             const std::string& robot_email,
                                             const std::string& user_email,
                                             base::Value user_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ShutdownBackend();
  std::string proxy_id(
      service_prefs_->GetString(prefs::kCloudPrintProxyId, std::string()));
  service_prefs_->RemovePref(prefs::kCloudPrintRoot);
  if (!proxy_id.empty()) {
    // Keep only proxy id;
    service_prefs_->SetString(prefs::kCloudPrintProxyId, proxy_id);
  }
  service_prefs_->SetValue(prefs::kCloudPrintUserSettings,
                           user_settings.CreateDeepCopy());
  service_prefs_->WritePrefs();

  if (!CreateBackend())
    return;
  DCHECK(backend_.get());
  user_email_ = user_email;
  backend_->InitializeWithRobotAuthCode(robot_auth_code, robot_email);
  if (client_) {
    client_->OnCloudPrintProxyEnabled(true);
  }
}

bool CloudPrintProxy::CreateBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_.get())
    return false;

  ConnectorSettings settings;
  settings.InitFrom(service_prefs_);

  // By default we don't poll for jobs when we lose XMPP connection. But this
  // behavior can be overridden by a preference.
  bool enable_job_poll =
    service_prefs_->GetBoolean(prefs::kCloudPrintEnableJobPoll, false);

  gaia::OAuthClientInfo oauth_client_info;
  oauth_client_info.client_id =
    google_apis::GetOAuth2ClientID(google_apis::CLIENT_CLOUD_PRINT);
  oauth_client_info.client_secret =
    google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_CLOUD_PRINT);
  oauth_client_info.redirect_uri = "oob";
  backend_ = std::make_unique<CloudPrintProxyBackend>(
      this, settings, oauth_client_info, enable_job_poll,
      network_connection_tracker_);
  return true;
}

void CloudPrintProxy::UnregisterPrintersAndDisableForUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_.get()) {
    // Try getting auth and printers info from the backend.
    // We'll get notified in this case.
    backend_->UnregisterPrinters();
  } else {
    // If no backend available, disable connector immediately.
    DisableForUser();
  }
}

void CloudPrintProxy::DisableForUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_email_.clear();
  enabled_ = false;
  if (client_) {
    client_->OnCloudPrintProxyDisabled(true);
  }
  ShutdownBackend();
}

void CloudPrintProxy::GetProxyInfo(CloudPrintProxyInfo* info) {
  info->enabled = enabled_;
  info->email.clear();
  if (enabled_)
    info->email = user_email();
  ConnectorSettings settings;
  settings.InitFrom(service_prefs_);
  info->proxy_id = settings.proxy_id();
}

void CloudPrintProxy::GetPrinters(std::vector<std::string>* printers) {
  ConnectorSettings settings;
  settings.InitFrom(service_prefs_);
  scoped_refptr<PrintSystem> print_system =
      PrintSystem::CreateInstance(settings.print_system_settings());
  if (!print_system.get())
    return;
  PrintSystem::PrintSystemResult result = print_system->Init();
  if (!result.succeeded())
    return;
  printing::PrinterList printer_list;
  print_system->EnumeratePrinters(&printer_list);
  for (size_t i = 0; i < printer_list.size(); ++i)
    printers->push_back(printer_list[i].printer_name);
}

void CloudPrintProxy::OnAuthenticated(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_prefs_->SetString(prefs::kCloudPrintRobotRefreshToken,
                            robot_oauth_refresh_token);
  service_prefs_->SetString(prefs::kCloudPrintRobotEmail,
                            robot_email);
  // If authenticating from a robot, the user email will be empty.
  if (!user_email.empty()) {
    user_email_ = user_email;
  }
  service_prefs_->SetString(prefs::kCloudPrintEmail, user_email_);
  enabled_ = true;
  DCHECK(!user_email_.empty());
  service_prefs_->WritePrefs();
  // When this switch used we don't want connector continue running, we just
  // need authentication.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCloudPrintSetupProxy)) {
    ShutdownBackend();
    if (client_) {
      client_->OnCloudPrintProxyDisabled(false);
    }
  }
}

void CloudPrintProxy::OnAuthenticationFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't disable permanently. Could be just connection issue.
  ShutdownBackend();
  if (client_) {
    client_->OnCloudPrintProxyDisabled(false);
  }
}

void CloudPrintProxy::OnPrintSystemUnavailable() {
  // If the print system is unavailable, we want to shutdown the proxy and
  // disable it non-persistently.
  ShutdownBackend();
  if (client_) {
    client_->OnCloudPrintProxyDisabled(false);
  }
}

void CloudPrintProxy::OnUnregisterPrinters(
    const std::string& auth_token,
    const std::list<std::string>& printer_ids) {
  UMA_HISTOGRAM_COUNTS_10000("CloudPrint.UnregisterPrinters",
                             printer_ids.size());
  ShutdownBackend();
  ConnectorSettings settings;
  settings.InitFrom(service_prefs_);
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("cloud_print_proxy",
                                                 "cloud_print", R"(
          semantics {
            description:
              "Sends a request to Cloud Print to unregister one or more "
              "printers."
            trigger:
              "User request of unregistering printers or a change of an admin "
              "policy regarding Cloud Print."
            data: "OAuth2 token and list of printer ids to unregister."
          })");
  wipeout_.reset(new CloudPrintWipeout(this, settings.server_url(),
                                       partial_traffic_annotation));
  wipeout_->UnregisterPrinters(auth_token, printer_ids);
}

void CloudPrintProxy::OnXmppPingUpdated(int ping_timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_prefs_->SetInt(prefs::kCloudPrintXmppPingTimeout, ping_timeout);
  service_prefs_->WritePrefs();
}

void CloudPrintProxy::OnUnregisterPrintersComplete() {
  wipeout_.reset();
  // Finish disabling cloud print for this user.
  DisableForUser();
}

void CloudPrintProxy::ShutdownBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_.get())
    backend_->Shutdown();
  backend_.reset();
}

}  // namespace cloud_print
