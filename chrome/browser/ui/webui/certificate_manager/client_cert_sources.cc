// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"

#include <map>
#include <optional>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/crypto_buildflags.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom-shared.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/browser/ui/crypto_module_delegate_nss.h"
#include "net/cert/x509_util_nss.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_histograms.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/net/client_cert_store_ash.h"
#include "chrome/browser/ash/net/client_cert_store_kcer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/cert/nss_cert_database.h"
#endif

namespace {

class ClientCertStoreFactory {
 public:
  virtual ~ClientCertStoreFactory() = default;
  virtual std::unique_ptr<net::ClientCertStore> CreateClientCertStore() = 0;
};

// A certificate loader that wraps a ClientCertStoreFactory. Read-only.
class ClientCertStoreLoader {
 public:
  explicit ClientCertStoreLoader(
      std::unique_ptr<ClientCertStoreFactory> factory)
      : factory_(std::move(factory)) {}

  // Lifetimes note: The callback will not be called if the
  // ClientCertStoreLoader (and thus, the ClientCertStore handle held by
  // `active_requests_`) is destroyed first.
  void GetCerts(base::OnceCallback<void(net::CertificateList)> callback) {
    std::unique_ptr<net::ClientCertStore> store =
        factory_->CreateClientCertStore();
    net::ClientCertStore* store_ptr = store.get();
    active_requests_[store_ptr] = std::move(store);
    // Unretained is safe as the callback is not run if `active_requests_` is
    // destroyed.
    store_ptr->GetClientCerts(
        base::MakeRefCounted<net::SSLCertRequestInfo>(),
        base::BindOnce(&ClientCertStoreLoader::HandleClientCertsResult,
                       base::Unretained(this), store_ptr, std::move(callback)));
  }

 private:
  void HandleClientCertsResult(
      net::ClientCertStore* store,
      base::OnceCallback<void(net::CertificateList)> callback,
      net::ClientCertIdentityList identities) {
    net::CertificateList certs;
    certs.reserve(identities.size());
    for (const auto& identity : identities) {
      certs.push_back(identity->certificate());
    }
    active_requests_.erase(store);
    std::move(callback).Run(std::move(certs));
  }

  std::unique_ptr<ClientCertStoreFactory> factory_;
  std::map<net::ClientCertStore*, std::unique_ptr<net::ClientCertStore>>
      active_requests_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ClientCertStoreFactoryAsh : public ClientCertStoreFactory {
 public:
  explicit ClientCertStoreFactoryAsh(Profile* profile) : profile_(profile) {}

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    CHECK(!ash::features::ShouldUseKcerClientCertStore());

    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile_);
    // Use the device-wide system key slot only if the user is affiliated on
    // the device.
    const bool use_system_key_slot = user->IsAffiliated();
    return std::make_unique<ash::ClientCertStoreAsh>(
        nullptr,  // no additional provider
        use_system_key_slot, user->username_hash(),
        ash::ClientCertStoreAsh::PasswordDelegateFactory());
  }

 private:
  raw_ptr<Profile> profile_;
};
#elif BUILDFLAG(USE_NSS_CERTS)
class ClientCertStoreFactoryNSS : public ClientCertStoreFactory {
 public:
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return std::make_unique<net::ClientCertStoreNSS>(
        base::BindRepeating(&CreateCryptoModuleBlockingPasswordDelegate,
                            kCryptoModulePasswordClientAuth));
  }
};
#elif BUILDFLAG(IS_WIN)
class ClientCertStoreFactoryWin : public ClientCertStoreFactory {
 public:
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return std::make_unique<net::ClientCertStoreWin>();
  }
};
#elif BUILDFLAG(IS_MAC)
class ClientCertStoreFactoryMac : public ClientCertStoreFactory {
 public:
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return std::make_unique<net::ClientCertStoreMac>();
  }
};
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<ClientCertStoreLoader> CreatePlatformClientCertLoader(
    Profile* profile) {
#if BUILDFLAG(USE_NSS_CERTS)
  return std::make_unique<ClientCertStoreLoader>(
      std::make_unique<ClientCertStoreFactoryNSS>());
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<ClientCertStoreLoader>(
      std::make_unique<ClientCertStoreFactoryWin>());
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<ClientCertStoreLoader>(
      std::make_unique<ClientCertStoreFactoryMac>());
#else
  return nullptr;
#endif
}
#endif

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

class ClientCertStoreFactoryProvisioned : public ClientCertStoreFactory {
 public:
  explicit ClientCertStoreFactoryProvisioned(
      client_certificates::CertificateProvisioningService* provisioning_service)
      : provisioning_service_(provisioning_service) {}

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return client_certificates::ClientCertificatesService::Create(
        provisioning_service_, std::make_unique<NullClientCertStore>());
  }

 private:
  raw_ptr<client_certificates::CertificateProvisioningService>
      provisioning_service_;
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
      std::make_unique<ClientCertStoreFactoryProvisioned>(
          provisioning_service));
}
#endif

void PopulateCertInfosFromCertificateList(
    CertificateManagerPageHandler::GetCertificatesCallback callback,
    const net::CertificateList& certs,
    bool is_deletable) {
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> out_infos;
  for (const auto& cert : certs) {
    x509_certificate_model::X509CertificateModel model(
        bssl::UpRef(cert->cert_buffer()), "");
    out_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
        model.HashCertSHA256(), model.GetTitle(), is_deletable));
  }
  std::move(callback).Run(std::move(out_infos));
}

net::X509Certificate* FindCertificateFromCertificateList(
    std::string_view sha256_hex_hash,
    const net::CertificateList& certs) {
  net::SHA256HashValue hash;
  if (!base::HexStringToSpan(sha256_hex_hash, hash.data)) {
    return nullptr;
  }

  for (const auto& cert : certs) {
    if (net::X509Certificate::CalculateFingerprint256(cert->cert_buffer()) ==
        hash) {
      return cert.get();
    }
  }

  return nullptr;
}

void ViewCertificateFromCertificateList(
    const std::string& sha256_hex_hash,
    const net::CertificateList& certs,
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }

  net::X509Certificate* cert =
      FindCertificateFromCertificateList(sha256_hex_hash, certs);
  if (cert) {
    ShowCertificateDialog(std::move(web_contents),
                          bssl::UpRef(cert->cert_buffer()));
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
    if (certs_) {
      ReplyToGetCertificatesCallback(std::move(callback));
      return;
    }
    RefreshCachedCertificateList(
        base::BindOnce(&ClientCertSource::ReplyToGetCertificatesCallback,
                       base::Unretained(this), std::move(callback)));
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
  // Refresh list of cached certificates and run `callback` when done.
  void RefreshCachedCertificateList(base::OnceClosure callback) {
    if (!loader_) {
      std::move(callback).Run();
      return;
    }
    // Unretained is safe here as if `this` is destroyed, the ClientCertStore
    // will be destroyed, and the ClientCertStore contract is that the callback
    // will not be called after the ClientCertStore object is destroyed.
    loader_->GetCerts(base::BindOnce(&ClientCertSource::SaveCertsAndRespond,
                                     base::Unretained(this),
                                     std::move(callback)));
  }

  void ReplyToGetCertificatesCallback(
      CertificateManagerPageHandler::GetCertificatesCallback callback) const {
    PopulateCertInfosFromCertificateList(std::move(callback), *certs_,
                                         /*is_deletable=*/false);
  }

  void SaveCertsAndRespond(base::OnceClosure callback,
                           net::CertificateList certs) {
    certs_ = std::move(certs);
    std::move(callback).Run();
  }

  std::unique_ptr<ClientCertStoreLoader> loader_;
  std::optional<net::CertificateList> certs_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// ChromeOS currently can use either Kcer or NSS for listing client certs. This
// interface provides an abstraction to hide that from CrosClientCertSource.
// Once NSS client cert support is removed, this could just be merged into
// CrosClientCertSource.
class CrosCertLoader : public CertificateManagerPageHandler::CertSource {
 public:
  virtual void RefreshCachedCertificateList(base::OnceClosure callback) = 0;

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback)
      override {
    if (certs_) {
      ReplyToGetCertificatesCallback(std::move(callback));
      return;
    }
    RefreshCachedCertificateList(
        base::BindOnce(&CrosCertLoader::ReplyToGetCertificatesCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override {
    if (!certs_) {
      return;
    }

    net::X509Certificate* cert = FindCertificate(sha256_hex_hash);
    if (cert) {
      ShowCertificateDialog(std::move(web_contents),
                            bssl::UpRef(cert->cert_buffer()));
    }
  }

  net::X509Certificate* FindCertificate(
      std::string_view sha256_hex_hash) const {
    if (!certs_) {
      return nullptr;
    }

    net::SHA256HashValue hash;
    if (!base::HexStringToSpan(sha256_hex_hash, hash.data)) {
      return nullptr;
    }

    for (const auto& info : *certs_) {
      if (net::X509Certificate::CalculateFingerprint256(
              info.cert->cert_buffer()) == hash) {
        return info.cert.get();
      }
    }

    return nullptr;
  }

 protected:
  struct CertInfo {
    scoped_refptr<net::X509Certificate> cert;
    bool is_deletable;
  };

  void ReplyToGetCertificatesCallback(
      CertificateManagerPageHandler::GetCertificatesCallback callback) const {
    std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> out_infos;
    for (const auto& info : *certs_) {
      x509_certificate_model::X509CertificateModel model(
          bssl::UpRef(info.cert->cert_buffer()), "");
      out_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
          model.HashCertSHA256(), model.GetTitle(), info.is_deletable));
    }
    std::move(callback).Run(std::move(out_infos));
  }

  std::optional<std::vector<CertInfo>> certs_;

 private:
  base::WeakPtrFactory<CrosCertLoader> weak_ptr_factory_{this};
};

class CrosKcerLoader : public CrosCertLoader {
 public:
  explicit CrosKcerLoader(Profile* profile)
      : profile_(profile), kcer_(kcer::KcerFactoryAsh::GetKcer(profile)) {}
  ~CrosKcerLoader() override = default;

  void RefreshCachedCertificateList(base::OnceClosure callback) override {
    if (!kcer_) {
      std::move(callback).Run();
      return;
    }

    kcer_->GetAvailableTokens(base::BindOnce(&CrosKcerLoader::GotKcerTokens,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::move(callback)));
  }

 private:
  void GotKcerTokens(base::OnceClosure callback,
                     base::flat_set<kcer::Token> tokens) {
    if (!kcer_) {
      std::move(callback).Run();
      return;
    }

    kcer_->ListCerts(
        std::move(tokens),
        base::BindOnce(&CrosKcerLoader::GotKcerCerts,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GotKcerCerts(base::OnceClosure callback,
                    std::vector<scoped_refptr<const kcer::Cert>> kcer_certs,
                    base::flat_map<kcer::Token, kcer::Error> kcer_errors) {
    ClientCertManagementAccessControls policy(profile_);
    certs_ = std::vector<CertInfo>();
    certs_->reserve(kcer_certs.size());
    for (scoped_refptr<const kcer::Cert>& cert : kcer_certs) {
      if (!cert || !cert->GetX509Cert()) {
        // Probably shouldn't happen, but double check just in case.
        continue;
      }

      // TODO(crbug.com/40928765): This should be checking each cert for if it
      // is software or hardware backed, however that information isn't in
      // kcer::Cert and requires doing an async GetKeyInfo call for each cert.
      // The only time the difference matters is in guest mode where deleting
      // hardware backed certs isn't allowed, however guest mode doesn't let
      // you import hardware backed certs in the first place. In any case the
      // correct behavior is still enforced if such a cert somehow existed and
      // the user tried to delete it. So while this is theoretically incorrect
      // it's probably not worth bothering to fix.
      bool is_deletable = policy.IsChangeAllowed(
          ClientCertManagementAccessControls::kSoftwareBacked,
          cert->GetToken() == kcer::Token::kDevice
              ? ClientCertManagementAccessControls::kDeviceWide
              : ClientCertManagementAccessControls::kUser);
      certs_->emplace_back(cert->GetX509Cert(), is_deletable);
    }

    std::move(callback).Run();
  }

  raw_ptr<Profile> profile_;
  base::WeakPtr<kcer::Kcer> kcer_;
  base::WeakPtrFactory<CrosKcerLoader> weak_ptr_factory_{this};
};

class CrosNSSLoader : public CrosCertLoader {
 public:
  explicit CrosNSSLoader(Profile* profile)
      : profile_(profile),
        loader_(std::make_unique<ClientCertStoreLoader>(
            std::make_unique<ClientCertStoreFactoryAsh>(profile))) {}
  ~CrosNSSLoader() override = default;

  void RefreshCachedCertificateList(base::OnceClosure callback) override {
    if (!loader_) {
      std::move(callback).Run();
      return;
    }

    loader_->GetCerts(base::BindOnce(&CrosNSSLoader::SaveCertsAndRespond,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
  }

 private:
  void SaveCertsAndRespond(base::OnceClosure callback,
                           net::CertificateList certs) {
    ClientCertManagementAccessControls policy(profile_);
    // TODO(crbug.com/40928765): This should actually be set by checking
    // ClientCertManagementAccessControls.IsChangeAllowed on a per-cert basis.
    // However listing certs using kcer is already the default so it's
    // questionable whether spending the effort to implement it correctly for
    // the NSS implementation is worth doing. In any case the correct behavior
    // is still enforced if the user tried to delete a cert where it mattered.
    const bool is_deletable = policy.IsManagementAllowed(
        ClientCertManagementAccessControls::kSoftwareBacked);
    certs_ = std::vector<CertInfo>();
    certs_->reserve(certs.size());
    for (scoped_refptr<net::X509Certificate>& cert : certs) {
      certs_->emplace_back(cert, is_deletable);
    }

    std::move(callback).Run();
  }

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<ClientCertStoreLoader> loader_;
  base::WeakPtrFactory<CrosNSSLoader> weak_ptr_factory_{this};
};

// Subclass of ClientCertSource that also allows importing client certificates
// to the ChromeOS client cert store.
class CrosClientCertSource : public CertificateManagerPageHandler::CertSource,
                             public ui::SelectFileDialog::Listener {
 public:
  explicit CrosClientCertSource(
      mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>*
          remote_client,
      Profile* profile)
      : remote_client_(remote_client), profile_(profile) {
    if (ash::features::ShouldUseKcerClientCertStore()) {
      cros_cert_loader_ = std::make_unique<CrosKcerLoader>(profile);
    } else {
      cros_cert_loader_ = std::make_unique<CrosNSSLoader>(profile);
    }
  }

  ~CrosClientCertSource() override {
    if (select_file_dialog_) {
      select_file_dialog_->ListenerDestroyed();
    }
  }

  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback)
      override {
    cros_cert_loader_->GetCertificateInfos(std::move(callback));
  }

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override {
    cros_cert_loader_->ViewCertificate(sha256_hex_hash,
                                       std::move(web_contents));
  }

  void ImportCertificate(
      base::WeakPtr<content::WebContents> web_contents,
      CertificateManagerPageHandler::ImportCertificateCallback callback)
      override {
    BeginImportCertificate(/*hardware_backed=*/false, std::move(web_contents),
                           std::move(callback));
  }

  void ImportAndBindCertificate(
      base::WeakPtr<content::WebContents> web_contents,
      CertificateManagerPageHandler::ImportCertificateCallback callback)
      override {
    BeginImportCertificate(/*hardware_backed=*/true, std::move(web_contents),
                           std::move(callback));
  }

  void DeleteCertificate(
      const std::string& sha256hash_hex,
      CertificateManagerPageHandler::DeleteCertificateCallback callback)
      override {
    scoped_refptr<net::X509Certificate> cert =
        cros_cert_loader_->FindCertificate(sha256hash_hex);
    if (!cert) {
      // This error is not expected to be displayed under normal circumstances,
      // so it's not localized.
      std::move(callback).Run(
          certificate_manager_v2::mojom::ActionResult::NewError(
              "cert not found"));
      return;
    }

    std::u16string cert_title =
        base::UTF8ToUTF16(x509_certificate_model::X509CertificateModel(
                              bssl::UpRef(cert->cert_buffer()), "")
                              .GetTitle());

    (*remote_client_)
        ->AskForConfirmation(
            l10n_util::GetStringFUTF8(
                IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_CERT_TITLE,
                cert_title),
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_CLIENT_CERT_DESCRIPTION),
            base::BindOnce(
                &CrosClientCertSource::GotDeleteCertificateConfirmation,
                weak_ptr_factory_.GetWeakPtr(), sha256hash_hex,
                std::move(callback)));
  }

  void BeginImportCertificate(
      bool hardware_backed,
      base::WeakPtr<content::WebContents> web_contents,
      CertificateManagerPageHandler::ImportCertificateCallback callback) {
    // Containing web contents went away (e.g. user navigated away) or dialog
    // is already open. Don't try to open the dialog.
    if (!web_contents || select_file_dialog_) {
      std::move(callback).Run(nullptr);
      return;
    }

    if (!ClientCertManagementAccessControls(profile_).IsManagementAllowed(
            hardware_backed
                ? ClientCertManagementAccessControls::kHardwareBacked
                : ClientCertManagementAccessControls::kSoftwareBacked)) {
      // This error is not expected to be displayed under normal circumstances,
      // so it's not localized.
      std::move(callback).Run(
          certificate_manager_v2::mojom::ActionResult::NewError("not allowed"));
      return;
    }

    import_hardware_backed_ = hardware_backed;
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
    // browser blocked on completing this task, the import isn't actually done
    // yet, so just blocking shutdown on the file read wouldn't accomplish
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

 private:
  void FileRead(std::optional<std::vector<uint8_t>> file_bytes) {
    if (!file_bytes) {
      std::move(import_callback_)
          .Run(certificate_manager_v2::mojom::ActionResult::NewError(
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_READ_FILE_ERROR)));
      return;
    }

    (*remote_client_)
        ->AskForImportPassword(base::BindOnce(
            &CrosClientCertSource::GotImportPassword,
            weak_ptr_factory_.GetWeakPtr(), std::move(*file_bytes)));
  }

  void GotImportPassword(std::vector<uint8_t> file_bytes,
                         const std::optional<std::string>& password) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!password) {
      std::move(import_callback_).Run(nullptr);
      return;
    }

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CrosClientCertSource::GetCertDBOnIOThread,
            NssServiceFactory::GetForContext(profile_)
                ->CreateNSSCertDatabaseGetterForIOThread(),
            base::BindOnce(
                &CrosClientCertSource::GotNSSCertDatabaseForImportOnIOThread,
                import_hardware_backed_, std::move(file_bytes), *password,
                base::BindOnce(&CrosClientCertSource::FinishedNSSImport,
                               weak_ptr_factory_.GetWeakPtr()))));
  }

  static void GetCertDBOnIOThread(
      NssCertDatabaseGetter database_getter,
      base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    auto split_callback = base::SplitOnceCallback(std::move(callback));

    net::NSSCertDatabase* cert_db =
        std::move(database_getter).Run(std::move(split_callback.first));
    // If the NSS database was already available, |cert_db| is non-null and
    // |did_get_cert_db_callback| has not been called. Call it explicitly.
    if (cert_db) {
      std::move(split_callback.second).Run(cert_db);
    }
  }

  static void GotNSSCertDatabaseForImportOnIOThread(
      bool use_hardware_backed,
      std::vector<uint8_t> file_bytes,
      std::string password,
      base::OnceCallback<void(std::vector<uint8_t> file_bytes,
                              std::string password,
                              int nss_import_result)> finished_import_callback,
      net::NSSCertDatabase* cert_db) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    crypto::ScopedPK11Slot slot;
    if (use_hardware_backed) {
      slot = cert_db->GetPrivateSlot();
    } else {
      slot = cert_db->GetPublicSlot();
    }
    bool is_extractable = !use_hardware_backed;
    // TODO(crbug.com/40928765): Should do the NSS import on worker thread, not
    // IO thread. (Would need to add an ImportFromPKCS12Async method on
    // NSSCertDatabase.)
    int nss_import_result = cert_db->ImportFromPKCS12(
        slot.get(), std::string(base::as_string_view(file_bytes)),
        base::UTF8ToUTF16(password), is_extractable, nullptr);

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(finished_import_callback),
                                  std::move(file_bytes), std::move(password),
                                  nss_import_result));
  }

  void FinishedNSSImport(std::vector<uint8_t> file_bytes,
                         std::string password,
                         int nss_import_result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (nss_import_result == net::OK) {
      kcer::RecordPkcs12MigrationUmaEvent(
          kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportNssSuccess);
      // `import_hardware_backed_` == false indicates that the cert came from
      // the "Import" button. By default it's imported into the software NSS
      // database (aka public slot). With the experiment enabled it should also
      // be imported into Chaps. `import_hardware_backed_` == true means that
      // the cert came from the "Import and Bind" button and it's import into
      // Chaps by default.
      if (!import_hardware_backed_ &&
          chromeos::features::IsPkcs12ToChapsDualWriteEnabled()) {
        // Record the dual-write event. Even if the import fails, it's
        // theoretically possible that some related objects are still created
        // and would need to be deleted in case of a rollback.
        base::WeakPtr<kcer::Kcer> kcer =
            kcer::KcerFactoryAsh::GetKcer(profile_);
        if (kcer) {
          kcer::KcerFactoryAsh::RecordPkcs12CertDualWritten();
          return kcer->ImportPkcs12Cert(
              kcer::Token::kUser, kcer::Pkcs12Blob(std::move(file_bytes)),
              std::move(password),
              /*hardware_backed=*/import_hardware_backed_,
              /*mark_as_migrated=*/true,
              base::BindOnce(&CrosClientCertSource::FinishedKcerImport,
                             weak_ptr_factory_.GetWeakPtr(),
                             nss_import_result));
        }
      }
    } else {
      kcer::RecordPkcs12MigrationUmaEvent(
          kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportNssFailed);
    }

    ReplyToImportCallback(nss_import_result);
  }

  void FinishedKcerImport(
      int nss_import_result,
      base::expected<void, kcer::Error> kcer_import_result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (kcer_import_result.has_value()) {
      kcer::RecordPkcs12MigrationUmaEvent(
          kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportKcerSuccess);
    } else {
      kcer::RecordPkcs12MigrationUmaEvent(
          kcer::Pkcs12MigrationUmaEvent::kPkcs12ImportKcerFailed);
      kcer::RecordKcerError(kcer_import_result.error());
    }

    // Just return the nss_import_result. Kcer will attempt to import only if
    // NSS succeeds and even if Kcer fails, the cert should be usable.
    ReplyToImportCallback(nss_import_result);
  }

  void ReplyToImportCallback(int nss_import_result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (nss_import_result == net::OK) {
      // Refresh the certificate list to include the newly imported cert, and
      // call the import complete callback once the list has been updated.
      cros_cert_loader_->RefreshCachedCertificateList(base::BindOnce(
          std::move(import_callback_),
          certificate_manager_v2::mojom::ActionResult::NewSuccess(
              certificate_manager_v2::mojom::SuccessResult::kSuccess)));
    } else {
      // TODO(crbug.com/40928765): If the error was bad password, could prompt
      // the user to try again rather than just failing and requiring the user
      // to reselect the file to try again.
      int message_id;
      switch (nss_import_result) {
        case net::ERR_PKCS12_IMPORT_BAD_PASSWORD:
          message_id = IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_BAD_PASSWORD;
          break;
        case net::ERR_PKCS12_IMPORT_INVALID_MAC:
          message_id = IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_INVALID_MAC;
          break;
        case net::ERR_PKCS12_IMPORT_INVALID_FILE:
          message_id = IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_INVALID_FILE;
          break;
        case net::ERR_PKCS12_IMPORT_UNSUPPORTED:
          message_id = IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_UNSUPPORTED;
          break;
        default:
          message_id = IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_FAILED;
      }
      std::move(import_callback_)
          .Run(certificate_manager_v2::mojom::ActionResult::NewError(
              l10n_util::GetStringUTF8(message_id)));
    }
  }

  void GotDeleteCertificateConfirmation(
      const std::string& sha256hash_hex,
      CertificateManagerPageHandler::DeleteCertificateCallback callback,
      bool confirmed) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!confirmed) {
      std::move(callback).Run(nullptr);
      return;
    }

    scoped_refptr<net::X509Certificate> cert =
        cros_cert_loader_->FindCertificate(sha256hash_hex);
    if (!cert) {
      // This error is not expected to be displayed under normal circumstances,
      // so it's not localized.
      std::move(callback).Run(
          certificate_manager_v2::mojom::ActionResult::NewError(
              "cert not found"));
      return;
    }

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CrosClientCertSource::GetCertDBOnIOThread,
            NssServiceFactory::GetForContext(profile_)
                ->CreateNSSCertDatabaseGetterForIOThread(),
            base::BindOnce(
                &CrosClientCertSource::GotNSSCertDatabaseForDeleteOnIOThread,
                cert, ClientCertManagementAccessControls(profile_),
                base::BindOnce(&CrosClientCertSource::FinishedDelete,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback)))));
  }

  static void GotNSSCertDatabaseForDeleteOnIOThread(
      scoped_refptr<net::X509Certificate> cert,
      ClientCertManagementAccessControls client_cert_policy,
      base::OnceCallback<void(bool nss_delete_result)> finished_delete_callback,
      net::NSSCertDatabase* cert_db) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    net::ScopedCERTCertificate nss_cert =
        net::x509_util::CreateCERTCertificateFromX509Certificate(cert.get());

    if (!nss_cert) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(finished_delete_callback), false));
      return;
    }

    const auto hardware_backed =
        cert_db->IsHardwareBacked(nss_cert.get())
            ? ClientCertManagementAccessControls::kHardwareBacked
            : ClientCertManagementAccessControls::kSoftwareBacked;
    const auto device_wide =
        cert_db->IsCertificateOnSlot(nss_cert.get(),
                                     cert_db->GetSystemSlot().get())
            ? ClientCertManagementAccessControls::kDeviceWide
            : ClientCertManagementAccessControls::kUser;
    if (!client_cert_policy.IsChangeAllowed(hardware_backed, device_wide)) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(finished_delete_callback), false));
      return;
    }

    cert_db->DeleteCertAndKeyAsync(
        std::move(nss_cert),
        base::BindPostTask(content::GetUIThreadTaskRunner({}),
                           std::move(finished_delete_callback)));
  }

  void FinishedDelete(
      CertificateManagerPageHandler::DeleteCertificateCallback callback,
      bool delete_result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (delete_result) {
      // Refresh the certificate list to remove the deleted cert, and
      // call the deletion complete callback once the list has been updated.
      cros_cert_loader_->RefreshCachedCertificateList(base::BindOnce(
          std::move(callback),
          certificate_manager_v2::mojom::ActionResult::NewSuccess(
              certificate_manager_v2::mojom::SuccessResult::kSuccess)));
    } else {
      // TODO(crbug.com/40928765): pass through better error status codes from
      // the lower level deletion code?
      std::move(callback).Run(
          certificate_manager_v2::mojom::ActionResult::NewError(
              l10n_util::GetStringUTF8(
                  IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR)));
    }
  }

  std::unique_ptr<CrosCertLoader> cros_cert_loader_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  bool import_hardware_backed_;
  CertificateManagerPageHandler::ImportCertificateCallback import_callback_;
  raw_ptr<mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>>
      remote_client_;
  raw_ptr<Profile> profile_;
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
      PopulateCertInfosFromCertificateList(std::move(callback), *certs_,
                                           /*is_deletable=*/false);
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
    PopulateCertInfosFromCertificateList(std::move(callback), *certs_,
                                         /*is_deletable=*/false);
  }

  std::unique_ptr<chromeos::CertificateProvider> provider_;
  std::optional<net::CertificateList> certs_;
  base::WeakPtrFactory<ExtensionsClientCertSource> weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreatePlatformClientCertSource(
    mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>*
        remote_client,
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<CrosClientCertSource>(remote_client, profile);
#else
  return std::make_unique<ClientCertSource>(
      CreatePlatformClientCertLoader(profile));
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
ClientCertManagementAccessControls::ClientCertManagementAccessControls(
    Profile* profile)
    : is_guest_(
          user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
          user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()),
      is_kiosk_(user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()),
      client_cert_policy_(static_cast<ClientCertificateManagementPermission>(
          profile->GetPrefs()->GetInteger(
              prefs::kClientCertificateManagementAllowed))) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

bool ClientCertManagementAccessControls::IsManagementAllowed(
    KeyStorage key_storage) const {
  return !(key_storage == kHardwareBacked && is_guest_) && !is_kiosk_ &&
         client_cert_policy_ != ClientCertificateManagementPermission::kNone;
}

bool ClientCertManagementAccessControls::IsChangeAllowed(
    KeyStorage key_storage,
    CertLocation cert_location) const {
  if (!IsManagementAllowed(key_storage)) {
    return false;
  }

  if (cert_location == kUser) {
    return client_cert_policy_ != ClientCertificateManagementPermission::kNone;
  }

  return client_cert_policy_ == ClientCertificateManagementPermission::kAll;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif
