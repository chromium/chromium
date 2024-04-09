// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/certificate_manager_handler.h"

#include <vector>

CertificateManagerPageHandler::CertificateManagerPageHandler(
    mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
        pending_client,
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandler>
        pending_handler)
    : remote_client_(std::move(pending_client)),
      handler_(this, std::move(pending_handler)) {}

CertificateManagerPageHandler::~CertificateManagerPageHandler() = default;

// TODO(crbug.com/40928765): Replace this with a real implementation.
void CertificateManagerPageHandler::GetChromeRootStoreCerts(
    GetChromeRootStoreCertsCallback callback) {
  std::vector<certificate_manager_v2::mojom::SummaryCertInfoPtr> cert_infos;
  cert_infos.push_back(certificate_manager_v2::mojom::SummaryCertInfo::New(
      "hash", "display_name"));
  std::move(callback).Run(std::move(cert_infos));
}
