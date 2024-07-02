// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_ENTERPRISE_CERT_SOURCES_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_ENTERPRISE_CERT_SOURCES_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "content/public/browser/web_contents.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

class EnterpriseCertSource : public CertificateManagerPageHandler::CertSource {
 public:
  explicit EnterpriseCertSource(std::string export_file_name);

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback) override;

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override;

  void ExportCertificates(
      base::WeakPtr<content::WebContents> web_contents) override;

 protected:
  virtual std::vector<std::vector<uint8_t>> GetCerts() = 0;

 private:
  std::string export_file_name_;
};

class EnterpriseTrustedCertSource : public EnterpriseCertSource {
 public:
  explicit EnterpriseTrustedCertSource(Profile* profile);

  // Can't use the parent class's implementation because certs with additional
  // constraints need to be handled differently.
  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override;

 protected:
  std::vector<std::vector<uint8_t>> GetCerts() override;

 private:
  raw_ptr<Profile> profile_;
};

class EnterpriseIntermediateCertSource : public EnterpriseCertSource {
 public:
  explicit EnterpriseIntermediateCertSource(Profile* profile);

 protected:
  std::vector<std::vector<uint8_t>> GetCerts() override;

 private:
  raw_ptr<Profile> profile_;
};

class EnterpriseDistrustedCertSource : public EnterpriseCertSource {
 public:
  explicit EnterpriseDistrustedCertSource(Profile* profile);

 protected:
  std::vector<std::vector<uint8_t>> GetCerts() override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_ENTERPRISE_CERT_SOURCES_H_
