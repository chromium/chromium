// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CHROME_ROOT_STORE_CERT_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CHROME_ROOT_STORE_CERT_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "content/public/browser/web_contents.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

class ChromeRootStoreCertSource
    : public CertificateManagerPageHandler::CertSource {
 public:
  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback) override;

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override;

  void ExportCertificates(
      base::WeakPtr<content::WebContents> web_contents) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CHROME_ROOT_STORE_CERT_SOURCE_H_
