// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/speech/upstream_loader.h"

#include "base/containers/span.h"
#include "components/speech/upstream_loader_client.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace speech {

UpstreamLoader::UpstreamLoader(
    std::unique_ptr<network::ResourceRequest> resource_request,
    net::NetworkTrafficAnnotationTag upstream_traffic_annotation,
    network::mojom::URLLoaderFactory* url_loader_factory,
    UpstreamLoaderClient* upstream_loader_client)
    : upstream_loader_client_(upstream_loader_client) {
  DCHECK(upstream_loader_client_);
  // Attach a chunked upload body.
  mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> data_remote;
  receiver_set_.Add(this, data_remote.InitWithNewPipeAndPassReceiver());
  resource_request->request_body =
      base::MakeRefCounted<network::ResourceRequestBody>();
  resource_request->request_body->SetAllowHTTP1ForStreamingUpload(true);
  resource_request->request_body->SetToChunkedDataPipe(
      std::move(data_remote),
      network::ResourceRequestBody::ReadOnlyOnce(false));
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), upstream_traffic_annotation);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&UpstreamLoader::OnComplete, base::Unretained(this)));
}

UpstreamLoader::~UpstreamLoader() = default;

// Attempts to send more of the upload body, if more data is available, and
// |upload_pipe_| is valid.
void UpstreamLoader::SendData() {
  DCHECK_LE(upload_position_, upload_body_.size());

  if (!upload_pipe_.is_valid())
    return;

  // Nothing more to write yet, or done writing everything.
  if (upload_position_ == upload_body_.size())
    return;

  // Since kMaxUploadWrite is a uint32_t, no overflow occurs in this downcast.
  base::span<const uint8_t> bytes_to_write =
      base::as_byte_span(upload_body_).subspan(upload_position_);
  bytes_to_write =
      bytes_to_write.first(std::min(bytes_to_write.size(), kMaxUploadWrite));
  size_t actually_written_bytes = 0;
  MojoResult result = upload_pipe_->WriteData(
      bytes_to_write, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);

  // Wait for the pipe to have more capacity available, if needed.
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    upload_pipe_watcher_->ArmOrNotify();
    return;
  }

  // Do nothing on pipe closure - depend on the SimpleURLLoader to notice the
  // other pipes being closed on error. Can reach this point if there's a
  // retry, for instance, so cannot draw any conclusions here.
  if (result != MOJO_RESULT_OK)
    return;

  upload_position_ += actually_written_bytes;
  // If more data is available, arm the watcher again. Don't write again in a
  // loop, even if WriteData would allow it, to avoid blocking the current
  // thread.
  if (upload_position_ < upload_body_.size())
    upload_pipe_watcher_->ArmOrNotify();
}

void UpstreamLoader::AppendChunkToUpload(const std::string& data,
                                         bool is_last_chunk) {
  DCHECK(!has_last_chunk_);

  upload_body_ += data;
  if (is_last_chunk) {
    // Send size before the rest of the body. While it doesn't matter much, if
    // the other side receives the size before the last chunk, which Mojo does
    // not guarantee, some protocols can merge the data and the last chunk
    // itself into a single frame.
    has_last_chunk_ = is_last_chunk;
    if (get_size_callback_)
      std::move(get_size_callback_).Run(net::OK, upload_body_.size());
  }

  SendData();
}

void UpstreamLoader::OnUploadPipeWriteable(MojoResult unused) {
  SendData();
}

void UpstreamLoader::OnComplete(std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  upstream_loader_client_->OnUpstreamDataComplete(response_body != nullptr,
                                                  response_code);
}

void UpstreamLoader::GetSize(GetSizeCallback get_size_callback) {
  if (has_last_chunk_) {
    std::move(get_size_callback).Run(net::OK, upload_body_.size());
  } else {
    get_size_callback_ = std::move(get_size_callback);
  }
}

void UpstreamLoader::StartReading(mojo::ScopedDataPipeProducerHandle pipe) {
  // Delete any existing pipe, if any.
  upload_pipe_watcher_.reset();
  upload_pipe_ = std::move(pipe);
  upload_pipe_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  upload_pipe_watcher_->Watch(
      upload_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&UpstreamLoader::OnUploadPipeWriteable,
                          base::Unretained(this)));
  upload_position_ = 0;

  // Will attempt to start sending the request body, if any data is available.
  SendData();
}

}  // namespace speech
