// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/certificate_manager_handler.h"

#include <vector>

#include "chrome/common/net/x509_certificate_model.h"
#include "content/public/browser/network_service_instance.h"
#include "net/cert/x509_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

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

}  // namespace

CertificateManagerPageHandler::CertificateManagerPageHandler(
    mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
        pending_client,
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandler>
        pending_handler)
    : remote_client_(std::move(pending_client)),
      handler_(this, std::move(pending_handler)) {}

CertificateManagerPageHandler::~CertificateManagerPageHandler() = default;

void CertificateManagerPageHandler::GetChromeRootStoreCerts(
    GetChromeRootStoreCertsCallback callback) {
  cert_verifier::mojom::CertVerifierServiceFactory* factory =
      content::GetCertVerifierServiceFactory();
  DCHECK(factory);
  factory->GetChromeRootStoreInfo(
      base::BindOnce(&PopulateChromeRootStoreLogsAsync, std::move(callback)));
}
