// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_certificate_repository.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/attestation/oak_ct_verifier.h"

namespace private_ai {

TlsCertificateRepository::TlsCertificateRepository(
    std::unique_ptr<TlsCertificateDownloader> downloader,
    std::string oakct_namespace,
    std::string oakct_base_domain,
    network::NetworkContextGetter network_context_getter)
    : downloader_(std::move(downloader)),
      oakct_namespace_(std::move(oakct_namespace)),
      oakct_base_domain_(std::move(oakct_base_domain)),
      network_context_getter_(std::move(network_context_getter)) {
  CHECK(downloader_);
}

TlsCertificateRepository::~TlsCertificateRepository() = default;

void TlsCertificateRepository::GetCertificates(CertificatesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  if (cached_certs_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), cached_certs_));
    return;
  }

  RequestLookup(std::move(callback));
}

void TlsCertificateRepository::ForceRefresh(CertificatesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  cached_certs_.reset();
  RequestLookup(std::move(callback));
}

void TlsCertificateRepository::RequestLookup(CertificatesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Multiple callers can arrive while either the download or the subsequent
  // asynchronous OakCT verification is in flight. Coalesce them into
  // `pending_callbacks_` so that only one end-to-end lookup (download +
  // verification) runs at a time and we avoid redundant certificate
  // verification calls in the network service.
  const bool lookup_in_flight = !pending_callbacks_.empty();
  pending_callbacks_.push_back(std::move(callback));
  if (lookup_in_flight) {
    return;
  }

  downloader_->Download(base::BindOnce(&TlsCertificateRepository::OnDownloaded,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void TlsCertificateRepository::OnDownloaded(std::optional<PemBundle> bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean("PrivateAi.Certs.DownloadSuccess",
                            bundle.has_value());

  if (!bundle) {
    Finish(std::nullopt);
    return;
  }

  Verify(*bundle);
}

void TlsCertificateRepository::Verify(const PemBundle& bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  network::mojom::NetworkContext* network_context =
      network_context_getter_ ? network_context_getter_.Run() : nullptr;

  // Both files are verified exactly as published. The verifier reads them
  // synchronously; the TCA root is copied into the callback to be handed out
  // on success.
  OakCtVerifier::Verify(
      network_context, base::as_byte_span(bundle.tca_pem),
      base::as_byte_span(bundle.oakct_pem), oakct_namespace_,
      oakct_base_domain_,
      base::BindOnce(&TlsCertificateRepository::OnVerified,
                     weak_ptr_factory_.GetWeakPtr(), bundle.tca_pem));
}

void TlsCertificateRepository::OnVerified(std::string tca_root_pem,
                                          OakCtVerifier::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramEnumeration("PrivateAi.Certs.OakCtVerificationResult",
                                result);

  if (result != OakCtVerifier::Result::kVerified) {
    LOG(ERROR) << "TCA root certificate failed OakCT verification: "
               << static_cast<int>(result);
    Finish(std::nullopt);
    return;
  }

  Finish(TlsCertificates{.tca_root_pem = std::move(tca_root_pem)});
}

void TlsCertificateRepository::Finish(std::optional<TlsCertificates> certs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (certs) {
    cached_certs_ = certs;
  }

  // Take the callbacks first so that re-entrant calls see an idle repository
  // and can start a new lookup.
  std::vector<CertificatesCallback> callbacks =
      std::exchange(pending_callbacks_, {});
  for (auto& callback : callbacks) {
    std::move(callback).Run(certs);
  }
}

}  // namespace private_ai
