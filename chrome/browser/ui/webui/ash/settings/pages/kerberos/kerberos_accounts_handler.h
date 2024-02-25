// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_KERBEROS_KERBEROS_ACCOUNTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_KERBEROS_KERBEROS_ACCOUNTS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_service.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace kerberos {
class ListAccountsResponse;
}  // namespace kerberos

class Profile;

namespace ash::settings {

class KerberosAccountsHandler : public ::settings::SettingsPageUIHandler,
                                public KerberosCredentialsManager::Observer {
 public:
  static std::unique_ptr<KerberosAccountsHandler> CreateIfKerberosEnabled(
      Profile* profile);

  // Adds load time strings to Kerberos settings UI.
  static void AddLoadTimeKerberosStrings(
      content::WebUIDataSource* html_source,
      KerberosCredentialsManager* kerberos_credentials_manager);

  KerberosAccountsHandler(const KerberosAccountsHandler&) = delete;
  KerberosAccountsHandler& operator=(const KerberosAccountsHandler&) = delete;

  ~KerberosAccountsHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // KerberosCredentialsManager::Observer:
  void OnAccountsChanged() override;

 private:
  explicit KerberosAccountsHandler(
      KerberosCredentialsManager* kerberos_credentials_manager);

  // WebUI "getKerberosAccounts" message callback.
  void HandleGetKerberosAccounts(const base::Value::List& args);

  // WebUI "addKerberosAccount" message callback.
  void HandleAddKerberosAccount(const base::Value::List& args);

  // Callback for the credential manager's AddAccountAndAuthenticate method.
  void OnAddAccountAndAuthenticate(const std::string& callback_id,
                                   kerberos::ErrorType error);

  // WebUI "removeKerberosAccount" message callback.
  void HandleRemoveKerberosAccount(const base::Value::List& args);

  // Callback for the credential manager's RemoveAccount method.
  void OnRemoveAccount(const std::string& callback_id,
                       kerberos::ErrorType error);

  // WebUI "validateKerberosConfig" message callback.
  void HandleValidateKerberosConfig(const base::Value::List& args);

  // Callback for the credential manager's ValidateConfig method.
  void OnValidateConfig(const std::string& callback_id,
                        const kerberos::ValidateConfigResponse& response);

  // WebUI "setAsActiveKerberosAccount" message callback.
  void HandleSetAsActiveKerberosAccount(const base::Value::List& args);

  // Callback for the credential manager's ListAccounts method.
  void OnListAccounts(const std::string& callback_id,
                      const kerberos::ListAccountsResponse& response);

  // Fires the "kerberos-accounts-changed" event, which refreshes the Kerberos
  // Accounts UI.
  void RefreshUI();

  // This instance can be added as observer to KerberosCredentialsManager.
  // This class keeps track of that and removes this instance on destruction.
  base::ScopedObservation<KerberosCredentialsManager,
                          KerberosCredentialsManager::Observer>
      credentials_manager_observation_{this};

  // Not owned.
  raw_ptr<KerberosCredentialsManager> kerberos_credentials_manager_;

  base::WeakPtrFactory<KerberosAccountsHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_KERBEROS_KERBEROS_ACCOUNTS_HANDLER_H_
