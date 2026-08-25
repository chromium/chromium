// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_security_domains_url_loader_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "components/trusted_vault/test/fake_security_domains_server.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

namespace trusted_vault {

namespace {

net::test_server::HttpMethod ConvertHttpMethod(std::string_view method) {
  if (base::EqualsCaseInsensitiveASCII(method, "GET")) {
    return net::test_server::METHOD_GET;
  }
  if (base::EqualsCaseInsensitiveASCII(method, "POST")) {
    return net::test_server::METHOD_POST;
  }
  if (base::EqualsCaseInsensitiveASCII(method, "PATCH")) {
    return net::test_server::METHOD_PATCH;
  }
  if (base::EqualsCaseInsensitiveASCII(method, "PUT")) {
    return net::test_server::METHOD_PUT;
  }
  if (base::EqualsCaseInsensitiveASCII(method, "DELETE")) {
    return net::test_server::METHOD_DELETE;
  }
  if (base::EqualsCaseInsensitiveASCII(method, "HEAD")) {
    return net::test_server::METHOD_HEAD;
  }
  return net::test_server::METHOD_UNKNOWN;
}

class ResponseExtractor : public net::test_server::HttpResponseDelegate {
 public:
  ResponseExtractor() = default;
  ~ResponseExtractor() override = default;

  void AddResponse(
      std::unique_ptr<net::test_server::HttpResponse> response) override {
    response_ = std::move(response);
  }

  void SendResponseHeaders(net::HttpStatusCode status,
                           std::string_view status_reason,
                           const base::StringPairs& headers) override {
    status_ = status;
  }

  void SendRawResponseHeaders(std::string_view headers) override {}

  void SendContents(std::string_view contents,
                    base::OnceClosure callback) override {
    content_.append(contents);
    std::move(callback).Run();
  }

  void FinishResponse() override {}

  void SendContentsAndFinish(std::string_view contents) override {
    content_.append(contents);
  }

  void SendHeadersContentAndFinish(net::HttpStatusCode status,
                                   std::string_view status_reason,
                                   const base::StringPairs& headers,
                                   std::string_view contents) override {
    status_ = status;
    content_.append(contents);
  }

  base::WeakPtr<HttpResponseDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  net::HttpStatusCode status() const { return status_; }
  const std::string& content() const { return content_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> response_;
  net::HttpStatusCode status_ = net::HTTP_OK;
  std::string content_;
  base::WeakPtrFactory<ResponseExtractor> weak_ptr_factory_{this};
};

}  // namespace

FakeSecurityDomainsURLLoaderFactory::FakeSecurityDomainsURLLoaderFactory(
    FakeSecurityDomainsServer* server,
    const GURL& server_url)
    : server_(server),
      server_url_(server_url),
      test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()) {
  CHECK(server_);
  test_url_loader_factory_->SetInterceptor(base::BindRepeating(
      &FakeSecurityDomainsURLLoaderFactory::OnInterceptRequest,
      base::Unretained(this)));
}

FakeSecurityDomainsURLLoaderFactory::~FakeSecurityDomainsURLLoaderFactory() =
    default;

void FakeSecurityDomainsURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  test_url_loader_factory_->CreateLoaderAndStart(
      std::move(loader), request_id, options, request, std::move(client),
      traffic_annotation);
}

void FakeSecurityDomainsURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  test_url_loader_factory_->Clone(std::move(receiver));
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
FakeSecurityDomainsURLLoaderFactory::Clone() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  test_url_loader_factory_->Clone(
      pending_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
      std::move(pending_remote));
}

void FakeSecurityDomainsURLLoaderFactory::OnInterceptRequest(
    const network::ResourceRequest& request) {
  net::test_server::HttpRequest http_request;
  http_request.base_url = server_url_.GetWithEmptyPath();
  http_request.relative_url = request.url.PathForRequest();
  http_request.method = ConvertHttpMethod(request.method);
  http_request.content = network::GetUploadData(request);

  std::unique_ptr<net::test_server::HttpResponse> http_response =
      server_->HandleRequest(http_request);

  if (!http_response) {
    test_url_loader_factory_->AddResponse(
        request.url, network::CreateURLResponseHead(net::HTTP_NOT_FOUND), "",
        network::URLLoaderCompletionStatus(net::OK));
    return;
  }

  ResponseExtractor extractor;
  http_response->SendResponse(extractor.AsWeakPtr());

  test_url_loader_factory_->AddResponse(
      request.url, network::CreateURLResponseHead(extractor.status()),
      extractor.content(), network::URLLoaderCompletionStatus(net::OK));
}

}  // namespace trusted_vault
