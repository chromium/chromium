// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/user_cert_sources.h"

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/server_certificate_database.h"
#include "chrome/browser/net/server_certificate_database.pb.h"
#include "chrome/browser/net/server_certificate_database_service.h"
#include "chrome/browser/net/server_certificate_database_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/browser/ui/webui/certificate_viewer/certificate_viewer_webui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "net/cert/x509_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void PopulateUserCertsAsync(
    CertificateManagerPageHandler::GetCertificatesCallback callback,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust,
    std::vector<net::ServerCertificateDatabase::CertInformation>
        server_cert_infos) {
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> cert_infos;
  for (auto const& cert_info : server_cert_infos) {
    if (cert_info.cert_metadata.trust().trust_type() != trust) {
      continue;
    }
    x509_certificate_model::X509CertificateModel model(
        net::x509_util::CreateCryptoBuffer(cert_info.der_cert), "");
    // TODO(crbug.com/40928765): is_deletable should be set to false if
    // CACertificateManagementAllowed is set to None.
    cert_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
        cert_info.sha256hash_hex, model.GetTitle(),
        /*is_deletable=*/true));
  }
  std::move(callback).Run(std::move(cert_infos));
}

void ViewCertificateAsync(
    std::string sha256_hex_hash,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust,
    base::WeakPtr<content::WebContents> web_contents,
    std::vector<net::ServerCertificateDatabase::CertInformation>
        server_cert_infos) {
  // Containing web contents went away (e.g. user navigated away). Don't
  // try to open the dialog.
  if (!web_contents) {
    return;
  }

  std::array<uint8_t, crypto::kSHA256Length> hash;
  if (!base::HexStringToSpan(sha256_hex_hash, hash)) {
    return;
  }

  for (auto const& cert_info : server_cert_infos) {
    if (cert_info.cert_metadata.trust().trust_type() != trust) {
      continue;
    }
    if (hash == crypto::SHA256Hash(cert_info.der_cert)) {
      // Found the cert, open cert viewer dialog if able and return.
      // TODO (crbug.com/40928765): Allow modifying constraints through the
      // certificate viewer.
      ShowCertificateDialog(
          std::move(web_contents),
          net::x509_util::CreateCryptoBuffer(cert_info.der_cert));
      return;
    }
  }
}

void ExportCertificatesAsync(
    base::WeakPtr<content::WebContents> web_contents,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust,
    std::string file_name,
    std::vector<net::ServerCertificateDatabase::CertInformation>
        server_cert_infos) {
  // Containing web contents went away (e.g. user navigated away). Don't
  // try to open the dialog.
  if (!web_contents) {
    return;
  }

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> export_certs;
  for (auto const& cert_info : server_cert_infos) {
    if (cert_info.cert_metadata.trust().trust_type() != trust) {
      continue;
    }
    export_certs.push_back(
        net::x509_util::CreateCryptoBuffer(cert_info.der_cert));
  }
  ShowCertExportDialogSaveAll(web_contents.get(),
                              web_contents->GetTopLevelNativeWindow(),
                              std::move(export_certs), file_name);
}

void DeleteCertificateResultAsync(
    CertificateManagerPageHandler::DeleteCertificateCallback callback,
    bool result) {
  if (result) {
    std::move(callback).Run(
        certificate_manager_v2::mojom::ActionResult::NewSuccess(
            certificate_manager_v2::mojom::SuccessResult::kSuccess));
    return;
  }
  std::move(callback).Run(certificate_manager_v2::mojom::ActionResult::NewError(
      "Error deleting certificate"));
}

void GotDeleteConfirmation(
    const std::string& sha256hash_hex,
    CertificateManagerPageHandler::DeleteCertificateCallback callback,
    base::WeakPtr<Profile> profile,
    bool confirmed) {
  if (confirmed && profile) {
    net::ServerCertificateDatabaseService* server_cert_service =
        net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
            profile.get());
    server_cert_service->DeleteCertificate(
        sha256hash_hex,
        base::BindOnce(&DeleteCertificateResultAsync, std::move(callback)));
    return;
  }
  std::move(callback).Run(nullptr);
}

}  // namespace

UserCertSource::UserCertSource(
    std::string export_file_name,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust,
    raw_ptr<Profile> profile,
    mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>*
        remote_client)
    : export_file_name_(std::move(export_file_name)),
      trust_(trust),
      profile_(profile),
      remote_client_(remote_client) {}

void UserCertSource::GetCertificateInfos(
    CertificateManagerPageHandler::GetCertificatesCallback callback) {
  net::ServerCertificateDatabaseService* server_cert_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile_);
  if (!base::FeatureList::IsEnabled(
          ::features::kEnableCertManagementUIV2Write)) {
    std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> cert_infos;
    std::move(callback).Run(std::move(cert_infos));
    return;
  }
  server_cert_service->GetAllCertificates(
      base::BindOnce(&PopulateUserCertsAsync, std::move(callback), trust_));
}

void UserCertSource::ViewCertificate(
    const std::string& sha256_hex_hash,
    base::WeakPtr<content::WebContents> web_contents) {
  net::ServerCertificateDatabaseService* server_cert_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile_);
  server_cert_service->GetAllCertificates(base::BindOnce(
      &ViewCertificateAsync, sha256_hex_hash, trust_, web_contents));
}

void UserCertSource::ExportCertificates(
    base::WeakPtr<content::WebContents> web_contents) {
  net::ServerCertificateDatabaseService* server_cert_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile_);
  server_cert_service->GetAllCertificates(base::BindOnce(
      &ExportCertificatesAsync, web_contents, trust_, export_file_name_));
}

void UserCertSource::DeleteCertificate(
    const std::string& sha256hash_hex,
    const std::string& display_name,
    CertificateManagerPageHandler::DeleteCertificateCallback callback) {
  // TODO(crbug.com/40928765): This should early return if
  // CACertificateManagementAllowed is set to None.
  (*remote_client_)
      ->AskForConfirmation(
          l10n_util::GetStringFUTF8(
              IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_CERT_TITLE,
              base::UTF8ToUTF16(display_name)),
          l10n_util::GetStringUTF8(
              IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_SERVER_CERT_DESCRIPTION),
          base::BindOnce(&GotDeleteConfirmation, sha256hash_hex,
                         std::move(callback), profile_->GetWeakPtr()));
}
