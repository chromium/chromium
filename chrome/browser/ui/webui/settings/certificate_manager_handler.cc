// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/certificate_manager_handler.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "content/public/browser/network_service_instance.h"
#include "crypto/crypto_buildflags.h"
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
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/client_certificates_service.h"
#include "components/enterprise/client_certificates/core/features.h"
#endif

namespace {

void PopulateChromeRootStoreLogsAsync(
    CertificateManagerPageHandler::GetChromeRootStoreCertsCallback callback,
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

class ClientCertSource {
 public:
  virtual ~ClientCertSource() = default;

  virtual void GetCerts(
      base::OnceCallback<void(net::CertificateList)> callback) = 0;
};

// A ClientCertSource that wraps a ClientCertStore. Read-only.
// Lifetimes note: The callback will not be called if the ClientCertStoreSource
// (and thus, the ClientCertStore) is destroyed first.
class ClientCertStoreSource : public ClientCertSource {
 public:
  explicit ClientCertStoreSource(std::unique_ptr<net::ClientCertStore> store)
      : store_(std::move(store)) {}
  ~ClientCertStoreSource() override = default;

  void GetCerts(
      base::OnceCallback<void(net::CertificateList)> callback) override {
    store_->GetClientCerts(
        base::MakeRefCounted<net::SSLCertRequestInfo>(),
        base::BindOnce(&ClientCertStoreSource::HandleClientCertsResult,
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

std::unique_ptr<ClientCertSource> CreatePlatformClientCertSource() {
#if BUILDFLAG(USE_NSS_CERTS)
  return std::make_unique<ClientCertStoreSource>(
      std::make_unique<net::ClientCertStoreNSS>(
          base::BindRepeating(&CreateCryptoModuleBlockingPasswordDelegate,
                              kCryptoModulePasswordClientAuth)));
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<ClientCertStoreSource>(
      std::make_unique<net::ClientCertStoreWin>());
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<ClientCertStoreSource>(
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

std::unique_ptr<ClientCertSource> CreateProvisionedClientCertSource(
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

  return std::make_unique<ClientCertStoreSource>(
      client_certificates::ClientCertificatesService::Create(
          provisioning_service, std::make_unique<NullClientCertStore>()));
}
#endif

void PopulateClientCertsAsync(
    std::unique_ptr<ClientCertSource> source,
    CertificateManagerPageHandler::GetPlatformClientCertsCallback callback,
    net::CertificateList certs) {
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> out_infos;
  for (const auto& cert : certs) {
    x509_certificate_model::X509CertificateModel model(
        bssl::UpRef(cert->cert_buffer()), "");
    out_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
        model.HashCertSHA256(), model.GetTitle()));
  }
  std::move(callback).Run(std::move(out_infos));
}

}  // namespace

CertificateManagerPageHandler::CertificateManagerPageHandler(
    mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
        pending_client,
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandler>
        pending_handler,
    Profile* profile)
    : remote_client_(std::move(pending_client)),
      handler_(this, std::move(pending_handler)),
      profile_(profile) {}

CertificateManagerPageHandler::~CertificateManagerPageHandler() = default;

void CertificateManagerPageHandler::GetChromeRootStoreCerts(
    GetChromeRootStoreCertsCallback callback) {
  cert_verifier::mojom::CertVerifierServiceFactory* factory =
      content::GetCertVerifierServiceFactory();
  DCHECK(factory);
  factory->GetChromeRootStoreInfo(
      base::BindOnce(&PopulateChromeRootStoreLogsAsync, std::move(callback)));
}

void CertificateManagerPageHandler::GetPlatformClientCerts(
    GetPlatformClientCertsCallback callback) {
  std::unique_ptr<ClientCertSource> source = CreatePlatformClientCertSource();
  if (!source) {
    std::move(callback).Run({});
    return;
  }
  ClientCertSource* source_ptr = source.get();
  source_ptr->GetCerts(base::BindOnce(&PopulateClientCertsAsync,
                                      std::move(source), std::move(callback)));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void CertificateManagerPageHandler::GetProvisionedClientCerts(
    GetProvisionedClientCertsCallback callback) {
  std::unique_ptr<ClientCertSource> source =
      CreateProvisionedClientCertSource(profile_);
  if (!source) {
    std::move(callback).Run({});
    return;
  }
  ClientCertSource* source_ptr = source.get();
  source_ptr->GetCerts(base::BindOnce(&PopulateClientCertsAsync,
                                      std::move(source), std::move(callback)));
}
#endif
