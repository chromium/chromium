// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_ONC_IMPORT_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_ONC_IMPORT_MESSAGE_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace net {
class NSSCertDatabase;
}

namespace ash {
namespace onc {
class CertificateImporterImpl;
}

class OncImportMessageHandler : public content::WebUIMessageHandler {
 public:
  OncImportMessageHandler();
  ~OncImportMessageHandler() override;
  OncImportMessageHandler(const OncImportMessageHandler&) = delete;
  OncImportMessageHandler& operator=(const OncImportMessageHandler&) = delete;

 private:
  // WebUIMessageHandler
  void RegisterMessages() override;

  void Respond(const std::string& callback_id,
               const std::string& result,
               bool is_error);
  void OnImportONC(const base::Value::List& list);
  void ImportONCToNSSDB(const std::string& callback_id,
                        const std::string& onc_blob,
                        net::NSSCertDatabase* nssdb);
  void OnCertificatesImported(
      std::unique_ptr<onc::CertificateImporterImpl> cert_importer,
      const std::string& callback_id,
      const std::string& previous_error,
      bool has_error,
      bool cert_import_success);

  base::WeakPtrFactory<OncImportMessageHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_ONC_IMPORT_MESSAGE_HANDLER_H_
