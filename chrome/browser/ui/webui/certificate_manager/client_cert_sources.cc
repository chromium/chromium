// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "crypto/crypto_buildflags.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/browser/ui/crypto_module_delegate_nss.h"
#include "net/ssl/client_cert_store_nss.h"
#endif  // BUILDFLAG(USE_NSS_CERTS)

#if BUILDFLAG(IS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "net/ssl/client_cert_store_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/client_certificates_service.h"
#include "components/enterprise/client_certificates/core/features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#endif

namespace {

// A certificate loader that wraps a ClientCertStore. Read-only.
// Lifetimes note: The callback will not be called if the ClientCertStoreLoader
// (and thus, the ClientCertStore) is destroyed first.
class ClientCertStoreLoader {
 public:
  explicit ClientCertStoreLoader(std::unique_ptr<net::ClientCertStore> store)
      : store_(std::move(store)) {}

  void GetCerts(base::OnceCallback<void(net::CertificateList)> callback) {
    store_->GetClientCerts(
        base::MakeRefCounted<net::SSLCertRequestInfo>(),
        base::BindOnce(&ClientCertStoreLoader::HandleClientCertsResult,
                       std::move(callback)));
  }

 private:
  static void HandleClientCertsResult(
      base::OnceCallback<void(net::CertificateList)> callback,
      net::ClientCertIdentityList identities) {
    net::CertificateList certs;
    certs.reserve(identities.size());
    for (const auto& identity : identities) {
      certs.push_back(identity->certificate());
    }
    std::move(callback).Run(std::move(certs));
  }

  std::unique_ptr<net::ClientCertStore> store_;
};

std::unique_ptr<ClientCertStoreLoader> CreatePlatformClientCertLoader() {
#if BUILDFLAG(USE_NSS_CERTS)
  return std::make_unique<ClientCertStoreLoader>(
      std::make_unique<net::ClientCertStoreNSS>(
          base::BindRepeating(&CreateCryptoModuleBlockingPasswordDelegate,
                              kCryptoModulePasswordClientAuth)));
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<ClientCertStoreLoader>(
      std::make_unique<net::ClientCertStoreWin>());
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<ClientCertStoreLoader>(
      std::make_unique<net::ClientCertStoreMac>());
#else
  return nullptr;
#endif
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// ClientCertStore implementation that always returns an empty list. The
// CertificateProvisioningService implementation expects to wrap a platform
// cert store, but here we only want to get results from the provisioning
// service itself, so instead of a platform cert store we pass an
// implementation that always returns an empty result when queried.
class NullClientCertStore : public net::ClientCertStore {
 public:
  ~NullClientCertStore() override = default;
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run({});
  }
};

std::unique_ptr<ClientCertStoreLoader> CreateProvisionedClientCertLoader(
    Profile* profile) {
  if (!profile || !client_certificates::features::
                      IsManagedClientCertificateForUserEnabled()) {
    return nullptr;
  }
  auto* provisioning_service =
      client_certificates::CertificateProvisioningServiceFactory::GetForProfile(
          profile);
  if (!provisioning_service) {
    return nullptr;
  }

  return std::make_unique<ClientCertStoreLoader>(
      client_certificates::ClientCertificatesService::Create(
          provisioning_service, std::make_unique<NullClientCertStore>()));
}
#endif

void PopulateCertInfosFromCertificateList(
    CertificateManagerPageHandler::GetCertificatesCallback callback,
    const net::CertificateList& certs) {
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> out_infos;
  for (const auto& cert : certs) {
    x509_certificate_model::X509CertificateModel model(
        bssl::UpRef(cert->cert_buffer()), "");
    out_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
        model.HashCertSHA256(), model.GetTitle()));
  }
  std::move(callback).Run(std::move(out_infos));
}

void ViewCertificateFromCertificateList(
    const std::string& sha256_hex_hash,
    const net::CertificateList& certs,
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }

  net::SHA256HashValue hash;
  if (!base::HexStringToSpan(sha256_hex_hash, hash.data)) {
    return;
  }

  for (const auto& cert : certs) {
    if (net::X509Certificate::CalculateFingerprint256(cert->cert_buffer()) ==
        hash) {
      ShowCertificateDialog(std::move(web_contents),
                            bssl::UpRef(cert->cert_buffer()));
      return;
    }
  }
}

class ClientCertSource : public CertificateManagerPageHandler::CertSource {
 public:
  explicit ClientCertSource(std::unique_ptr<ClientCertStoreLoader> loader)
      : loader_(std::move(loader)) {}
  ~ClientCertSource() override = default;

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback)
      override {
    if (!loader_) {
      std::move(callback).Run({});
      return;
    }
    if (certs_) {
      PopulateCertInfosFromCertificateList(std::move(callback), *certs_);
      return;
    }
    // Unretained is safe here as if `this` is destroyed, the ClientCertStore
    // will be destroyed, and the ClientCertStore contract is that the callback
    // will not be called after the ClientCertStore object is destroyed.
    loader_->GetCerts(base::BindOnce(&ClientCertSource::SaveCertsAndRespond,
                                     base::Unretained(this),
                                     std::move(callback)));
  }

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override {
    if (!loader_ || !certs_) {
      return;
    }
    ViewCertificateFromCertificateList(sha256_hex_hash, *certs_,
                                       std::move(web_contents));
  }

 private:
  void SaveCertsAndRespond(
      CertificateManagerPageHandler::GetCertificatesCallback callback,
      net::CertificateList certs) {
    certs_ = std::move(certs);
    PopulateCertInfosFromCertificateList(std::move(callback), *certs_);
  }

  std::unique_ptr<ClientCertStoreLoader> loader_;
  std::optional<net::CertificateList> certs_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Subclass of ClientCertSource that also allows importing client certificates
// to the ChromeOS client cert store.
class CrosClientCertSource : public ClientCertSource,
                             public ui::SelectFileDialog::Listener {
 public:
  explicit CrosClientCertSource(std::unique_ptr<ClientCertStoreLoader> loader)
      : ClientCertSource(std::move(loader)) {}

  ~CrosClientCertSource() override {
    if (select_file_dialog_) {
      select_file_dialog_->ListenerDestroyed();
    }
  }

  void ImportCertificate(
      base::WeakPtr<content::WebContents> web_contents,
      CertificateManagerPageHandler::ImportCertificateCallback callback)
      override {
    // Containing web contents went away (e.g. user navigated away) or dialog
    // is already open. Don't try to open the dialog.
    if (!web_contents || select_file_dialog_) {
      std::move(callback).Run(nullptr);
      return;
    }

    import_callback_ = std::move(callback);

    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(web_contents.get()));

    ui::SelectFileDialog::FileTypeInfo file_type_info;
    file_type_info.extensions = {{FILE_PATH_LITERAL("p12"),
                                  FILE_PATH_LITERAL("pfx"),
                                  FILE_PATH_LITERAL("crt")}};
    file_type_info.include_all_files = true;
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
        base::FilePath(), &file_type_info,
        1,  // 1-based index for |file_type_info.extensions| to specify default.
        FILE_PATH_LITERAL("p12"), web_contents->GetTopLevelNativeWindow(),
        /*params=*/nullptr);
  }

  // ui::SelectFileDialog::Listener
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    select_file_dialog_ = nullptr;

    // Use CONTINUE_ON_SHUTDOWN since this is only for reading a file, if it
    // doesn't complete before shutdown the file still exists, and even if the
    // browser blocked on completing this task, the import isn't actually
    // done yet, so just blocking shutdown on the file read wouldn't accomplish
    // anything. CONTINUE_ON_SHUTDOWN should be safe as base::ReadFileToBytes
    // doesn't access any global state.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&base::ReadFileToBytes, file.path()),
        base::BindOnce(&CrosClientCertSource::FileRead,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void FileSelectionCanceled() override {
    select_file_dialog_ = nullptr;

    std::move(import_callback_).Run(nullptr);
  }

  void FileRead(std::optional<std::vector<uint8_t>> file_bytes) {
    if (!file_bytes) {
      // TODO(crbug.com/40928765): localize
      std::move(import_callback_)
          .Run(certificate_manager_v2::mojom::ImportResult::NewError(
              "error reading file"));
      return;
    }

    // TODO(crbug.com/40928765): actually do the import
    std::move(import_callback_)
        .Run(certificate_manager_v2::mojom::ImportResult::NewError(
            "not implemented"));
  }

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  CertificateManagerPageHandler::ImportCertificateCallback import_callback_;
  base::WeakPtrFactory<CrosClientCertSource> weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
class ExtensionsClientCertSource
    : public CertificateManagerPageHandler::CertSource {
 public:
  explicit ExtensionsClientCertSource(
      std::unique_ptr<chromeos::CertificateProvider> provider)
      : provider_(std::move(provider)) {}
  ~ExtensionsClientCertSource() override = default;

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback)
      override {
    if (!provider_) {
      std::move(callback).Run({});
      return;
    }
    if (certs_) {
      PopulateCertInfosFromCertificateList(std::move(callback), *certs_);
      return;
    }

    provider_->GetCertificates(
        base::BindOnce(&ExtensionsClientCertSource::SaveCertsAndRespond,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override {
    if (!provider_ || !certs_) {
      return;
    }
    ViewCertificateFromCertificateList(sha256_hex_hash, *certs_,
                                       std::move(web_contents));
  }

 private:
  void SaveCertsAndRespond(
      CertificateManagerPageHandler::GetCertificatesCallback callback,
      net::ClientCertIdentityList cert_identities) {
    certs_ = net::CertificateList();
    certs_->reserve(cert_identities.size());
    for (const auto& identity : cert_identities) {
      certs_->push_back(identity->certificate());
    }
    PopulateCertInfosFromCertificateList(std::move(callback), *certs_);
  }

  std::unique_ptr<chromeos::CertificateProvider> provider_;
  std::optional<net::CertificateList> certs_;
  base::WeakPtrFactory<ExtensionsClientCertSource> weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreatePlatformClientCertSource() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<CrosClientCertSource>(
      CreatePlatformClientCertLoader());
#else
  return std::make_unique<ClientCertSource>(CreatePlatformClientCertLoader());
#endif
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreateProvisionedClientCertSource(Profile* profile) {
  return std::make_unique<ClientCertSource>(
      CreateProvisionedClientCertLoader(profile));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreateExtensionsClientCertSource(Profile* profile) {
  chromeos::CertificateProviderService* certificate_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          profile);
  return std::make_unique<ExtensionsClientCertSource>(
      certificate_provider_service->CreateCertificateProvider());
}
#endif
