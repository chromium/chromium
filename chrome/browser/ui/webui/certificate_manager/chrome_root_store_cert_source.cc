// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/chrome_root_store_cert_source.h"

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace {

void PopulateChromeRootStoreLogsAsync(
    CertificateManagerPageHandler::GetCertificatesCallback callback,
    cert_verifier::mojom::ChromeRootStoreInfoPtr info) {
  // TODO(crbug.com/40928765): store the info returned so we can use it in later
  // calls (e.g. the cert bytes will be needed when we view the details or
  // export the cert.
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> cert_infos;
  for (auto const& cert_info : info->root_cert_info) {
    x509_certificate_model::X509CertificateModel model(
        net::x509_util::CreateCryptoBuffer(cert_info->cert), "");
    cert_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
        cert_info->sha256hash_hex, model.GetTitle(),
        /*is_deletable=*/false));
  }
  std::move(callback).Run(std::move(cert_infos));
}

void ViewCrsCertificateAsync(
    std::string hash,
    base::WeakPtr<content::WebContents> web_contents,
    cert_verifier::mojom::ChromeRootStoreInfoPtr info) {
  // Containing web contents went away (e.g. user navigated away). Don't
  // try to open the dialog.
  if (!web_contents) {
    return;
  }

  for (auto const& cert_info : info->root_cert_info) {
    if (cert_info->sha256hash_hex == hash) {
      // Found the cert, open cert viewer dialog if able and then exit function.
      ShowCertificateDialog(
          std::move(web_contents),
          net::x509_util::CreateCryptoBuffer(cert_info->cert));
      return;
    }
  }
}

void ExportCrsCertificatesAsync(
    base::WeakPtr<content::WebContents> web_contents,
    cert_verifier::mojom::ChromeRootStoreInfoPtr info) {
  // Containing web contents went away (e.g. user navigated away). Don't
  // try to open the dialog.
  if (!web_contents) {
    return;
  }

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> export_certs;
  for (auto const& cert_info : info->root_cert_info) {
    export_certs.push_back(net::x509_util::CreateCryptoBuffer(cert_info->cert));
  }

  ShowCertExportDialogSaveAll(
      web_contents.get(), web_contents->GetTopLevelNativeWindow(),
      std::move(export_certs), "chrome_root_store_certs.pem");
  return;
}

}  // namespace

void ChromeRootStoreCertSource::GetCertificateInfos(
    CertificateManagerPageHandler::GetCertificatesCallback callback) {
  content::GetCertVerifierServiceFactory()->GetChromeRootStoreInfo(
      base::BindOnce(&PopulateChromeRootStoreLogsAsync, std::move(callback)));
}

void ChromeRootStoreCertSource::ViewCertificate(
    const std::string& sha256_hex_hash,
    base::WeakPtr<content::WebContents> web_contents) {
  // This should really use a cached set of info with other calls to
  // GetChromeRootStoreInfo.
  content::GetCertVerifierServiceFactory()->GetChromeRootStoreInfo(
      base::BindOnce(&ViewCrsCertificateAsync, sha256_hex_hash,
                     std::move(web_contents)));
}

void ChromeRootStoreCertSource::ExportCertificates(
    base::WeakPtr<content::WebContents> web_contents) {
  // This should really use a cached set of info with other calls to
  // GetChromeRootStoreInfo.
  content::GetCertVerifierServiceFactory()->GetChromeRootStoreInfo(
      base::BindOnce(&ExportCrsCertificatesAsync, web_contents));
}
