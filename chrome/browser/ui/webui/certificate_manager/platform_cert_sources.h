// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_PLATFORM_CERT_SOURCES_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_PLATFORM_CERT_SOURCES_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "content/public/browser/web_contents.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

class PlatformCertSource : public CertificateManagerPageHandler::CertSource {
 public:
  explicit PlatformCertSource(std::string export_file_name,
                              cert_verifier::mojom::CertificateTrust trust);

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback) override;

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override;

  void ExportCertificates(
      base::WeakPtr<content::WebContents> web_contents) override;

 private:
  std::string export_file_name_;
  cert_verifier::mojom::CertificateTrust trust_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_PLATFORM_CERT_SOURCES_H_
