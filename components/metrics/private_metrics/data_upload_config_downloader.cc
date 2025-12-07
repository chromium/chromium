// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/data_upload_config_downloader.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace metrics::private_metrics {

namespace {
constexpr size_t kMaxDownloadBytes = 4 * 1024 * 1024;  // 4 MiBs

// Max number of retries for fetching the configuration.
constexpr int kMaxRetries = 3;

// The conditions for which a retry will trigger.
constexpr int kRetryMode = network::SimpleURLLoader::RETRY_ON_5XX |
                           network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                           network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;

constexpr net::NetworkTrafficAnnotationTag kPrivateMetricsKeyNetworkTag =
    net::DefineNetworkTrafficAnnotation("private_metrics_encryption_key", R"(
        semantics {
          sender: "Private Metrics Encryption Key"
          description:
            "Retrieves a public key and its signed endorsements in the form "
            "of a serialized DataUploadConfig. The public key will be used to "
            "encrypt PrivateMetricReport prior to uploading to Google "
            "servers. The private key will only be granted in a trusted "
            "execution environment, and the signed endorsements can be used "
            "to verify the attestation."
          trigger:
            "Private Metric encryption keys will automatically be fetched "
            "while Chrome is running with usage statistics and 'Make searches "
            "and browsing better' settings enabled."
          data: "None"
          user_data {
            type: NONE
          }
          internal {
            contacts {
              email: "//components/metrics/OWNERS"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2025-08-21"
        }
        policy {
         cookies_allowed: NO
         setting:
            "Users can enable or disable this feature by disabling 'Make "
            "searches and browsing better' in Chrome's settings under Advanced "
            "Settings, Privacy. This has to be enabled for all active "
            "profiles. This is only enabled if the user has 'Help improve "
            "Chrome's features and performance' enabled in the same settings "
            "menu."
        chrome_policy {
          MetricsReportingEnabled {
            policy_options {mode: MANDATORY}
            MetricsReportingEnabled: false
          }
          UrlKeyedAnonymizedDataCollectionEnabled {
            policy_options {mode: MANDATORY}
            UrlKeyedAnonymizedDataCollectionEnabled: false
          }
        }
        })");

inline constexpr char kDataUploadConfigGstaticUrl[] =
    "https://www.gstatic.com/chrome/private-metrics/data-upload-config";
}  // namespace

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
    const GURL& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kPrivateMetricsKeyNetworkTag);
}

DataUploadConfigDownloader::DataUploadConfigDownloader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

DataUploadConfigDownloader::~DataUploadConfigDownloader() = default;

void DataUploadConfigDownloader::FetchDataUploadConfig(
    DataUploadConfigCallback callback) {
  if (!url_loader_factory_ || pending_request_) {
    return;
  }

  GURL destination = GURL(kDataUploadConfigGstaticUrl);
  pending_request_ = CreateSimpleURLLoader(destination);
  network::SimpleURLLoader::BodyAsStringCallback handler = base::BindOnce(
      &DataUploadConfigDownloader::HandleSerializedDataUploadConfig,
      self_ptr_factory_.GetWeakPtr(), std::move(callback));
  pending_request_->SetRetryOptions(kMaxRetries, kRetryMode);
  pending_request_->DownloadToString(url_loader_factory_.get(),
                                     std::move(handler), kMaxDownloadBytes);
}

raw_ptr<network::SimpleURLLoader>
DataUploadConfigDownloader::GetPendingRequestForTesting() {
  return pending_request_.get();
}

void DataUploadConfigDownloader::HandleSerializedDataUploadConfig(
    DataUploadConfigCallback callback,
    std::optional<std::string> response_body) {
  if (!pending_request_) {
    return;
  }

  // Move the pending request here so it's deleted when this function ends.
  std::unique_ptr<network::SimpleURLLoader> request =
      std::move(pending_request_);

  // Response code is not 200 or `response_body` is empty.
  if (!request->ResponseInfo() || !request->ResponseInfo()->headers ||
      request->ResponseInfo()->headers->response_code() != net::HTTP_OK ||
      !response_body.has_value() || response_body->empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::UmaHistogramCounts10000("PrivateMetrics.DataUploadConfig.DownloadSize",
                                response_body->size() / 1024);

  fcp::confidentialcompute::DataUploadConfig data_upload_config;
  if (!data_upload_config.ParseFromString(response_body.value())) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(data_upload_config);
}

}  // namespace metrics::private_metrics
