// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_certificate_downloader.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

namespace {

constexpr size_t kMaxCertSizeBytes = 64 * 1024;  // 64 KiB

constexpr net::NetworkTrafficAnnotationTag kCertTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("private_ai_cert_fetch", R"(
        semantics {
          sender: "PrivateAI Certificate Fetcher"
          description:
            "Fetches the Trusted Certificate Authority (TCA) root certificate "
            "and its accompanying Oak Certificate Transparency (OakCT) "
            "certificate from Google's Static Content Service. These "
            "certificates are required to establish a hardware-attested, "
            "end-to-end TLS connection with Private AI servers."
          trigger:
            "Triggered on the first Private AI request when no valid cached "
            "certificate is available in memory or on disk."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "georgeseb@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2026-09-24"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by users directly, but is only "
            "active when Private AI features (such as ContextualCues for "
            "Gemini) are enabled."
          policy_exception_justification:
            "Essential security certificates for encrypted inference."
        })");

std::unique_ptr<network::SimpleURLLoader> CreateLoader(const GURL& url) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return network::SimpleURLLoader::Create(std::move(request),
                                          kCertTrafficAnnotation);
}

}  // namespace

TlsCertificateDownloader::TlsCertificateDownloader(
    const GURL& tca_root_url,
    const GURL& oakct_cert_url,
    network::NetworkContextGetter network_context_getter)
    : tca_root_url_(tca_root_url),
      oakct_cert_url_(oakct_cert_url),
      network_context_getter_(std::move(network_context_getter)) {}

TlsCertificateDownloader::~TlsCertificateDownloader() = default;

void TlsCertificateDownloader::SetURLLoaderFactoryForTesting(
    network::mojom::URLLoaderFactory* test_factory) {
  test_url_loader_factory_ = test_factory;
}

void TlsCertificateDownloader::Download(DownloadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  callbacks_.push_back(std::move(callback));
  if (callbacks_.size() > 1) {
    return;
  }

  network::mojom::URLLoaderFactory* factory = GetURLLoaderFactory();
  if (!factory) {
    LOG(ERROR) << "No URLLoaderFactory available for certificate download";
    // Keep the callback asynchronous even on this early failure.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TlsCertificateDownloader::Finish,
                       weak_ptr_factory_.GetWeakPtr(), std::nullopt));
    return;
  }

  tca_loader_ = CreateLoader(tca_root_url_);
  oakct_loader_ = CreateLoader(oakct_cert_url_);

  tca_loader_->DownloadToString(
      factory,
      base::BindOnce(&TlsCertificateDownloader::OnTcaDownloaded,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxCertSizeBytes);

  oakct_loader_->DownloadToString(
      factory,
      base::BindOnce(&TlsCertificateDownloader::OnOakCtDownloaded,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxCertSizeBytes);
}

network::mojom::URLLoaderFactory*
TlsCertificateDownloader::GetURLLoaderFactory() {
  if (test_url_loader_factory_) {
    return test_url_loader_factory_;
  }
  if (!url_loader_factory_remote_.is_bound() ||
      !url_loader_factory_remote_.is_connected()) {
    url_loader_factory_remote_.reset();
    if (!network_context_getter_) {
      return nullptr;
    }
    network::mojom::NetworkContext* network_context =
        network_context_getter_.Run();
    if (!network_context) {
      return nullptr;
    }
    network_context->CreateURLLoaderFactory(
        url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
        network::mojom::URLLoaderFactoryParams::New());
  }
  return url_loader_factory_remote_.get();
}

void TlsCertificateDownloader::OnTcaDownloaded(
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response_body) {
    LOG(ERROR) << "Failed to download TCA root certificate: net error "
               << tca_loader_->NetError();
    Finish(std::nullopt);
    return;
  }

  tca_pem_ = std::move(response_body);
  OnDownloadsComplete();
}

void TlsCertificateDownloader::OnOakCtDownloaded(
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response_body) {
    LOG(ERROR) << "Failed to download OakCT certificate: net error "
               << oakct_loader_->NetError();
    Finish(std::nullopt);
    return;
  }

  oakct_pem_ = std::move(response_body);
  OnDownloadsComplete();
}

void TlsCertificateDownloader::OnDownloadsComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!tca_pem_ || !oakct_pem_) {
    return;
  }

  Finish(PemBundle{.tca_pem = std::move(*tca_pem_),
                   .oakct_pem = std::move(*oakct_pem_)});
}

void TlsCertificateDownloader::Finish(std::optional<PemBundle> bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset all state before running the callbacks so that they may start another
  // download re-entrantly.
  tca_loader_.reset();
  oakct_loader_.reset();
  tca_pem_.reset();
  oakct_pem_.reset();

  base::WeakPtr<TlsCertificateDownloader> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  std::vector<DownloadCallback> callbacks = std::exchange(callbacks_, {});
  for (auto& callback : callbacks) {
    std::move(callback).Run(bundle);
    if (!weak_this) {
      return;
    }
  }
}

}  // namespace private_ai
