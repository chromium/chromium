// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_data_fetcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "net/base/data_url.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kContentLocationHeader[] = "Content-Location";
const char kNoUmaClient[] = "NoClient";

const int kDownloadTimeoutSeconds = 30;

}  // namespace

namespace image_fetcher {

// An active image URL fetcher request. The struct contains the related requests
// state.
struct ImageDataFetcher::ImageDataFetcherRequest {
  ImageDataFetcherRequest(ImageDataFetcherCallback callback,
                          std::unique_ptr<network::SimpleURLLoader> loader)
      : callback(std::move(callback)), loader(std::move(loader)) {}

  ~ImageDataFetcherRequest() = default;

  // The callback to run after the image data was fetched. The callback will
  // be run even if the image data could not be fetched successfully.
  ImageDataFetcherCallback callback;

  std::unique_ptr<network::SimpleURLLoader> loader;
};

ImageDataFetcher::ImageDataFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDataFetcher::~ImageDataFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ImageDataFetcher::SetImageDownloadLimit(
    std::optional<int64_t> max_download_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_download_bytes_ = max_download_bytes;
}

void ImageDataFetcher::FetchImageData(const GURL& image_url,
                                      ImageDataFetcherCallback callback,
                                      ImageFetcherParams params,
                                      bool send_cookies) {
  FetchImageData(
      image_url, std::move(callback), params, /*referrer=*/std::string(),
      net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      send_cookies);
}

void ImageDataFetcher::FetchImageData(
    const GURL& image_url,
    ImageDataFetcherCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    bool send_cookies) {
  FetchImageData(
      image_url, std::move(callback),
      ImageFetcherParams(traffic_annotation, kNoUmaClient),
      /*referrer=*/std::string(),
      net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      send_cookies);
}

void ImageDataFetcher::FetchImageData(
    const GURL& image_url,
    ImageDataFetcherCallback callback,
    const std::string& referrer,
    net::ReferrerPolicy referrer_policy,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    bool send_cookies) {
  FetchImageData(image_url, std::move(callback),
                 ImageFetcherParams(traffic_annotation, kNoUmaClient), referrer,
                 referrer_policy, send_cookies);
}

void ImageDataFetcher::FetchImageData(const GURL& image_url,
                                      ImageDataFetcherCallback callback,
                                      ImageFetcherParams params,
                                      const std::string& referrer,
                                      net::ReferrerPolicy referrer_policy,
                                      bool send_cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Handle data urls explicitly since SimpleURLLoader doesn't.
  if (image_url.SchemeIs(url::kDataScheme)) {
    RequestMetadata metadata;
    std::string charset, data;
    if (!net::DataURL::Parse(image_url, &metadata.mime_type, &charset, &data)) {
      DVLOG(0) << "Failed to parse data url";
    }

    std::move(callback).Run(std::move(data), metadata);
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = image_url;
  request->referrer_policy = referrer_policy;
  request->referrer = GURL(referrer);
  request->credentials_mode = send_cookies
                                  ? network::mojom::CredentialsMode::kInclude
                                  : network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       params.traffic_annotation());

  // For compatibility in error handling. This is a little wasteful since the
  // body will get thrown out anyway, though.
  loader->SetAllowHttpErrorResults(true);

  loader->SetTimeoutDuration(base::Seconds(kDownloadTimeoutSeconds));

  if (max_download_bytes_.has_value()) {
    loader->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&ImageDataFetcher::OnURLLoaderComplete,
                       base::Unretained(this), loader.get(), std::move(params)),
        max_download_bytes_.value());
  } else {
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&ImageDataFetcher::OnURLLoaderComplete,
                       base::Unretained(this), loader.get(),
                       std::move(params)));
  }

  std::unique_ptr<ImageDataFetcherRequest> request_track(
      new ImageDataFetcherRequest(std::move(callback), std::move(loader)));

  network::SimpleURLLoader* loader_raw = request_track->loader.get();
  pending_requests_[loader_raw] = std::move(request_track);
}

void ImageDataFetcher::OnURLLoaderComplete(
    const network::SimpleURLLoader* source,
    ImageFetcherParams params,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_requests_.find(source) != pending_requests_.end());
  bool success = source->NetError() == net::OK;
  int status_code = source->NetError();

  RequestMetadata metadata;
  if (success && source->ResponseInfo() && source->ResponseInfo()->headers) {
    net::HttpResponseHeaders* headers = source->ResponseInfo()->headers.get();
    metadata.mime_type = source->ResponseInfo()->mime_type;
    metadata.http_response_code = headers->response_code();
    // Just read the first value-pair for this header (not caring about |iter|).
    headers->EnumerateHeader(
        /*iter=*/nullptr, kContentLocationHeader,
        &metadata.content_location_header);
    success &= (metadata.http_response_code == net::HTTP_OK);
    status_code = metadata.http_response_code;
  }

  std::string image_data;
  if (success && response_body) {
    image_data = std::move(*response_body);
  }
  FinishRequest(source, metadata, image_data);

  ImageFetcherMetricsReporter::ReportRequestStatusCode(params.uma_client_name(),
                                                       status_code);
}

void ImageDataFetcher::FinishRequest(const network::SimpleURLLoader* source,
                                     const RequestMetadata& metadata,
                                     const std::string& image_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto request_iter = pending_requests_.find(source);
  CHECK(request_iter != pending_requests_.end(), base::NotFatalUntil::M130);
  auto callback = std::move(request_iter->second->callback);
  pending_requests_.erase(request_iter);
  std::move(callback).Run(image_data, metadata);
  // |this| might be destroyed now.
}

void ImageDataFetcher::InjectResultForTesting(const RequestMetadata& metadata,
                                              const std::string& image_data) {
  DCHECK_EQ(pending_requests_.size(), 1u);
  FinishRequest(pending_requests_.begin()->first, metadata, image_data);
}

}  // namespace image_fetcher
