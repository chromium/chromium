// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/platform_cert_sources.h"

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "net/cert/x509_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace {

void PopulatePlatformRootStoreLogsAsync(
    CertificateManagerPageHandler::GetCertificatesCallback callback,
    cert_verifier::mojom::CertificateTrust trust,
    cert_verifier::mojom::PlatformRootStoreInfoPtr info) {
  // TODO(crbug.com/40928765): store the info returned so we can use it in later
  // calls (e.g. the cert bytes will be needed when we view the details or
  // export the cert.
  //
  // Even more ideally, store this information somewhere such that multiple
  // PlatformCertSource objects can use the same cached results.
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> cert_infos;
  for (auto const& cert_info : info->user_added_certs) {
    if (trust != cert_info->trust_setting) {
      continue;
    }
    x509_certificate_model::X509CertificateModel model(
        net::x509_util::CreateCryptoBuffer(cert_info->cert), "");
    cert_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
        model.HashCertSHA256(), model.GetTitle(),
        /*is_deletable=*/false));
  }
  std::move(callback).Run(std::move(cert_infos));
}

void ViewCertificateAsync(std::string sha256_hex_hash,
                          cert_verifier::mojom::CertificateTrust trust,
                          base::WeakPtr<content::WebContents> web_contents,
                          cert_verifier::mojom::PlatformRootStoreInfoPtr info) {
  // Containing web contents went away (e.g. user navigated away). Don't
  // try to open the dialog.
  if (!web_contents) {
    return;
  }

  std::array<uint8_t, crypto::kSHA256Length> hash;
  if (!base::HexStringToSpan(sha256_hex_hash, hash)) {
    return;
  }

  for (auto const& cert_info : info->user_added_certs) {
    if (trust != cert_info->trust_setting) {
      continue;
    }
    if (hash == crypto::SHA256Hash(cert_info->cert)) {
      // Found the cert, open cert viewer dialog if able and then exit function.
      ShowCertificateDialog(
          std::move(web_contents),
          net::x509_util::CreateCryptoBuffer(cert_info->cert));
      return;
    }
  }
}

void ExportCertificatesAsync(
    cert_verifier::mojom::CertificateTrust trust,
    std::string file_name,
    base::WeakPtr<content::WebContents> web_contents,
    cert_verifier::mojom::PlatformRootStoreInfoPtr info) {
  // Containing web contents went away (e.g. user navigated away). Don't
  // try to open the dialog.
  if (!web_contents) {
    return;
  }

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> export_certs;
  for (auto const& cert_info : info->user_added_certs) {
    if (trust != cert_info->trust_setting) {
      continue;
    }
    export_certs.push_back(net::x509_util::CreateCryptoBuffer(cert_info->cert));
  }

  ShowCertExportDialogSaveAll(web_contents.get(),
                              web_contents->GetTopLevelNativeWindow(),
                              std::move(export_certs), file_name);
  return;
}

}  // namespace

PlatformCertSource::PlatformCertSource(
    std::string export_file_name,
    cert_verifier::mojom::CertificateTrust trust)
    : export_file_name_(std::move(export_file_name)), trust_(trust) {}

void PlatformCertSource::GetCertificateInfos(
    CertificateManagerPageHandler::GetCertificatesCallback callback) {
  content::GetCertVerifierServiceFactory()->GetPlatformRootStoreInfo(
      base::BindOnce(&PopulatePlatformRootStoreLogsAsync, std::move(callback),
                     trust_));
}

void PlatformCertSource::ViewCertificate(
    const std::string& sha256_hex_hash,
    base::WeakPtr<content::WebContents> web_contents) {
  content::GetCertVerifierServiceFactory()->GetPlatformRootStoreInfo(
      base::BindOnce(&ViewCertificateAsync, sha256_hex_hash, trust_,
                     std::move(web_contents)));
}

void PlatformCertSource::ExportCertificates(
    base::WeakPtr<content::WebContents> web_contents) {
  content::GetCertVerifierServiceFactory()->GetPlatformRootStoreInfo(
      base::BindOnce(&ExportCertificatesAsync, trust_, export_file_name_,
                     web_contents));
}
