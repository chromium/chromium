// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/server/bloom_url_loader_impl.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace chromeos {
namespace bloom {

namespace {

// TODO(jeroendh) Investigate how big the actual images are (and potentially
// improve compression by using JPEG).
constexpr int kMaxImageSizeInBytes = 5 * 1024 * 1024;

int GetResponseCode(const network::SimpleURLLoader& simple_loader) {
  if (simple_loader.ResponseInfo() && simple_loader.ResponseInfo()->headers) {
    return simple_loader.ResponseInfo()->headers->response_code();
  }

  return -1;
}

// Transient stack object that creates and sends a request to the given URL.
// Can safely be deleted at any time.
class HttpRequest {
 public:
  using Callback = network::SimpleURLLoader::BodyAsStringCallback;

  explicit HttpRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory),
        request_(std::make_unique<network::ResourceRequest>()) {
    request()->credentials_mode = network::mojom::CredentialsMode::kOmit;
  }

  HttpRequest(HttpRequest&) = delete;
  HttpRequest& operator=(HttpRequest&) = delete;
  ~HttpRequest() = default;

  HttpRequest& SetURL(const GURL& url) {
    request()->url = GURL(url);
    return *this;
  }

  HttpRequest& SetMethod(const char* method) {
    request()->method = method;
    return *this;
  }

  HttpRequest& SetAccessToken(const std::string& access_token) {
    return AddHeader("Authorization", "Bearer " + access_token);
  }

  HttpRequest& AddHeader(base::StringPiece name, base::StringPiece value) {
    request()->headers.SetHeader(name, value);
    return *this;
  }

  HttpRequest& SetBody(std::string&& body, const std::string& mime_type) {
    DCHECK(!request_body_.has_value());
    request_body_ = std::move(body);
    mime_type_ = mime_type;
    return *this;
  }

  void Send(Callback callback) {
    auto simple_loader = network::SimpleURLLoader::Create(
        std::move(request_), NO_TRAFFIC_ANNOTATION_YET);

    if (request_body_) {
      DCHECK(mime_type_);
      simple_loader->AttachStringForUpload(request_body_.value(),
                                           mime_type_.value());
    }

    auto* loader_ptr = simple_loader.get();
    loader_ptr->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(HttpRequest::OnDownloadCompleted, std::move(callback),
                       std::move(simple_loader)),
        kMaxImageSizeInBytes);
  }

 private:
  network::ResourceRequest* request() { return request_.get(); }

  static void OnDownloadCompleted(
      Callback callback,
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      std::unique_ptr<std::string> response_body) {
    if (url_loader->NetError() != net::OK) {
      LOG(WARNING) << "Error contacting Bloom server: "
                   << net::ErrorToString(url_loader->NetError())
                   << ", error code " << GetResponseCode(*url_loader);
    }

    std::move(callback).Run(std::move(response_body));
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::ResourceRequest> request_;

  base::Optional<std::string> request_body_;
  base::Optional<std::string> mime_type_;
};

class PostRequest : public HttpRequest {
 public:
  explicit PostRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : HttpRequest(std::move(url_loader_factory)) {
    SetMethod(net::HttpRequestHeaders::kPostMethod);
  }
};

class GetRequest : public HttpRequest {
 public:
  explicit GetRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : HttpRequest(std::move(url_loader_factory)) {
    SetMethod(net::HttpRequestHeaders::kGetMethod);
  }
};

}  // namespace

BloomURLLoaderImpl::BloomURLLoaderImpl(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(network::SharedURLLoaderFactory::Create(
          std::move(url_loader_factory))),
      weak_ptr_factory_(this) {}

BloomURLLoaderImpl::~BloomURLLoaderImpl() = default;

void BloomURLLoaderImpl::SendPostRequest(const GURL& url,
                                         const std::string& access_token,
                                         std::string&& body,
                                         const std::string& mime_type,
                                         Callback callback) {
  PostRequest(url_loader_factory_)
      .SetURL(url)
      .SetAccessToken(access_token)
      .SetBody(std::move(body), mime_type)
      .Send(base::BindOnce(&BloomURLLoaderImpl::OnServerResponse,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void BloomURLLoaderImpl::SendGetRequest(const GURL& url,
                                        const std::string& access_token,
                                        Callback callback) {
  GetRequest(url_loader_factory_)
      .SetURL(url)
      .SetAccessToken(access_token)
      .Send(base::BindOnce(&BloomURLLoaderImpl::OnServerResponse,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void BloomURLLoaderImpl::OnServerResponse(
    Callback callback,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::move(callback).Run(std::move(*response_body));
}

}  // namespace bloom
}  // namespace chromeos
