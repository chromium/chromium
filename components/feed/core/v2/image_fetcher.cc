// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/image_fetcher.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/public/types.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace feed {

ImageFetcher::ImageFetcher(
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

ImageFetcher::~ImageFetcher() = default;

ImageFetchId ImageFetcher::Fetch(const GURL& url, ImageCallback callback) {
  ImageFetchId id = id_generator_.GenerateNextId();
  TRACE_EVENT_BEGIN("android.ui.jank", "FeedImage",
                    perfetto::Track(GetTrackId(id)), "url", url);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("interest_feedv2_image_send", R"(
        semantics {
          sender: "Feed Library"
          description: "Images for articles in the feed."
          trigger: "Triggered when viewing the feed on the NTP."
          data: "Request for an image containing an ID for the image and "
          "device specs (e.g. screen size) for resizing images."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This can be disabled from the New Tab Page by collapsing "
          "the articles section."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<::network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  auto* const simple_loader_ptr = simple_loader.get();

  bool inserted =
      pending_requests_
          .try_emplace(id, std::move(simple_loader), std::move(callback))
          .second;
  DCHECK(inserted);

  simple_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ImageFetcher::OnFetchComplete, weak_factory_.GetWeakPtr(),
                     id, url),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
  return id;
}

void ImageFetcher::OnFetchComplete(ImageFetchId id,
                                   const GURL& url,
                                   std::unique_ptr<std::string> response_data) {
  TRACE_EVENT_END("android.ui.jank", perfetto::Track(GetTrackId(id)), "bytes",
                  response_data ? response_data->size() : 0);
  std::optional<PendingRequest> request = RemovePending(id);
  if (!request)
    return;

  NetworkResponse response;
  if (request->loader->ResponseInfo() &&
      request->loader->ResponseInfo()->headers) {
    response.status_code =
        request->loader->ResponseInfo()->headers->response_code();
  } else {
    response.status_code = request->loader->NetError();
  }
  MetricsReporter::OnImageFetched(url, response.status_code);

  if (response_data)
    response.response_bytes = std::move(*response_data);
  std::move(request->callback).Run(std::move(response));
}

void ImageFetcher::Cancel(ImageFetchId id) {
  std::optional<PendingRequest> request = RemovePending(id);
  if (!request)
    return;

  // Cancel the fetch before running the callback.
  request->loader.reset();
  std::move(request->callback)
      .Run({/*response_bytes=*/std::string(), net::Error::ERR_ABORTED});
}

std::optional<ImageFetcher::PendingRequest> ImageFetcher::RemovePending(
    ImageFetchId id) {
  auto iterator = pending_requests_.find(id);
  if (iterator == pending_requests_.end())
    return std::nullopt;

  auto request = std::make_optional(std::move(iterator->second));
  pending_requests_.erase(iterator);
  return request;
}

uint64_t ImageFetcher::GetTrackId(ImageFetchId id) const {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)) +
         id.GetUnsafeValue();
}

ImageFetcher::PendingRequest::PendingRequest(
    std::unique_ptr<network::SimpleURLLoader> loader,
    ImageCallback callback)
    : loader(std::move(loader)), callback(std::move(callback)) {}
ImageFetcher::PendingRequest::PendingRequest(
    ImageFetcher::PendingRequest&& other) = default;
ImageFetcher::PendingRequest& ImageFetcher::PendingRequest::operator=(
    ImageFetcher::PendingRequest&& other) = default;
ImageFetcher::PendingRequest::~PendingRequest() = default;

}  // namespace feed
