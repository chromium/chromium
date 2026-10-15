// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TLS_CERTIFICATE_DOWNLOADER_H_
#define COMPONENTS_PRIVATE_AI_TLS_CERTIFICATE_DOWNLOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace private_ai {

// Downloads the Trusted Certificate Authority (TCA) root certificate and its
// accompanying Oak Certificate Transparency (OakCT) certificate, used for
// Private AI TLS sessions, from Google's Static Content Service (SCS).
//
// This class only moves bytes: it performs no verification and no caching.
// See TlsCertificateRepository for both.
class TlsCertificateDownloader {
 public:
  // The two certificates as served by SCS, PEM encoded.
  struct PemBundle {
    std::string tca_pem;
    std::string oakct_pem;
  };

  using DownloadCallback = base::OnceCallback<void(std::optional<PemBundle>)>;

  TlsCertificateDownloader(
      const GURL& tca_root_url,
      const GURL& oakct_cert_url,
      network::NetworkContextGetter network_context_getter);
  virtual ~TlsCertificateDownloader();

  TlsCertificateDownloader(const TlsCertificateDownloader&) = delete;
  TlsCertificateDownloader& operator=(const TlsCertificateDownloader&) = delete;

  // Downloads both certificates, bypassing the HTTP cache. Runs `callback` with
  // std::nullopt if either download fails. If a download is already in flight,
  // `callback` joins the in-flight download and is invoked once it completes.
  // The callback is always invoked asynchronously on the calling sequence.
  // Virtual to allow fakes in unit tests.
  virtual void Download(DownloadCallback callback);

  // Sets a pre-created URLLoaderFactory for testing.
  void SetURLLoaderFactoryForTesting(
      network::mojom::URLLoaderFactory* test_factory);

 private:
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();
  void OnTcaDownloaded(std::optional<std::string> response_body);
  void OnOakCtDownloaded(std::optional<std::string> response_body);
  void OnDownloadsComplete();
  void Finish(std::optional<PemBundle> bundle);

  const GURL tca_root_url_;
  const GURL oakct_cert_url_;
  const network::NetworkContextGetter network_context_getter_;

  raw_ptr<network::mojom::URLLoaderFactory> test_url_loader_factory_ = nullptr;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_remote_;

  // Callbacks awaiting the in-flight download, if any.
  std::vector<DownloadCallback> callbacks_;
  std::unique_ptr<network::SimpleURLLoader> tca_loader_;
  std::unique_ptr<network::SimpleURLLoader> oakct_loader_;
  std::optional<std::string> tca_pem_;
  std::optional<std::string> oakct_pem_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TlsCertificateDownloader> weak_ptr_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TLS_CERTIFICATE_DOWNLOADER_H_
