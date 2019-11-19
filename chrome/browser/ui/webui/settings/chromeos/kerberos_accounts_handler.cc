// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/kerberos_accounts_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace settings {

KerberosAccountsHandler::KerberosAccountsHandler() = default;
KerberosAccountsHandler::~KerberosAccountsHandler() = default;

void KerberosAccountsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getKerberosAccounts",
      base::BindRepeating(&KerberosAccountsHandler::HandleGetKerberosAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleAddKerberosAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleRemoveKerberosAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "validateKerberosConfig",
      base::BindRepeating(
          &KerberosAccountsHandler::HandleValidateKerberosConfig,
          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setAsActiveKerberosAccount",
      base::BindRepeating(
          &KerberosAccountsHandler::HandleSetAsActiveKerberosAccount,
          weak_factory_.GetWeakPtr()));
}

void KerberosAccountsHandler::HandleGetKerberosAccounts(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const std::string& callback_id = args->GetList()[0].GetString();

  if (!KerberosCredentialsManager::Get().IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  KerberosCredentialsManager::Get().ListAccounts(
      base::BindOnce(&KerberosAccountsHandler::OnListAccounts,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnListAccounts(
    const std::string& callback_id,
    const kerberos::ListAccountsResponse& response) {
  base::ListValue accounts;

  // Ticket icon is a key.
  gfx::ImageSkia skia_ticket_icon =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_KERBEROS_ICON_KEY);
  std::string ticket_icon = webui::GetBitmapDataUrl(
      skia_ticket_icon.GetRepresentation(1.0f).GetBitmap());

  const std::string& active_principal =
      KerberosCredentialsManager::Get().GetActiveAccount();

  for (int n = 0; n < response.accounts_size(); ++n) {
    const kerberos::Account& account = response.accounts(n);

    // Format validity time as 'xx hours yy minutes' for validity < 1 day and
    // 'nn days' otherwise.
    base::TimeDelta tgt_validity =
        base::TimeDelta::FromSeconds(account.tgt_validity_seconds());
    const base::string16 valid_for_duration = ui::TimeFormat::Detailed(
        ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
        tgt_validity < base::TimeDelta::FromDays(1) ? -1 : 0, tgt_validity);

    base::DictionaryValue account_dict;
    account_dict.SetString("principalName", account.principal_name());
    account_dict.SetString("config", account.krb5conf());
    account_dict.SetBoolean("isSignedIn", account.tgt_validity_seconds() > 0);
    account_dict.SetString("validForDuration", valid_for_duration);
    account_dict.SetBoolean("isActive",
                            account.principal_name() == active_principal);
    account_dict.SetBoolean("isManaged", account.is_managed());
    account_dict.SetBoolean("passwordWasRemembered",
                            account.password_was_remembered());
    account_dict.SetString("pic", ticket_icon);
    accounts.Append(std::move(account_dict));
  }

  ResolveJavascriptCallback(base::Value(callback_id), std::move(accounts));
}

void KerberosAccountsHandler::HandleAddKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  // TODO(https://crbug.com/961246):
  //   - Prevent account changes when Kerberos is disabled.
  //   - Remove all accounts when Kerberos is disabled.

  CHECK_EQ(6U, args->GetSize());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& principal_name = args->GetList()[1].GetString();
  const std::string& password = args->GetList()[2].GetString();
  const bool remember_password = args->GetList()[3].GetBool();
  const std::string& config = args->GetList()[4].GetString();
  const bool allow_existing = args->GetList()[5].GetBool();

  if (!KerberosCredentialsManager::Get().IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(kerberos::ERROR_KERBEROS_DISABLED));
    return;
  }

  KerberosCredentialsManager::Get().AddAccountAndAuthenticate(
      principal_name, false /* is_managed */, password, remember_password,
      config, allow_existing,
      base::BindOnce(&KerberosAccountsHandler::OnAddAccountAndAuthenticate,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnAddAccountAndAuthenticate(
    const std::string& callback_id,
    kerberos::ErrorType error) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(error)));
}

void KerberosAccountsHandler::HandleRemoveKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& principal_name = args->GetList()[1].GetString();

  if (!KerberosCredentialsManager::Get().IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(kerberos::ERROR_KERBEROS_DISABLED));
    return;
  }

  KerberosCredentialsManager::Get().RemoveAccount(
      principal_name, base::BindOnce(&KerberosAccountsHandler::OnRemoveAccount,
                                     weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnRemoveAccount(const std::string& callback_id,
                                              kerberos::ErrorType error) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(error)));
}

void KerberosAccountsHandler::HandleValidateKerberosConfig(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& krb5conf = args->GetList()[1].GetString();

  if (!KerberosCredentialsManager::Get().IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(kerberos::ERROR_KERBEROS_DISABLED));
    return;
  }

  KerberosCredentialsManager::Get().ValidateConfig(
      krb5conf, base::BindOnce(&KerberosAccountsHandler::OnValidateConfig,
                               weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnValidateConfig(
    const std::string& callback_id,
    const kerberos::ValidateConfigResponse& response) {
  base::Value error_info(base::Value::Type::DICTIONARY);
  error_info.SetKey("code", base::Value(response.error_info().code()));
  if (response.error_info().has_line_index()) {
    error_info.SetKey("lineIndex",
                      base::Value(response.error_info().line_index()));
  }

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("error", base::Value(static_cast<int>(response.error())));
  value.SetKey("errorInfo", std::move(error_info));
  ResolveJavascriptCallback(base::Value(callback_id), std::move(value));
}

void KerberosAccountsHandler::HandleSetAsActiveKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const std::string& principal_name = args->GetList()[0].GetString();

  KerberosCredentialsManager::Get().SetActiveAccount(principal_name);
}

void KerberosAccountsHandler::OnJavascriptAllowed() {
  credentials_manager_observer_.Add(&KerberosCredentialsManager::Get());
}

void KerberosAccountsHandler::OnJavascriptDisallowed() {
  credentials_manager_observer_.RemoveAll();
}

void KerberosAccountsHandler::OnAccountsChanged() {
  RefreshUI();
}

void KerberosAccountsHandler::RefreshUI() {
  FireWebUIListener("kerberos-accounts-changed");
}

}  // namespace settings
}  // namespace chromeos
