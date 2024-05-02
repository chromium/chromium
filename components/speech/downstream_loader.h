// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPEECH_DOWNSTREAM_LOADER_H_
#define COMPONENTS_SPEECH_DOWNSTREAM_LOADER_H_

#include <memory>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace speech {

class DownstreamLoaderClient;

// Streams response data from the server to the DownstreamLoaderClient.
class DownstreamLoader : public network::SimpleURLLoaderStreamConsumer {
 public:
  DownstreamLoader(std::unique_ptr<network::ResourceRequest> resource_request,
                   net::NetworkTrafficAnnotationTag upstream_traffic_annotation,
                   network::mojom::URLLoaderFactory* url_loader_factory,
                   DownstreamLoaderClient* downstream_loader_client);
  DownstreamLoader(const DownstreamLoader&) = delete;
  DownstreamLoader& operator=(const DownstreamLoader&) = delete;
  ~DownstreamLoader() override;

  // SimpleURLLoaderStreamConsumer implementation:
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

 private:
  // The DownstreamLoaderClient must outlive the DownstreamLoader.
  const raw_ptr<DownstreamLoaderClient> downstream_loader_client_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace speech

#endif  // COMPONENTS_SPEECH_DOWNSTREAM_LOADER_H_
