// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cryptohome/cryptohome_web_ui_handler.h"

#include <numeric>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_pkcs11_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "crypto/nss_util.h"

using content::BrowserThread;

namespace ash {

namespace {

// Maximal number of RecoveryId entries we want to display.
constexpr int kRecoveryIdHistoryDepth = 5;

void ForwardToUIThread(base::OnceCallback<void(bool)> ui_callback,
                       bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_callback), result));
}

user_data_auth::GetAuthFactorExtendedInfoRequest
GenerateAuthFactorExtendedInfoRequest(int depth) {
  user_data_auth::GetAuthFactorExtendedInfoRequest req;
  user_data_auth::RecoveryExtendedInfoRequest req_extended_info;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user) {
    *req.mutable_account_id() =
        cryptohome::CreateAccountIdentifierFromAccountId(
            primary_user->GetAccountId());
    *req.mutable_auth_factor_label() = kCryptohomeRecoveryKeyLabel;
    req_extended_info.set_max_depth(depth);
    *req.mutable_recovery_info_request() = std::move(req_extended_info);
  }
  return req;
}

}  // namespace

CryptohomeWebUIHandler::CryptohomeWebUIHandler() {}

CryptohomeWebUIHandler::~CryptohomeWebUIHandler() {}

void CryptohomeWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded", base::BindRepeating(&CryptohomeWebUIHandler::OnPageLoaded,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeWebUIHandler::OnPageLoaded(const base::Value::List& args) {
  UserDataAuthClient* userdataauth_client = UserDataAuthClient::Get();
  CryptohomePkcs11Client* cryptohome_pkcs11_client =
      CryptohomePkcs11Client::Get();

  userdataauth_client->IsMounted(
      user_data_auth::IsMountedRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnIsMounted,
                     weak_ptr_factory_.GetWeakPtr()));
  chromeos::TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnGetTpmStatus,
                     weak_ptr_factory_.GetWeakPtr()));
  cryptohome_pkcs11_client->Pkcs11IsTpmTokenReady(
      user_data_auth::Pkcs11IsTpmTokenReadyRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnPkcs11IsTpmTokenReady,
                     weak_ptr_factory_.GetWeakPtr()));

  user_data_auth::GetAuthFactorExtendedInfoRequest req =
      GenerateAuthFactorExtendedInfoRequest(kRecoveryIdHistoryDepth);
  userdataauth_client->GetAuthFactorExtendedInfo(
      req, base::BindOnce(&CryptohomeWebUIHandler::OnGetAuthFactorExtendedInfo,
                          weak_ptr_factory_.GetWeakPtr()));

  auto ui_callback =
      base::BindOnce(&CryptohomeWebUIHandler::GotIsTPMTokenEnabledOnUIThread,
                     weak_ptr_factory_.GetWeakPtr());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&crypto::IsTPMTokenEnabled,
                                base::BindOnce(&ForwardToUIThread,
                                               std::move(ui_callback))));
}

void CryptohomeWebUIHandler::GotIsTPMTokenEnabledOnUIThread(
    bool is_tpm_token_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value is_tpm_token_enabled_value(is_tpm_token_enabled);
  SetCryptohomeProperty("is-tpm-token-ready",
                        std::move(is_tpm_token_enabled_value));
}

void CryptohomeWebUIHandler::OnGetTpmStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
  if (reply.status() != ::tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to get TPM status; status: " << reply.status();
    return;
  }
  // It also means TPM is ready if tpm manager reports TPM is owned.
  SetCryptohomeProperty("tpm-is-ready", base::Value(reply.is_owned()));
  SetCryptohomeProperty("tpm-is-enabled", base::Value(reply.is_enabled()));
  SetCryptohomeProperty("tpm-is-owned", base::Value(reply.is_owned()));
  SetCryptohomeProperty("has-reset-lock-permissions",
                        base::Value(reply.has_reset_lock_permissions()));
}

void CryptohomeWebUIHandler::OnGetAuthFactorExtendedInfo(
    std::optional<user_data_auth::GetAuthFactorExtendedInfoReply> reply) {
  std::string recovery_ids = "<empty>";
  if (reply.has_value() &&
      !reply->recovery_info_reply().recovery_ids().empty()) {
    recovery_ids =
        std::accumulate(reply->recovery_info_reply().recovery_ids().begin(),
                        reply->recovery_info_reply().recovery_ids().end(),
                        std::string(), [](std::string ss, std::string s) {
                          return ss.empty() ? s : ss + " " + s;
                        });
  }
  SetCryptohomeProperty("recovery_ids", base::Value(recovery_ids));
}

void CryptohomeWebUIHandler::OnIsMounted(
    std::optional<user_data_auth::IsMountedReply> reply) {
  bool mounted = false;
  if (reply.has_value()) {
    mounted = reply->is_mounted();
  }
  SetCryptohomeProperty("is-mounted", base::Value(mounted));
}

void CryptohomeWebUIHandler::OnPkcs11IsTpmTokenReady(
    std::optional<user_data_auth::Pkcs11IsTpmTokenReadyReply> reply) {
  bool ready = false;
  if (reply.has_value()) {
    ready = reply->ready();
  }
  SetCryptohomeProperty("pkcs11-is-tpm-token-ready", base::Value(ready));
}

void CryptohomeWebUIHandler::SetCryptohomeProperty(
    const std::string& destination_id,
    const base::Value& value) {
  base::Value destination_id_value(destination_id);
  web_ui()->CallJavascriptFunctionUnsafe("SetCryptohomeProperty",
                                         destination_id_value, value);
}

}  // namespace ash
