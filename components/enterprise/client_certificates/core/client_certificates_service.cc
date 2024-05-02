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
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/threaded_ssl_private_key.h"

namespace client_certificates {

namespace {

void ConvertIdentityToList(
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

  std::move(callback).Run(std::move(managed_identities));
}

}  // namespace

class ClientCertificatesServiceImpl : public ClientCertificatesService {
 public:
  ClientCertificatesServiceImpl(
      CertificateProvisioningService* certificate_provisioning_service,
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
      certificate_provisioning_service_;
  std::unique_ptr<net::ClientCertStore> platform_certificate_store_;

  base::WeakPtrFactory<ClientCertificatesServiceImpl> weak_factory_{this};
};

std::unique_ptr<ClientCertificatesService> ClientCertificatesService::Create(
    CertificateProvisioningService* certificate_provisioning_service,
    std::unique_ptr<net::ClientCertStore> platform_certificate_store) {
  return std::make_unique<ClientCertificatesServiceImpl>(
      certificate_provisioning_service, std::move(platform_certificate_store));
}

ClientCertificatesServiceImpl::ClientCertificatesServiceImpl(
    CertificateProvisioningService* certificate_provisioning_service,
    std::unique_ptr<net::ClientCertStore> platform_certificate_store)
    : certificate_provisioning_service_(certificate_provisioning_service),
      platform_certificate_store_(std::move(platform_certificate_store)) {
  CHECK(certificate_provisioning_service_);
  CHECK(platform_certificate_store_);
}

ClientCertificatesServiceImpl::~ClientCertificatesServiceImpl() = default;

void ClientCertificatesServiceImpl::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  auto barrier_callback = base::BarrierCallback<net::ClientCertIdentityList>(
      2U, base::BindOnce(&ClientCertificatesServiceImpl::FlattenLists,
                         weak_factory_.GetWeakPtr(), std::move(callback)));

  platform_certificate_store_->GetClientCerts(cert_request_info,
                                              barrier_callback);

  certificate_provisioning_service_->GetManagedIdentity(
      base::BindOnce(ConvertIdentityToList, barrier_callback));
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
