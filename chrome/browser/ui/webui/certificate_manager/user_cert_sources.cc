// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/user_cert_sources.h"

#include <vector>

#include "base/containers/to_vector.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/net/server_certificate_database.h"
#include "chrome/browser/net/server_certificate_database.pb.h"
#include "chrome/browser/net/server_certificate_database_service.h"
#include "chrome/browser/net/server_certificate_database_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/browser/ui/webui/certificate_viewer/certificate_viewer_webui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

void PopulateUserCertsAsync(
    CertificateManagerPageHandler::GetCertificatesCallback callback,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust,
    bool can_delete,
    std::vector<net::ServerCertificateDatabase::CertInformation>
        server_cert_infos) {
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> cert_infos;
  for (auto const& cert_info : server_cert_infos) {
    if (cert_info.cert_metadata.trust().trust_type() != trust) {
      continue;
    }
    x509_certificate_model::X509CertificateModel model(
        net::x509_util::CreateCryptoBuffer(cert_info.der_cert), "");
    cert_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
        cert_info.sha256hash_hex, model.GetTitle(), can_delete));
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
      if (base::FeatureList::IsEnabled(
              ::features::kEnableCertManagementUIV2EditCerts)) {
        ShowCertificateDialog(
            std::move(web_contents),
            net::x509_util::CreateCryptoBuffer(cert_info.der_cert),
            cert_info.cert_metadata);
      } else {
        ShowCertificateDialog(
            std::move(web_contents),
            net::x509_util::CreateCryptoBuffer(cert_info.der_cert));
      }
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

UserCertSource::~UserCertSource() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

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
  bool can_delete = IsCACertificateManagementAllowed(*profile_->GetPrefs());
  server_cert_service->GetAllCertificates(base::BindOnce(
      &PopulateUserCertsAsync, std::move(callback), trust_, can_delete));
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
    const std::string& display_name,
    const std::string& sha256hash_hex,
    CertificateManagerPageHandler::DeleteCertificateCallback callback) {
  PrefService* prefs = profile_->GetPrefs();
  // This error string does not need localization since it will not be shown on
  // UI. If the pref is not set, the UI to delete certificates will not be
  // shown.
  if (!IsCACertificateManagementAllowed(*prefs)) {
    std::move(callback).Run(
        certificate_manager_v2::mojom::ActionResult::NewError(
            "Deleting certificates is not allowed"));
    return;
  }
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

void UserCertSource::ImportCertificate(
    base::WeakPtr<content::WebContents> web_contents,
    CertificateManagerPageHandler::ImportCertificateCallback callback) {
  if (!web_contents || select_file_dialog_) {
    std::move(callback).Run(nullptr);
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  // This error string does not need localization since it will not be shown on
  // UI. If the pref is not set, the UI to delete certificates will not be
  // shown.
  if (!IsCACertificateManagementAllowed(*prefs)) {
    std::move(callback).Run(
        certificate_manager_v2::mojom::ActionResult::NewError(
            "Importing certificates is not allowed"));
    return;
  }

  import_callback_ = std::move(callback);
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents.get()));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {
      {FILE_PATH_LITERAL("der"), FILE_PATH_LITERAL("cer"),
       FILE_PATH_LITERAL("crt"), FILE_PATH_LITERAL("pem"),
       FILE_PATH_LITERAL("p7b"), FILE_PATH_LITERAL("p7c")}};
  file_type_info.include_all_files = true;
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
      base::FilePath(), &file_type_info, 1, FILE_PATH_LITERAL("der"),
      web_contents->GetTopLevelNativeWindow(), nullptr);
}

void UserCertSource::FileSelected(const ui::SelectedFileInfo& file, int index) {
  select_file_dialog_ = nullptr;
  // Use CONTINUE_ON_SHUTDOWN since this is only for reading a file, if it
  // doesn't complete before shutdown the file still exists, and even if the
  // browser blocked on completing this task, the import isn't actually done
  // yet, so just blocking shutdown on the file read wouldn't accomplish
  // anything. CONTINUE_ON_SHUTDOWN should be safe as base::ReadFileToBytes
  // doesn't access any global state.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&base::ReadFileToBytes, file.path()),
      base::BindOnce(&UserCertSource::FileRead,
                     weak_ptr_factory_.GetWeakPtr()));
}
void UserCertSource::FileSelectionCanceled() {
  select_file_dialog_ = nullptr;
  std::move(import_callback_).Run(nullptr);
}

void UserCertSource::FileRead(std::optional<std::vector<uint8_t>> file_bytes) {
  if (!file_bytes) {
    std::move(import_callback_)
        .Run(certificate_manager_v2::mojom::ActionResult::NewError(
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_CERTIFICATE_MANAGER_V2_READ_FILE_ERROR)));
    return;
  }
  net::CertificateList certs_to_import =
      net::X509Certificate::CreateCertificateListFromBytes(
          file_bytes.value(), net::X509Certificate::FORMAT_AUTO);

  if (certs_to_import.size() != 1) {
    if (certs_to_import.size() == 0) {
      std::move(import_callback_)
          .Run(certificate_manager_v2::mojom::ActionResult::NewError(
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_READ_FILE_ERROR)));
      return;
    }
    std::move(import_callback_)
        .Run(certificate_manager_v2::mojom::ActionResult::NewError(
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_CERTIFICATE_MANAGER_V2_MULTIPLE_CERT_ERROR)));
    return;
  }

  scoped_refptr<net::X509Certificate> cert_to_import = certs_to_import[0];

  net::ServerCertificateDatabaseService* server_cert_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile_);

  net::ServerCertificateDatabase::CertInformation cert_info;
  cert_info.sha256hash_hex = base::ToLowerASCII(
      base::HexEncode(net::X509Certificate::CalculateFingerprint256(
                          cert_to_import->cert_buffer())
                          .data));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(trust_);
  cert_info.der_cert = base::ToVector(cert_to_import->cert_span());

  server_cert_service->AddOrUpdateUserCertificate(
      std::move(cert_info),
      base::BindOnce(&UserCertSource::ImportCertificateResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UserCertSource::ImportCertificateResult(bool success) {
  if (success) {
    std::move(import_callback_)
        .Run(certificate_manager_v2::mojom::ActionResult::NewSuccess(
            certificate_manager_v2::mojom::SuccessResult::kSuccess));
    return;
  }
  std::move(import_callback_)
      .Run(certificate_manager_v2::mojom::ActionResult::NewError(
          l10n_util::GetStringUTF8(
              IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_ERROR_TITLE)));
}
