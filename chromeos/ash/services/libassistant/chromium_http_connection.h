// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_CHROMIUM_HTTP_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_CHROMIUM_HTTP_CONNECTION_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace ash::libassistant {

// Implements libassistant's HttpConnection.
class ChromiumHttpConnection
    : public assistant_client::HttpConnection,
      public network::mojom::ChunkedDataPipeGetter,
      public network::SimpleURLLoaderStreamConsumer,
      public base::RefCountedThreadSafe<ChromiumHttpConnection> {
 public:
  ChromiumHttpConnection(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                             pending_url_loader_factory,
                         Delegate* delegate);

  ChromiumHttpConnection(const ChromiumHttpConnection&) = delete;
  ChromiumHttpConnection& operator=(const ChromiumHttpConnection&) = delete;

  // assistant_client::HttpConnection implementation:
  void SetRequest(const std::string& url, Method method) override;
  void AddHeader(const std::string& name, const std::string& value) override;
  void SetUploadContent(const std::string& content,
                        const std::string& content_type) override;
  void SetChunkedUploadContentType(const std::string& content_type) override;
  void EnableHeaderResponse() override;
  void EnablePartialResults() override;
  void Start() override;
  void Pause() override;
  void Resume() override;
  void Close() override;
  void UploadData(const std::string& data, bool is_last_chunk) override;

  // network::mojom::ChunkedDataPipeGetter implementation:
  void GetSize(GetSizeCallback get_size_callback) override;
  void StartReading(mojo::ScopedDataPipeProducerHandle pipe) override;

  // network::SimpleURLLoaderStreamConsumer implementation:
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

 protected:
  ~ChromiumHttpConnection() override;

 private:
  friend class base::RefCountedThreadSafe<ChromiumHttpConnection>;

  enum class State {
    NEW,
    STARTED,
    COMPLETED,
    DESTROYED,
  };

  // Send more chunked upload data.
  void SendData();

  // |upload_pipe_| can now receive more data.
  void OnUploadPipeWriteable(MojoResult unused);

  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Callback invoked when the response of the http connection has started.
  void OnResponseStarted(
      const GURL& final_url,
      const network::mojom::URLResponseHead& response_header);

  const raw_ptr<Delegate> delegate_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  State state_ = State::NEW;
  bool has_last_chunk_ = false;
  uint64_t upload_body_size_ = 0;
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // The portion of the body not yet uploaded when doing chunked uploads.
  std::string upload_body_;
  // Current pipe being used to send the |upload_body_| to |url_loader_|.
  mojo::ScopedDataPipeProducerHandle upload_pipe_;
  // Watches |upload_pipe_| for writeability.
  std::unique_ptr<mojo::SimpleWatcher> upload_pipe_watcher_;
  // If non-null, invoked once the size of the upload is known.
  network::mojom::ChunkedDataPipeGetter::GetSizeCallback get_size_callback_;
  mojo::ReceiverSet<network::mojom::ChunkedDataPipeGetter> receiver_set_;

  // Parameters to be set before Start() call.
  GURL url_;
  Method method_ = Method::GET;
  ::net::HttpRequestHeaders headers_;
  std::string upload_content_;
  std::string upload_content_type_;
  std::string chunked_upload_content_type_;
  bool handle_partial_response_ = false;
  bool enable_header_response_ = false;

  // Set to true if the response transfer of the connection is paused.
  bool is_paused_ = false;

  base::OnceClosure on_resume_callback_;
  std::string partial_response_cache_;
};

class ChromiumHttpConnectionFactory
    : public assistant_client::HttpConnectionFactory {
 public:
  explicit ChromiumHttpConnectionFactory(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);

  ChromiumHttpConnectionFactory(const ChromiumHttpConnectionFactory&) = delete;
  ChromiumHttpConnectionFactory& operator=(
      const ChromiumHttpConnectionFactory&) = delete;

  ~ChromiumHttpConnectionFactory() override;

  // assistant_client::HttpConnectionFactory implementation:
  assistant_client::HttpConnection* Create(
      assistant_client::HttpConnection::Delegate* delegate) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_CHROMIUM_HTTP_CONNECTION_H_
