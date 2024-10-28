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

class UserCertSource : public CertificateManagerPageHandler::CertSource {
 public:
  UserCertSource(
      std::string export_file_name,
      chrome_browser_server_certificate_database::CertificateTrust::
          CertificateTrustType trust,
      raw_ptr<Profile> profile,
      mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>*
          remote_client);

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback) override;

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override;

  void ExportCertificates(
      base::WeakPtr<content::WebContents> web_contents) override;

  void DeleteCertificate(
      const std::string& sha256hash_hex,
      const std::string& display_name,
      CertificateManagerPageHandler::DeleteCertificateCallback callback)
      override;

 private:
  std::string export_file_name_;
  chrome_browser_server_certificate_database::CertificateTrust::
      CertificateTrustType trust_;
  raw_ptr<Profile> profile_;
  raw_ptr<mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>>
      remote_client_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_USER_CERT_SOURCES_H_
