// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cryptohome_web_ui_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/userdataauth/cryptohome_pkcs11_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "crypto/nss_util.h"

using content::BrowserThread;

namespace chromeos {

CryptohomeWebUIHandler::CryptohomeWebUIHandler() {}

CryptohomeWebUIHandler::~CryptohomeWebUIHandler() {}

void CryptohomeWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded", base::BindRepeating(&CryptohomeWebUIHandler::OnPageLoaded,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeWebUIHandler::OnPageLoaded(const base::ListValue* args) {
  UserDataAuthClient* userdataauth_client = UserDataAuthClient::Get();
  CryptohomePkcs11Client* cryptohome_pkcs11_client =
      CryptohomePkcs11Client::Get();

  userdataauth_client->IsMounted(
      user_data_auth::IsMountedRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnIsMounted,
                     weak_ptr_factory_.GetWeakPtr()));
  TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnGetTpmStatus,
                     weak_ptr_factory_.GetWeakPtr()));
  cryptohome_pkcs11_client->Pkcs11IsTpmTokenReady(
      user_data_auth::Pkcs11IsTpmTokenReadyRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnPkcs11IsTpmTokenReady,
                     weak_ptr_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&crypto::IsTPMTokenReady, base::OnceClosure()),
      base::BindOnce(&CryptohomeWebUIHandler::DidGetNSSUtilInfoOnUIThread,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeWebUIHandler::DidGetNSSUtilInfoOnUIThread(
    bool is_tpm_token_ready) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value is_tpm_token_ready_value(is_tpm_token_ready);
  SetCryptohomeProperty("is-tpm-token-ready", is_tpm_token_ready_value);
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

void CryptohomeWebUIHandler::OnIsMounted(
    base::Optional<user_data_auth::IsMountedReply> reply) {
  bool mounted = false;
  if (reply.has_value()) {
    mounted = reply->is_mounted();
  }
  SetCryptohomeProperty("is-mounted", base::Value(mounted));
}

void CryptohomeWebUIHandler::OnPkcs11IsTpmTokenReady(
    base::Optional<user_data_auth::Pkcs11IsTpmTokenReadyReply> reply) {
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

}  // namespace chromeos
