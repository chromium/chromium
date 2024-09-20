// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_error.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash::babelorca {
namespace {

// TODO(b/353974384): Identify an accurate max size.
constexpr int kMaxResponseBodySize = 1024 * 1024;
constexpr char kOauthHeaderTemplate[] = "Authorization: Bearer %s";

}  // namespace

TachyonClientImpl::TachyonClientImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

TachyonClientImpl::~TachyonClientImpl() = default;

void TachyonClientImpl::StartRequest(
    std::unique_ptr<RequestDataWrapper> request_data,
    std::string oauth_token,
    AuthFailureCallback auth_failure_cb) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(request_data->url);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.AddHeaderFromString(
      base::StringPrintf(kOauthHeaderTemplate, oauth_token.c_str()));

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       request_data->annotation_tag);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->AttachStringForUpload(request_data->content_data,
                                        "application/x-protobuf");
  if (request_data->max_retries > 0) {
    const int retry_mode = network::SimpleURLLoader::RETRY_ON_5XX |
                           network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE;
    url_loader_ptr->SetRetryOptions(request_data->max_retries, retry_mode);
  }
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&TachyonClientImpl::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(request_data), std::move(auth_failure_cb)),
      kMaxResponseBodySize);
}

void TachyonClientImpl::OnResponse(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<RequestDataWrapper> request_data,
    AuthFailureCallback auth_failure_cb,
    std::unique_ptr<std::string> response_body) {
  if (url_loader->NetError() != net::OK &&
      url_loader->NetError() != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    std::move(request_data->response_cb)
        .Run(base::unexpected(TachyonRequestError::kNetworkError));
    return;
  }
  if (!url_loader->ResponseInfo() || !url_loader->ResponseInfo()->headers) {
    std::move(request_data->response_cb)
        .Run(base::unexpected(TachyonRequestError::kInternalError));
    return;
  }
  const int response_code =
      url_loader->ResponseInfo()->headers->response_code();
  if (response_code == net::HttpStatusCode::HTTP_UNAUTHORIZED) {
    std::move(auth_failure_cb).Run(std::move(request_data));
    return;
  }
  if (!network::IsSuccessfulStatus(response_code)) {
    std::move(request_data->response_cb)
        .Run(base::unexpected(TachyonRequestError::kHttpError));
    return;
  }
  if (!response_body) {
    std::move(request_data->response_cb)
        .Run(base::unexpected(TachyonRequestError::kInternalError));
    return;
  }
  std::move(request_data->response_cb).Run(std::move(*response_body));
}

}  // namespace ash::babelorca
