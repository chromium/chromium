// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/net/sync_server_connection_manager.h"

#include <stdint.h>

#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/net/http_post_provider.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace syncer {
namespace {

// This provides HTTP Post functionality through the interface provided
// by the application hosting the syncer backend.
class Connection : public CancelationSignal::Observer {
 public:
  // All pointers must not be null and must outlive this object.
  Connection(HttpPostProviderFactory* factory,
             CancelationSignal* cancelation_signal);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  ~Connection() override;

  HttpResponse PostRequestAndDownloadResponse(const GURL& connection_url,
                                              const std::string& access_token,
                                              const std::string& payload,
                                              std::string* buffer_out);

  // CancelationSignal::Observer overrides.
  void OnCancelationSignalReceived() override;

 private:
  // Pointer to the factory we use for creating HttpPostProviders. We do not
  // own |factory_|.
  const raw_ptr<HttpPostProviderFactory> factory_;

  // Cancelation signal is signalled when engine shuts down. Current blocking
  // operation should be aborted.
  const raw_ptr<CancelationSignal> cancelation_signal_;

  scoped_refptr<HttpPostProvider> const post_provider_;
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

Connection::~Connection() = default;

HttpResponse Connection::PostRequestAndDownloadResponse(
    const GURL& sync_request_url,
    const std::string& access_token,
    const std::string& payload,
    std::string* buffer_out) {
  post_provider_->SetURL(sync_request_url);

  if (!access_token.empty()) {
    std::string headers;
    headers = "Authorization: Bearer " + access_token;
    post_provider_->SetExtraRequestHeaders(headers.c_str());
  }

  // Must be octet-stream, or the payload may be parsed for a cookie.
  post_provider_->SetPostPayload("application/octet-stream", payload.length(),
                                 payload.data());

  // Issue the POST, blocking until it finishes.
  if (!cancelation_signal_->TryRegisterHandler(this)) {
    // Return early because cancelation signal was signaled.
    return HttpResponse::ForUnspecifiedError();
  }
  absl::Cleanup auto_unregister = [this] {
    cancelation_signal_->UnregisterHandler(this);
  };

  int net_error_code = 0;
  int http_status_code = 0;
  if (!post_provider_->MakeSynchronousPost(&net_error_code,
                                           &http_status_code)) {
    DCHECK_NE(net_error_code, net::OK);
    DVLOG(1) << "Http POST failed, error returns: " << net_error_code;
    return HttpResponse::ForNetError(net_error_code);
  }

  // We got a server response, copy over response codes and content.
  HttpResponse response = HttpResponse::ForHttpStatusCode(http_status_code);
  response.content_length =
      static_cast<int64_t>(post_provider_->GetResponseContentLength());

  // Write the content into the buffer.
  buffer_out->assign(post_provider_->GetResponseContent(),
                     post_provider_->GetResponseContentLength());
  return response;
}

void Connection::OnCancelationSignalReceived() {
  DCHECK(post_provider_);
  post_provider_->Abort();
}

}  // namespace

SyncServerConnectionManager::SyncServerConnectionManager(
    const GURL& sync_request_url,
    std::unique_ptr<HttpPostProviderFactory> factory,
    CancelationSignal* cancelation_signal)
    : sync_request_url_(sync_request_url),
      post_provider_factory_(std::move(factory)),
      cancelation_signal_(cancelation_signal) {
  DCHECK(post_provider_factory_);
  DCHECK(cancelation_signal_);
}

SyncServerConnectionManager::~SyncServerConnectionManager() = default;

HttpResponse SyncServerConnectionManager::PostBuffer(
    const std::string& buffer_in,
    const std::string& access_token,
    std::string* buffer_out) {
  if (access_token.empty()) {
    // Print a log to distinguish this "known failure" from others.
    DVLOG(1) << "ServerConnectionManager forcing SYNC_AUTH_ERROR due to missing"
                " access token";
    return HttpResponse::ForHttpStatusCode(net::HTTP_UNAUTHORIZED);
  }

  if (cancelation_signal_->IsSignalled()) {
    return HttpResponse::ForUnspecifiedError();
  }

  auto connection = std::make_unique<Connection>(post_provider_factory_.get(),
                                                 cancelation_signal_);

  // Note that the post may be aborted by now, which will just cause Init to
  // fail with CONNECTION_UNAVAILABLE.
  HttpResponse http_response = connection->PostRequestAndDownloadResponse(
      sync_request_url_, access_token, buffer_in, buffer_out);

  if (http_response.server_status == HttpResponse::SYNC_AUTH_ERROR) {
    ClearAccessToken();
  }

  return http_response;
}

}  // namespace syncer
