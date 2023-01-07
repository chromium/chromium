// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_fetcher.h"

#include "components/content_creation/notes/core/templates/template_metrics.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace content_creation {
namespace {
constexpr size_t kMaxBoundedStringDownloadSize = 5 * 1024 * 1024;

net::NetworkTrafficAnnotationTag GetTemplateTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("pull_template_request", R"(
        semantics {
          sender: "Note Creation Component"
          description:
            "Chrome provides the ability to create a note about web content "
            "and offers the user different template options on how the note"
            "will look. This request fetches the templates from Google servers."
          trigger:
            "User presses on the option to create note after they have"
            "clicked share on highlighted text (Android only)."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable dynamic templates by toggling"
            "chrome://flags/#webnotes-dynamic-templates."
          policy_exception_justification: "No policy affecting this feature "
            "at the moment."
        })");
}
}  // namespace

TemplateFetcher::TemplateFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  url_loader_factory_ = std::move(loader_factory);
}

TemplateFetcher::~TemplateFetcher() = default;

void TemplateFetcher::Start(FetchTemplateCompleteCallback callback) {
  url_loader_ = network::SimpleURLLoader::Create(
      CreateTemplateResourceRequest(), GetTemplateTrafficAnnotation());
  url_loader_->SetTimeoutDuration(base::Milliseconds(500));
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&TemplateFetcher::OnTemplateFetchComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      kMaxBoundedStringDownloadSize);
}

void TemplateFetcher::OnTemplateFetchComplete(
    FetchTemplateCompleteCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::string template_string = "";
  if (response_body != nullptr) {
    template_string = *response_body;
  }
  content_creation::LogTemplateFetcherMetrics(!template_string.empty());
  std::move(callback).Run(template_string);
}

std::unique_ptr<network::ResourceRequest>
TemplateFetcher::CreateTemplateResourceRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kTemplateUrl);
  resource_request->method = net::HttpRequestHeaders::kGetMethod;

  return resource_request;
}

}  // namespace content_creation
