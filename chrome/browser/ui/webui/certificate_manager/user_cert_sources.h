// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_USER_CERT_SOURCES_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_USER_CERT_SOURCES_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/net/server_certificate_database.pb.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "content/public/browser/web_contents.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class UserCertSource : public CertificateManagerPageHandler::CertSource,
                       public ui::SelectFileDialog::Listener {
 public:
  UserCertSource(
      std::string export_file_name,
      chrome_browser_server_certificate_database::CertificateTrust::
          CertificateTrustType trust,
      raw_ptr<Profile> profile,
      mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>*
          remote_client);
  ~UserCertSource() override;

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback) override;

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override;

  void ExportCertificates(
      base::WeakPtr<content::WebContents> web_contents) override;

  void DeleteCertificate(
      const std::string& display_name,
      const std::string& sha256hash_hex,
      CertificateManagerPageHandler::DeleteCertificateCallback callback)
      override;

  void ImportCertificate(
      base::WeakPtr<content::WebContents> web_contents,
      CertificateManagerPageHandler::ImportCertificateCallback callback)
      override;

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  void FileRead(std::optional<std::vector<uint8_t>> file_bytes);
  void ImportCertificateResult(bool success);

  std::string export_file_name_;
  chrome_browser_server_certificate_database::CertificateTrust::
      CertificateTrustType trust_;
  raw_ptr<Profile> profile_;
  raw_ptr<mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>>
      remote_client_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  CertificateManagerPageHandler::ImportCertificateCallback import_callback_;
  base::WeakPtrFactory<UserCertSource> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_USER_CERT_SOURCES_H_
