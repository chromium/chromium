// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_HANDLER_H_

#include <array>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

class Profile;
namespace content {
class WebContents;
}  // namespace content

// Mojo handler for the Certificate Manager v2 page.
class CertificateManagerPageHandler
    : public certificate_manager_v2::mojom::CertificateManagerPageHandler {
 public:
  class CertSource {
   public:
    virtual ~CertSource();
    virtual void GetCertificateInfos(
        CertificateManagerPageHandler::GetCertificatesCallback callback) = 0;
    virtual void ViewCertificate(
        const std::string& sha256_hex_hash,
        base::WeakPtr<content::WebContents> web_contents) = 0;
    virtual void ImportCertificate(
        base::WeakPtr<content::WebContents> web_contents,
        CertificateManagerPageHandler::ImportCertificateCallback callback);
    virtual void ImportAndBindCertificate(
        base::WeakPtr<content::WebContents> web_contents,
        CertificateManagerPageHandler::ImportCertificateCallback callback);
    virtual void DeleteCertificate(
        const std::string& sha256hash_hex,
        CertificateManagerPageHandler::DeleteCertificateCallback callback);
    virtual void ExportCertificates(
        base::WeakPtr<content::WebContents> web_contents) {}
  };

  explicit CertificateManagerPageHandler(
      mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
          pending_client,
      mojo::PendingReceiver<
          certificate_manager_v2::mojom::CertificateManagerPageHandler>
          pending_handler,
      Profile* profile,
      content::WebContents* web_contents);

  CertificateManagerPageHandler(const CertificateManagerPageHandler&) = delete;
  CertificateManagerPageHandler& operator=(
      const CertificateManagerPageHandler&) = delete;

  ~CertificateManagerPageHandler() override;

  void GetCertificates(
      certificate_manager_v2::mojom::CertificateSource source_id,
      GetCertificatesCallback callback) override;
  void ViewCertificate(
      certificate_manager_v2::mojom::CertificateSource source_id,
      const std::string& sha256hash_hex) override;
  void ImportCertificate(
      certificate_manager_v2::mojom::CertificateSource source_id,
      ImportCertificateCallback callback) override;
  void ImportAndBindCertificate(
      certificate_manager_v2::mojom::CertificateSource source_id,
      ImportCertificateCallback callback) override;
  void DeleteCertificate(
      certificate_manager_v2::mojom::CertificateSource source_id,
      const std::string& sha256hash_hex,
      DeleteCertificateCallback callback) override;
  void ExportCertificates(
      certificate_manager_v2::mojom::CertificateSource source_id) override;

  void GetCertManagementMetadata(
      GetCertManagementMetadataCallback callback) override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  void ShowNativeManageCertificates() override;
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  void SetIncludeSystemTrustStore(bool include) override;
#endif

 private:
  // Returns a reference to the CertSource object corresponding to `source`.
  // Will always return a valid object, or will fail with a CHECK if `source`
  // is invalid. If `source` is an optional cert source type that is not
  // enabled on the current runtime, it may return a dummy CertSource that
  // always returns an empty list of certificates.
  CertSource& GetCertSource(
      certificate_manager_v2::mojom::CertificateSource source);

  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      remote_client_;
  mojo::Receiver<certificate_manager_v2::mojom::CertificateManagerPageHandler>
      handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  std::array<
      std::unique_ptr<CertSource>,
      1 + static_cast<unsigned>(
              certificate_manager_v2::mojom::CertificateSource::kMaxValue)>
      cert_source_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_HANDLER_H_
