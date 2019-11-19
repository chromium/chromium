// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_WEB_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_WEB_UI_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {

class Value;

}  // base

namespace chromeos {

// Class to handle messages from chrome://cryptohome.
class CryptohomeWebUIHandler : public content::WebUIMessageHandler {
 public:
  CryptohomeWebUIHandler();

  ~CryptohomeWebUIHandler() override;

  // WebUIMessageHandler override.
  void RegisterMessages() override;

 private:
  // This method is called from JavaScript.
  void OnPageLoaded(const base::ListValue* args);

  void DidGetNSSUtilInfoOnUIThread(bool is_tpm_token_ready);

  // Returns a callback to handle Cryptohome property values.
  DBusMethodCallback<bool> GetCryptohomeBoolCallback(
      const std::string& destination_id);

  // This method is called when Cryptohome D-Bus method call completes.
  void OnCryptohomeBoolProperty(const std::string& destination_id,
                                base::Optional<bool> result);

  // Sets textcontent of the element whose id is |destination_id| to |value|.
  void SetCryptohomeProperty(const std::string& destination_id,
                             const base::Value& value);

  base::WeakPtrFactory<CryptohomeWebUIHandler> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CryptohomeWebUIHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_WEB_UI_HANDLER_H_
