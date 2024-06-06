// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/certificate_manager_handler.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "crypto/crypto_buildflags.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
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
        cert_info->sha256hash_hex, model.GetTitle()));
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
    if (cert_info->sha256hash_hex != hash) {
      continue;
    }

    // Found the cert, open cert viewer dialog if able and then exit function.
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> view_certs;
    view_certs.push_back(net::x509_util::CreateCryptoBuffer(cert_info->cert));
    CertificateViewerDialog::ShowConstrained(
        std::move(view_certs),
        /*cert_nicknames=*/{}, web_contents.get(),
        web_contents->GetTopLevelNativeWindow());
    return;
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

class ChromeRootStoreCertSource
    : public CertificateManagerPageHandler::CertSource {
 public:
  void GetCertificateInfos(
      CertificateManagerPageHandler::GetCertificatesCallback callback)
      override {
    content::GetCertVerifierServiceFactory()->GetChromeRootStoreInfo(
        base::BindOnce(&PopulateChromeRootStoreLogsAsync, std::move(callback)));
  }

  void ViewCertificate(
      const std::string& sha256_hex_hash,
      base::WeakPtr<content::WebContents> web_contents) override {
    // This should really use a cached set of info with other calls to
    // GetChromeRootStoreInfo.
    content::GetCertVerifierServiceFactory()->GetChromeRootStoreInfo(
        base::BindOnce(&ViewCrsCertificateAsync, sha256_hex_hash,
                       std::move(web_contents)));
  }

  void ExportCertificates(
      base::WeakPtr<content::WebContents> web_contents) override {
    // This should really use a cached set of info with other calls to
    // GetChromeRootStoreInfo.
    content::GetCertVerifierServiceFactory()->GetChromeRootStoreInfo(
        base::BindOnce(&ExportCrsCertificatesAsync, web_contents));
  }
};

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
      base::OnceCallback<void(net::CertificateList)>
          callback,
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
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
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> view_certs;
      view_certs.push_back(bssl::UpRef(cert->cert_buffer()));
      CertificateViewerDialog::ShowConstrained(
          std::move(view_certs),
          /*cert_nicknames=*/{}, web_contents.get(),
          web_contents->GetTopLevelNativeWindow());
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

CertificateManagerPageHandler::CertificateManagerPageHandler(
    mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
        pending_client,
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandler>
        pending_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : remote_client_(std::move(pending_client)),
      handler_(this, std::move(pending_handler)),
      profile_(profile),
      web_contents_(web_contents) {}

CertificateManagerPageHandler::~CertificateManagerPageHandler() = default;

void CertificateManagerPageHandler::GetCertificates(
    certificate_manager_v2::mojom::CertificateSource source_id,
    GetCertificatesCallback callback) {
  GetCertSource(source_id).GetCertificateInfos(std::move(callback));
}

void CertificateManagerPageHandler::ViewCertificate(
    certificate_manager_v2::mojom::CertificateSource source_id,
    const std::string& sha256hash_hex) {
  GetCertSource(source_id).ViewCertificate(sha256hash_hex,
                                           web_contents_->GetWeakPtr());
}

void CertificateManagerPageHandler::ExportCertificates(
    certificate_manager_v2::mojom::CertificateSource source_id) {
  GetCertSource(source_id).ExportCertificates(web_contents_->GetWeakPtr());
}

CertificateManagerPageHandler::CertSource::~CertSource() = default;

CertificateManagerPageHandler::CertSource&
CertificateManagerPageHandler::GetCertSource(
    certificate_manager_v2::mojom::CertificateSource source) {
  std::unique_ptr<CertSource>& source_ptr =
      cert_source_[static_cast<unsigned>(source)];
  if (!source_ptr) {
    switch (source) {
      case certificate_manager_v2::mojom::CertificateSource::kChromeRootStore:
        source_ptr = std::make_unique<ChromeRootStoreCertSource>();
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kPlatformClientCert:
        source_ptr = std::make_unique<ClientCertSource>(
            CreatePlatformClientCertLoader());
        break;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      case certificate_manager_v2::mojom::CertificateSource::
          kProvisionedClientCert:
        source_ptr = std::make_unique<ClientCertSource>(
            CreateProvisionedClientCertLoader(profile_));
        break;
#endif
#if BUILDFLAG(IS_CHROMEOS)
      case certificate_manager_v2::mojom::CertificateSource::
          kExtensionsClientCert:
        chromeos::CertificateProviderService* certificate_provider_service =
            chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
                profile_);
        source_ptr = std::make_unique<ExtensionsClientCertSource>(
            certificate_provider_service->CreateCertificateProvider());
        break;
#endif
    }
  }
  return *source_ptr;
}

void CertificateManagerPageHandler::GetPolicyInformation(
    GetPolicyInformationCallback callback) {
  ProfileNetworkContextService* service =
      ProfileNetworkContextServiceFactory::GetForContext(profile_.get());
  ProfileNetworkContextService::CertificatePoliciesForView policies =
      service->GetCertificatePolicyForView();

  certificate_manager_v2::mojom::CertPolicyInfoPtr cert_policy_info =
      certificate_manager_v2::mojom::CertPolicyInfo::New();
#if !BUILDFLAG(IS_CHROMEOS)
  cert_policy_info->include_system_trust_store =
      policies.certificate_policies->include_system_trust_store;
  cert_policy_info->is_include_system_trust_store_managed =
      policies.is_include_system_trust_store_managed;
#else
  // TODO(crbug.com/40928765): figure out how this should be displayed for
  // ChromeOS
  cert_policy_info->include_system_trust_store = true;
  cert_policy_info->is_include_system_trust_store_managed = false;
#endif

  cert_policy_info->num_policy_certs =
      policies.full_distrusted_certs.size() +
      policies.certificate_policies->trust_anchors.size() +
      policies.certificate_policies->trust_anchors_with_enforced_constraints
          .size() +
      policies.certificate_policies->trust_anchors_with_additional_constraints
          .size() +
      policies.certificate_policies->all_certificates.size();

  std::move(callback).Run(std::move(cert_policy_info));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void CertificateManagerPageHandler::ShowNativeManageCertificates() {
  settings_utils::ShowManageSSLCertificates(web_contents_);
}
#endif
