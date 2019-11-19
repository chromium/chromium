// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

// Content type needed in order to communicate with the server in binary
// proto format.
const char kRequestContentType[] = "application/x-protobuf";

}  // namespace

// static
std::unique_ptr<PrefetchRequestFetcher> PrefetchRequestFetcher::CreateForGet(
    const GURL& url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FinishedCallback callback) {
  return base::WrapUnique(
      new PrefetchRequestFetcher(url, std::string(), std::string(), false,
                                 url_loader_factory, std::move(callback)));
}

// static
std::unique_ptr<PrefetchRequestFetcher> PrefetchRequestFetcher::CreateForPost(
    const GURL& url,
    const std::string& message,
    const std::string& testing_header_value,
    bool empty_request,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FinishedCallback callback) {
  return base::WrapUnique(new PrefetchRequestFetcher(
      url, message, testing_header_value, empty_request, url_loader_factory,
      std::move(callback)));
}

PrefetchRequestFetcher::PrefetchRequestFetcher(
    const GURL& url,
    const std::string& message,
    const std::string& testing_header_value,
    bool empty_request,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FinishedCallback callback)
    : empty_request_(empty_request),
      url_loader_factory_(url_loader_factory),
      callback_(std::move(callback)) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("offline_prefetch", R"(
        semantics {
          sender: "Offline Prefetch"
          description:
            "Chromium interacts with Offline Page Service to prefetch "
            "suggested website resources."
          trigger:
            "When there are suggested website resources to fetch."
          data:
            "URLs of the suggested website resources to fetch."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable offline prefetch by toggling "
            "'Download articles for you' in settings under Downloads or "
            "by toggling chrome://flags#offline-prefetch."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = message.empty() ? "GET" : "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::string experiment_header = PrefetchExperimentHeader();
  if (!experiment_header.empty())
    resource_request->headers.AddHeaderFromString(experiment_header);

  if (!testing_header_value.empty())
    resource_request->headers.SetHeader(kPrefetchTestingHeaderName,
                                        testing_header_value);

  if (message.empty())
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        kRequestContentType);

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->SetAllowHttpErrorResults(true);

  if (!message.empty())
    url_loader_->AttachStringForUpload(message, kRequestContentType);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PrefetchRequestFetcher::OnURLLoadComplete,
                     base::Unretained(this)));
}

PrefetchRequestFetcher::~PrefetchRequestFetcher() {}

void PrefetchRequestFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  std::string data;
  PrefetchRequestStatus status = ParseResponse(std::move(response_body), &data);

  // TODO(jianli): Report UMA.

  std::move(callback_).Run(status, data);
}

PrefetchRequestStatus PrefetchRequestFetcher::ParseResponse(
    std::unique_ptr<std::string> response_body,
    std::string* data) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  if ((response_code < 200 || response_code > 299) && response_code != -1) {
    DVLOG(1) << "HTTP status: " << response_code;
    switch (response_code) {
      case net::HTTP_NOT_IMPLEMENTED:
        return PrefetchRequestStatus::kShouldSuspendNotImplemented;
      case net::HTTP_FORBIDDEN:
        // Check whether the request was forbidden because of a filter rule.
        if (response_body && response_body->find("request forbidden by OPS") !=
                                 std::string::npos) {
          if (!empty_request_)
            return PrefetchRequestStatus::kShouldSuspendNewlyForbiddenByOPS;
          return PrefetchRequestStatus::kShouldSuspendForbiddenByOPS;
        }
        return PrefetchRequestStatus::kShouldSuspendForbidden;
      default:
        return PrefetchRequestStatus::kShouldRetryWithBackoff;
    }
  }
  if (!response_body) {
    int net_error = url_loader_->NetError();
    DVLOG(1) << "Net error: " << net_error;
    return (net_error == net::ERR_BLOCKED_BY_ADMINISTRATOR)
               ? PrefetchRequestStatus::kShouldSuspendBlockedByAdministrator
               : PrefetchRequestStatus::kShouldRetryWithoutBackoff;
  }

  if (response_body->empty()) {
    DVLOG(1) << "Failed to get response or empty response";
    return PrefetchRequestStatus::kShouldRetryWithBackoff;
  }

  *data = *response_body;

  if (empty_request_)
    return PrefetchRequestStatus::kEmptyRequestSuccess;
  return PrefetchRequestStatus::kSuccess;
}

}  // namespace offline_pages
