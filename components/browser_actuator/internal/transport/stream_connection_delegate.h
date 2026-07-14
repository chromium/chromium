// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_STREAM_CONNECTION_DELEGATE_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_STREAM_CONNECTION_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace browser_actuator {

// Strategy interface for how a stream connection is prepared and how it
// communicates resume state, keeping that policy out of the connection
// machinery of MessageStreamClient implementations.
//
// The wire protocol has no built-in resume mechanism: resume state, if
// any, lives entirely inside message payloads, extracted in
// OnMessageDispatched and echoed back through request decoration in
// PrepareRequest. Delegates compose: an auth decorator can wrap a
// resume-state delegate.
//
// Note on resume durability: a resume token is only as durable as the
// server's replay buffer. The envelope protocol should carry an explicit
// resync/epoch signal so features can recover via a full state fetch when
// replay is no longer possible.
class StreamConnectionDelegate {
 public:
  // Receives the request for a connection attempt, or nullptr to abort the
  // attempt.
  using PrepareRequestCallback =
      base::OnceCallback<void(std::unique_ptr<network::ResourceRequest>)>;

  virtual ~StreamConnectionDelegate() = default;

  // Called for every dispatched message (one serialized proto), before
  // observers see it. Gives the delegate a chance to record
  // payload-derived resume state.
  virtual void OnMessageDispatched(const std::string& message) {}

  // Called when a connection attempt produced a valid stream.
  virtual void OnConnectionEstablished() {}

  // Called before every connection attempt, initial and reconnect alike.
  // Implementations may decorate `request` (resume state, auth headers,
  // URL rewrites) and may complete asynchronously; the attempt proceeds
  // when `callback` is run. Running `callback` with nullptr aborts the
  // attempt. The client makes at most one PrepareRequest call at a time.
  virtual void PrepareRequest(std::unique_ptr<network::ResourceRequest> request,
                              PrepareRequestCallback callback) = 0;

  // Called when a connection attempt is rejected at the HTTP level:
  // `response_code` is the rejected response's status code (which can be
  // 200 if the content type was wrong). Return true to retry with backoff
  // — e.g. after invalidating a stale OAuth token on a 401 — or false
  // (the default) to fail the connection permanently.
  virtual bool ShouldRetryOnHttpFailure(int response_code);
};

// Pass-through delegate for servers that need no request decoration.
class DefaultStreamConnectionDelegate : public StreamConnectionDelegate {
 public:
  void PrepareRequest(std::unique_ptr<network::ResourceRequest> request,
                      PrepareRequestCallback callback) override;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_STREAM_CONNECTION_DELEGATE_H_
