// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/client_certificates_service.h"

#include <iterator>
#include <memory>
#include <numeric>
#include <utility>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/ssl_client_cert_identity_wrapper.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_matcher.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace client_certificates {

namespace {

// Filters the given `identities` list to only include those that match the
// server's `cert_request_info`. When checking the match, this method also
// extracts the intermediate certificates from the `managed_identity` (if valid)
// to assist in building a valid certificate chain to the requested CAs.
void FilterIdentities(net::ClientCertIdentityList* identities,
                      const net::SSLCertRequestInfo& cert_request_info,
                      const std::optional<ClientIdentity>& managed_identity) {
  net::ClientCertIssuerSourceCollection sources;
  if (managed_identity.has_value() && managed_identity->is_valid()) {
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediate_buffers;
    for (const auto& buffer :
         managed_identity->certificate->intermediate_buffers()) {
      intermediate_buffers.push_back(bssl::UpRef(buffer.get()));
    }
    sources.push_back(std::make_unique<net::ClientCertIssuerSourceInMemory>(
        std::move(intermediate_buffers)));
  }
  net::FilterMatchingClientCertIdentities(identities, cert_request_info,
                                          sources);
}

void ConvertIdentityToList(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    base::OnceCallback<void(net::ClientCertIdentityList)> callback,
    std::optional<ClientIdentity> managed_identity) {
  net::ClientCertIdentityList managed_identities;
  if (managed_identity.has_value() && managed_identity->is_valid()) {
    auto ssl_private_key = managed_identity->private_key->GetSSLPrivateKey();
    if (ssl_private_key) {
      managed_identities.push_back(
          std::make_unique<SSLClientCertIdentityWrapper>(
              managed_identity->certificate, std::move(ssl_private_key)));
    }
  }

  if (cert_request_info) {
    FilterIdentities(&managed_identities, *cert_request_info, managed_identity);
  }

  std::move(callback).Run(std::move(managed_identities));
}

}  // namespace

class ClientCertificatesServiceImpl : public ClientCertificatesService {
 public:
  ClientCertificatesServiceImpl(
      CertificateProvisioningService* profile_certificate_provisioning_service,
      CertificateProvisioningService* browser_certificate_provisioning_service,
      std::unique_ptr<net::ClientCertStore> platform_certificate_store);

  ~ClientCertificatesServiceImpl() override;

  // net::ClientCertStore:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override;

 private:
  void FlattenLists(
      ClientCertListCallback callback,
      std::vector<net::ClientCertIdentityList> client_certs_lists);

  const raw_ptr<CertificateProvisioningService>
      profile_certificate_provisioning_service_;
  const raw_ptr<CertificateProvisioningService>
      browser_certificate_provisioning_service_;
  std::unique_ptr<net::ClientCertStore> platform_certificate_store_;

  base::WeakPtrFactory<ClientCertificatesServiceImpl> weak_factory_{this};
};

std::unique_ptr<ClientCertificatesService> ClientCertificatesService::Create(
    CertificateProvisioningService* profile_certificate_provisioning_service,
    CertificateProvisioningService* browser_certificate_provisioning_service,
    std::unique_ptr<net::ClientCertStore> platform_certificate_store) {
  return std::make_unique<ClientCertificatesServiceImpl>(
      profile_certificate_provisioning_service,
      browser_certificate_provisioning_service,
      std::move(platform_certificate_store));
}

ClientCertificatesServiceImpl::ClientCertificatesServiceImpl(
    CertificateProvisioningService* profile_certificate_provisioning_service,
    CertificateProvisioningService* browser_certificate_provisioning_service,
    std::unique_ptr<net::ClientCertStore> platform_certificate_store)
    : profile_certificate_provisioning_service_(
          profile_certificate_provisioning_service),
      browser_certificate_provisioning_service_(
          browser_certificate_provisioning_service),
      platform_certificate_store_(std::move(platform_certificate_store)) {
  CHECK(platform_certificate_store_);
  CHECK(profile_certificate_provisioning_service_ ||
        browser_certificate_provisioning_service_);
}

ClientCertificatesServiceImpl::~ClientCertificatesServiceImpl() = default;

void ClientCertificatesServiceImpl::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  auto barrier_callback = base::BarrierCallback<net::ClientCertIdentityList>(
      3U, base::BindOnce(&ClientCertificatesServiceImpl::FlattenLists,
                         weak_factory_.GetWeakPtr(), std::move(callback)));

  platform_certificate_store_->GetClientCerts(cert_request_info,
                                              barrier_callback);

  if (browser_certificate_provisioning_service_) {
    browser_certificate_provisioning_service_->GetManagedIdentity(
        base::BindOnce(ConvertIdentityToList, cert_request_info,
                       barrier_callback));
  } else {
    barrier_callback.Run(net::ClientCertIdentityList());
  }

  if (profile_certificate_provisioning_service_) {
    profile_certificate_provisioning_service_->GetManagedIdentity(
        base::BindOnce(ConvertIdentityToList, cert_request_info,
                       barrier_callback));
  } else {
    barrier_callback.Run(net::ClientCertIdentityList());
  }
}

void ClientCertificatesServiceImpl::FlattenLists(
    ClientCertListCallback callback,
    std::vector<net::ClientCertIdentityList> client_certs_lists) {
  // Flatten client_certs_lists.
  net::ClientCertIdentityList single_list;
  single_list.reserve(std::accumulate(
      client_certs_lists.begin(), client_certs_lists.end(), 0U,
      [](size_t acc, const net::ClientCertIdentityList& sub_list) {
        return acc + sub_list.size();
      }));
  for (net::ClientCertIdentityList& sub_list : client_certs_lists) {
    base::Extend(single_list, std::move(sub_list));
  }

  std::move(callback).Run(std::move(single_list));
}

}  // namespace client_certificates
