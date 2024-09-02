// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CRYPTOHOME_CRYPTOHOME_WEB_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CRYPTOHOME_CRYPTOHOME_WEB_UI_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {

class Value;

}  // namespace base

namespace ash {

// Class to handle messages from chrome://cryptohome.
class CryptohomeWebUIHandler : public content::WebUIMessageHandler {
 public:
  CryptohomeWebUIHandler();

  CryptohomeWebUIHandler(const CryptohomeWebUIHandler&) = delete;
  CryptohomeWebUIHandler& operator=(const CryptohomeWebUIHandler&) = delete;

  ~CryptohomeWebUIHandler() override;

  // WebUIMessageHandler override.
  void RegisterMessages() override;

 private:
  // This method is called from JavaScript.
  void OnPageLoaded(const base::Value::List& args);

  void GotIsTPMTokenEnabledOnUIThread(bool is_tpm_token_enabled);

  void OnIsMounted(std::optional<user_data_auth::IsMountedReply> reply);
  void OnPkcs11IsTpmTokenReady(
      std::optional<user_data_auth::Pkcs11IsTpmTokenReadyReply> reply);

  // This method is called when TpmManager D-Bus GetTpmNonsensitiveStatus call
  // completes.
  void OnGetTpmStatus(
      const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply);

  // Called when Cryptohome D-Bus GetAuthFactorExtendedInfo call completes.
  // Gets requested AuthFactor with additional metadata in reply.
  void OnGetAuthFactorExtendedInfo(
      std::optional<user_data_auth::GetAuthFactorExtendedInfoReply> reply);

  // Sets textcontent of the element whose id is |destination_id| to |value|.
  void SetCryptohomeProperty(const std::string& destination_id,
                             const base::Value& value);

  base::WeakPtrFactory<CryptohomeWebUIHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CRYPTOHOME_CRYPTOHOME_WEB_UI_HANDLER_H_
