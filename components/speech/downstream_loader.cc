// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/speech/downstream_loader.h"

#include <string_view>

#include "base/functional/callback.h"
#include "components/speech/downstream_loader_client.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace speech {

DownstreamLoader::DownstreamLoader(
    std::unique_ptr<network::ResourceRequest> resource_request,
    net::NetworkTrafficAnnotationTag upstream_traffic_annotation,
    network::mojom::URLLoaderFactory* url_loader_factory,
    DownstreamLoaderClient* downstream_loader_client)
    : downstream_loader_client_(downstream_loader_client) {
  DCHECK(downstream_loader_client_);
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), upstream_traffic_annotation);
  simple_url_loader_->DownloadAsStream(url_loader_factory, this);
}

DownstreamLoader::~DownstreamLoader() = default;

void DownstreamLoader::OnDataReceived(std::string_view string_piece,
                                      base::OnceClosure resume) {
  downstream_loader_client_->OnDownstreamDataReceived(string_piece);
  std::move(resume).Run();
}

void DownstreamLoader::OnComplete(bool success) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }

  downstream_loader_client_->OnDownstreamDataComplete(success, response_code);
}

void DownstreamLoader::OnRetry(base::OnceClosure start_retry) {
  // Retries are not enabled for these requests.
  NOTREACHED_IN_MIGRATION();
}

}  // namespace speech
