// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/net/sync_server_connection_manager.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "components/sync/base/cancelation_observer.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/http_post_provider_interface.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace syncer {
namespace {

std::string StripTrailingSlash(const std::string& s) {
  int stripped_end_pos = s.size();
  if (s.at(stripped_end_pos - 1) == '/') {
    stripped_end_pos = stripped_end_pos - 1;
  }

  return s.substr(0, stripped_end_pos);
}

// TODO(crbug.com/951350): Use a GURL instead of string concatenation.
std::string MakeConnectionURL(const std::string& sync_server,
                              const std::string& path,
                              bool use_ssl) {
  std::string connection_url = (use_ssl ? "https://" : "http://");
  connection_url += sync_server;
  connection_url = StripTrailingSlash(connection_url);
  connection_url += path;
  return connection_url;
}

// This provides HTTP Post functionality through the interface provided
// by the application hosting the syncer backend.
class Connection : public CancelationObserver {
 public:
  // All pointers must not be null and must outlive this object.
  Connection(HttpPostProviderFactory* factory,
             CancelationSignal* cancelation_signal);
  ~Connection() override;

  // TODO(crbug.com/951350): Return the HttpResponse by value. It's not
  // obvious what the boolean return value means. (True means success or HTTP
  // error, false means canceled or network error.)
  bool Init(const std::string& connection_url,
            int sync_server_port,
            const std::string& access_token,
            const std::string& payload,
            HttpResponse* response);
  bool ReadBufferResponse(std::string* buffer_out,
                          HttpResponse* response,
                          bool require_response);
  bool ReadDownloadResponse(HttpResponse* response, std::string* buffer_out);

  // CancelationObserver overrides.
  void OnSignalReceived() override;

 private:
  int ReadResponse(std::string* out_buffer, int length) const;

  // Pointer to the factory we use for creating HttpPostProviders. We do not
  // own |factory_|.
  HttpPostProviderFactory* const factory_;

  // Cancelation signal is signalled when engine shuts down. Current blocking
  // operation should be aborted.
  CancelationSignal* const cancelation_signal_;

  HttpPostProviderInterface* const post_provider_;

  std::string buffer_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

Connection::Connection(HttpPostProviderFactory* factory,
                       CancelationSignal* cancelation_signal)
    : factory_(factory),
      cancelation_signal_(cancelation_signal),
      post_provider_(factory_->Create()) {
  DCHECK(factory);
  DCHECK(cancelation_signal);
  DCHECK(post_provider_);
}

Connection::~Connection() {
  factory_->Destroy(post_provider_);
}

bool Connection::Init(const std::string& connection_url,
                      int sync_server_port,
                      const std::string& access_token,
                      const std::string& payload,
                      HttpResponse* response) {
  std::string sync_server;

  HttpPostProviderInterface* http = post_provider_;
  http->SetURL(connection_url.c_str(), sync_server_port);

  if (!access_token.empty()) {
    std::string headers;
    headers = "Authorization: Bearer " + access_token;
    http->SetExtraRequestHeaders(headers.c_str());
  }

  // Must be octet-stream, or the payload may be parsed for a cookie.
  http->SetPostPayload("application/octet-stream", payload.length(),
                       payload.data());

  // Issue the POST, blocking until it finishes.
  int net_error_code = 0;
  int http_status_code = 0;
  if (!cancelation_signal_->TryRegisterHandler(this)) {
    // Return early because cancelation signal was signaled.
    // TODO(crbug.com/951350): Introduce an extra status code for canceled?
    response->server_status = HttpResponse::CONNECTION_UNAVAILABLE;
    return false;
  }
  base::ScopedClosureRunner auto_unregister(base::BindOnce(
      &CancelationSignal::UnregisterHandler,
      base::Unretained(cancelation_signal_), base::Unretained(this)));

  if (!http->MakeSynchronousPost(&net_error_code, &http_status_code)) {
    DCHECK_NE(net_error_code, net::OK);
    DVLOG(1) << "Http POST failed, error returns: " << net_error_code;
    response->server_status = HttpResponse::CONNECTION_UNAVAILABLE;
    response->net_error_code = net_error_code;
    return false;
  }

  // We got a server response, copy over response codes and content.
  response->http_status_code = http_status_code;
  response->content_length =
      static_cast<int64_t>(http->GetResponseContentLength());
  response->payload_length =
      static_cast<int64_t>(http->GetResponseContentLength());
  if (response->http_status_code == net::HTTP_OK)
    response->server_status = HttpResponse::SERVER_CONNECTION_OK;
  else if (response->http_status_code == net::HTTP_UNAUTHORIZED)
    response->server_status = HttpResponse::SYNC_AUTH_ERROR;
  else
    response->server_status = HttpResponse::SYNC_SERVER_ERROR;

  // Write the content into our buffer.
  buffer_.assign(http->GetResponseContent(), http->GetResponseContentLength());
  return true;
}

bool Connection::ReadBufferResponse(std::string* buffer_out,
                                    HttpResponse* response,
                                    bool require_response) {
  if (net::HTTP_OK != response->http_status_code) {
    response->server_status = HttpResponse::SYNC_SERVER_ERROR;
    return false;
  }

  if (require_response && (1 > response->content_length))
    return false;

  const int64_t bytes_read =
      ReadResponse(buffer_out, static_cast<int>(response->content_length));
  if (bytes_read != response->content_length) {
    response->server_status = HttpResponse::IO_ERROR;
    return false;
  }
  return true;
}

bool Connection::ReadDownloadResponse(HttpResponse* response,
                                      std::string* buffer_out) {
  const int64_t bytes_read =
      ReadResponse(buffer_out, static_cast<int>(response->content_length));

  if (bytes_read != response->content_length) {
    LOG(ERROR) << "Mismatched content lengths, server claimed "
               << response->content_length << ", but sent " << bytes_read;
    response->server_status = HttpResponse::IO_ERROR;
    return false;
  }
  return true;
}

int Connection::ReadResponse(std::string* out_buffer, int length) const {
  int bytes_read = buffer_.length();
  CHECK_LE(length, bytes_read);
  out_buffer->assign(buffer_);
  return bytes_read;
}

void Connection::OnSignalReceived() {
  DCHECK(post_provider_);
  post_provider_->Abort();
}

}  // namespace

SyncServerConnectionManager::SyncServerConnectionManager(
    const std::string& server,
    int port,
    bool use_ssl,
    std::unique_ptr<HttpPostProviderFactory> factory,
    CancelationSignal* cancelation_signal)
    : sync_server_(server),
      sync_server_port_(port),
      use_ssl_(use_ssl),
      post_provider_factory_(std::move(factory)),
      cancelation_signal_(cancelation_signal) {
  DCHECK(post_provider_factory_);
  DCHECK(cancelation_signal_);
}

SyncServerConnectionManager::~SyncServerConnectionManager() = default;

bool SyncServerConnectionManager::PostBufferToPath(
    PostBufferParams* params,
    const std::string& path,
    const std::string& access_token) {
  if (access_token.empty()) {
    params->response.server_status = HttpResponse::SYNC_AUTH_ERROR;
    // Print a log to distinguish this "known failure" from others.
    DVLOG(1) << "ServerConnectionManager forcing SYNC_AUTH_ERROR due to missing"
                " access token";
    return false;
  }

  if (cancelation_signal_->IsSignalled()) {
    params->response.server_status = HttpResponse::CONNECTION_UNAVAILABLE;
    return false;
  }

  auto connection = std::make_unique<Connection>(post_provider_factory_.get(),
                                                 cancelation_signal_);
  std::string connection_url = MakeConnectionURL(sync_server_, path, use_ssl_);

  // Note that |post| may be aborted by now, which will just cause Init to fail
  // with CONNECTION_UNAVAILABLE.
  bool ok = connection->Init(connection_url, sync_server_port_, access_token,
                             params->buffer_in, &params->response);

  if (params->response.server_status == HttpResponse::SYNC_AUTH_ERROR) {
    ClearAccessToken();
  }

  if (!ok || net::HTTP_OK != params->response.http_status_code)
    return false;

  if (connection->ReadBufferResponse(&params->buffer_out, &params->response,
                                     true)) {
    params->response.server_status = HttpResponse::SERVER_CONNECTION_OK;
    return true;
  }
  return false;
}

}  // namespace syncer
