// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPEECH_UPSTREAM_LOADER_H_
#define COMPONENTS_SPEECH_UPSTREAM_LOADER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"

namespace speech {

class UpstreamLoaderClient;

// Maximum amount of data written per Mojo write.
const size_t kMaxUploadWrite = 128 * 1024;

// Streams sound data up to the server. Buffers entire request body into memory,
// so it can be replayed in the case of redirects or retries.
class UpstreamLoader : public network::mojom::ChunkedDataPipeGetter {
 public:
  UpstreamLoader(std::unique_ptr<network::ResourceRequest> resource_request,
                 net::NetworkTrafficAnnotationTag upstream_traffic_annotation,
                 network::mojom::URLLoaderFactory* url_loader_factory,
                 UpstreamLoaderClient* upstream_loader_client);
  UpstreamLoader(const UpstreamLoader&) = delete;
  UpstreamLoader& operator=(const UpstreamLoader&) = delete;
  ~UpstreamLoader() override;

  void SendData();

  void AppendChunkToUpload(const std::string& data, bool is_last_chunk);

 private:
  void OnUploadPipeWriteable(MojoResult unused);
  void OnComplete(std::unique_ptr<std::string> response_body);

  // mojom::ChunkedDataPipeGetter implementation:
  void GetSize(GetSizeCallback get_size_callback) override;
  void StartReading(mojo::ScopedDataPipeProducerHandle pipe) override;

  // Partial upload body. Have to cache the entire thing in memory, in case have
  // to replay it.
  std::string upload_body_;

  // Current position in |upload_body_|.  All bytes before this point have been
  // written to |upload_pipe_|.
  size_t upload_position_ = 0;

  // Whether |upload_body_| is complete.
  bool has_last_chunk_ = false;

  // Current pipe being used to send the |upload_body_| to the URLLoader.
  mojo::ScopedDataPipeProducerHandle upload_pipe_;

  // Watches |upload_pipe_| for writeability.
  std::unique_ptr<mojo::SimpleWatcher> upload_pipe_watcher_;

  // If non-null, invoked once the size of the upload is known.
  network::mojom::ChunkedDataPipeGetter::GetSizeCallback get_size_callback_;

  // The UpstreamLoaderClient must outlive the UpstreamLoader.
  const raw_ptr<UpstreamLoaderClient> upstream_loader_client_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  mojo::ReceiverSet<network::mojom::ChunkedDataPipeGetter> receiver_set_;
};

}  // namespace speech

#endif  // COMPONENTS_SPEECH_UPSTREAM_LOADER_H_
