// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TLS_CERTIFICATE_REPOSITORY_H_
#define COMPONENTS_PRIVATE_AI_TLS_CERTIFICATE_REPOSITORY_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/private_ai/attestation/oak_ct_verifier.h"
#include "components/private_ai/tls_certificate_downloader.h"
#include "services/network/public/cpp/network_context_getter.h"

namespace private_ai {

// Holds the TCA root certificate, PEM-encoded exactly as published. A
// TlsCertificates instance is only ever handed out after the TCA root has been
// verified to be published to Oak Certificate Transparency (see OakCtVerifier).
struct TlsCertificates {
  std::string tca_root_pem;
};

// Provides the verified Trusted Certificate Authority (TCA) root certificate
// used for Private AI TLS sessions.
//
// The TCA root and its Oak Certificate Transparency (OakCT) certificate are
// obtained from a TlsCertificateDownloader and verified with OakCtVerifier
// before being handed out; the verified TCA root is then cached in memory.
// Concurrent callers share a single in-flight lookup.
class TlsCertificateRepository {
 public:
  using CertificatesCallback =
      base::OnceCallback<void(std::optional<TlsCertificates>)>;

  // `oakct_namespace` and `oakct_base_domain` select the OakCT hostname space
  // (<content_id>.<namespace>.<base_domain>) that the TCA root must have been
  // published under.
  TlsCertificateRepository(
      std::unique_ptr<TlsCertificateDownloader> downloader,
      std::string oakct_namespace,
      std::string oakct_base_domain,
      network::NetworkContextGetter network_context_getter);
  virtual ~TlsCertificateRepository();

  TlsCertificateRepository(const TlsCertificateRepository&) = delete;
  TlsCertificateRepository& operator=(const TlsCertificateRepository&) = delete;

  // Returns the certificates from memory if available, otherwise downloads
  // and verifies them. Runs `callback` with std::nullopt on failure.
  // The callback is always invoked asynchronously on the calling sequence.
  // Virtual to allow fakes in unit tests.
  virtual void GetCertificates(CertificatesCallback callback);

  // Drops the memory cache and downloads fresh certificates, or joins an
  // in-flight lookup if one is in progress. Used when the server's
  // certificate chain does not verify against the current TCA root.
  // The callback is always invoked asynchronously on the calling sequence.
  // Virtual to allow fakes in unit tests.
  virtual void ForceRefresh(CertificatesCallback callback);

 private:
  using PemBundle = TlsCertificateDownloader::PemBundle;

  // Lookup pipeline. A lookup is in flight exactly while `pending_callbacks_`
  // is non-empty; `RequestLookup()` either joins it or starts a new one.
  void RequestLookup(CertificatesCallback callback);
  void OnDownloaded(std::optional<PemBundle> bundle);
  void Verify(const PemBundle& bundle);
  void OnVerified(std::string tca_root_pem, OakCtVerifier::Result result);
  void Finish(std::optional<TlsCertificates> certs);

  const std::unique_ptr<TlsCertificateDownloader> downloader_;
  const std::string oakct_namespace_;
  const std::string oakct_base_domain_;
  const network::NetworkContextGetter network_context_getter_;

  // Verified certificates served from memory.
  std::optional<TlsCertificates> cached_certs_;

  // Callbacks waiting for the in-flight lookup, if any.
  std::vector<CertificatesCallback> pending_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TlsCertificateRepository> weak_ptr_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TLS_CERTIFICATE_REPOSITORY_H_
