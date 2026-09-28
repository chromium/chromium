// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_ONC_IMPORT_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_ONC_IMPORT_MESSAGE_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "components/server_certificate_database/server_certificate_database.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {
class WebContents;
}

namespace net {
class NSSCertDatabase;
}

namespace ash {
namespace onc {
class CertificateImporterImpl;
}

class OncImportMessageHandler : public content::WebUIMessageHandler,
                                public ui::SelectFileDialog::Listener {
 public:
  using SelectFilePolicyCreator =
      base::RepeatingCallback<std::unique_ptr<ui::SelectFilePolicy>(
          content::WebContents*)>;

  explicit OncImportMessageHandler(
      SelectFilePolicyCreator select_file_policy_creator);
  ~OncImportMessageHandler() override;
  OncImportMessageHandler(const OncImportMessageHandler&) = delete;
  OncImportMessageHandler& operator=(const OncImportMessageHandler&) = delete;

 private:
  // WebUIMessageHandler
  void RegisterMessages() override;

  void Respond(const std::string& callback_id,
               const std::string& result,
               bool is_error);
  void OnImportONC(const base::ListValue& list);
  void ImportONCToNSSDB(const std::string& callback_id,
                        const std::string& onc_blob,
                        net::NSSCertDatabase* nssdb);

  void GetAllServerCertificates(
      std::unique_ptr<onc::CertificateImporterImpl> cert_importer,
      std::unique_ptr<chromeos::onc::OncParsedCertificates> onc_certs,
      const std::string& callback_id,
      const std::string& previous_result,
      bool has_error,
      bool cert_import_success);

  void ImportServerCertificates(
      std::unique_ptr<chromeos::onc::OncParsedCertificates> onc_certs,
      const std::string& callback_id,
      const std::string& previous_result,
      bool has_error,
      std::vector<net::ServerCertificateDatabase::CertInformation>
          current_certs);

  void OnAllCertificatesImportedUserInitiated(
      std::unique_ptr<onc::CertificateImporterImpl> cert_importer,
      const std::string& callback_id,
      const std::string& previous_error,
      bool has_error,
      bool cert_import_success);

  void OnServerCertsImportedDb(const std::string& callback_id,
                               const std::string& previous_error,
                               bool has_error,
                               bool cert_import_success);

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  void OnFileReadCompleted(const std::string& callback_id,
                           const std::string& onc_blob);

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::string current_callback_id_;

  SelectFilePolicyCreator select_file_policy_creator_;

  base::WeakPtrFactory<OncImportMessageHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_ONC_IMPORT_MESSAGE_HANDLER_H_
