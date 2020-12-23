// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cryptohome_web_ui_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
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
  CryptohomeClient* cryptohome_client = CryptohomeClient::Get();

  cryptohome_client->IsMounted(GetCryptohomeBoolCallback("is-mounted"));
  TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
      ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnGetTpmStatus,
                     weak_ptr_factory_.GetWeakPtr()));
  cryptohome_client->Pkcs11IsTpmTokenReady(
      GetCryptohomeBoolCallback("pkcs11-is-tpm-token-ready"));

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

DBusMethodCallback<bool> CryptohomeWebUIHandler::GetCryptohomeBoolCallback(
    const std::string& destination_id) {
  return base::BindOnce(&CryptohomeWebUIHandler::OnCryptohomeBoolProperty,
                        weak_ptr_factory_.GetWeakPtr(), destination_id);
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

void CryptohomeWebUIHandler::OnCryptohomeBoolProperty(
    const std::string& destination_id,
    base::Optional<bool> result) {
  SetCryptohomeProperty(destination_id, base::Value(result.value_or(false)));
}

void CryptohomeWebUIHandler::SetCryptohomeProperty(
    const std::string& destination_id,
    const base::Value& value) {
  base::Value destination_id_value(destination_id);
  web_ui()->CallJavascriptFunctionUnsafe("SetCryptohomeProperty",
                                         destination_id_value, value);
}

}  // namespace chromeos
