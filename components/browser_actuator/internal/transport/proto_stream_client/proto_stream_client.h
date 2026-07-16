// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROTO_STREAM_CLIENT_PROTO_STREAM_CLIENT_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROTO_STREAM_CLIENT_PROTO_STREAM_CLIENT_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/internal/transport/stream_framer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
namespace mojom {
class URLResponseHead;
}  // namespace mojom
}  // namespace network

namespace browser_actuator {

class StreamConnectionDelegate;

// A MessageStreamClient for OnePlatform-style server-streaming RPCs over
// `alt=proto` (Content-Type: application/x-protobuf): the response body
// is one never-ending StreamBody proto whose top-level length-delimited
// fields are the individual messages.
//
// ProtoStreamClient owns the network request (via network::SimpleURLLoader)
// and hands every received body chunk to a framer minted by the injected
// StreamFramerFactory — RustStreamFramer in production, so C++ never
// interprets the untrusted stream bytes (see //docs/security/rule-of-2.md).
// Complete messages, still serialized protos, are fanned out to observers.
//
// Lifecycle: Connect() starts the stream; the endpoint URL should already
// carry `alt=proto`. When the stream ends or the connection fails at the
// network level, the client reconnects automatically with exponential
// backoff, resuming via the StreamConnectionDelegate. HTTP-level
// rejection (non-200 or wrong content type) and malformed framing
// (framer hardening limits) stop the client until Connect() is called
// again — a rejecting or broken server is not something to retry
// automatically — unless the delegate asks for a retry via
// ShouldRetryOnHttpFailure (e.g. after invalidating a stale OAuth token).
// A received terminal `status` also stops the client: a completed RPC is
// not something to retry automatically either.
//
// A stall watchdog tears down and reconnects streams that go silent for
// the kProtoStreamStallTimeout feature param; the server is expected to
// emit periodic `noop` fields. A zero timeout disables the watchdog.
//
// All methods must be called on the owning sequence. Observers may call
// Disconnect() or Connect() from OnStreamMessage(), but must not destroy
// the ProtoStreamClient synchronously from a notification. A Disconnect()
// from inside a notification stops all further delivery at that
// notification's boundary — including any terminal status framed in the
// same chunk. (An observer that wants the status should simply not
// disconnect: a delivered terminal status already stops the client.)
// A Disconnect() followed by Connect() from the same notification is a
// restart: the teardown still runs at the notification boundary, and a
// fresh connection starts right after it.
//
// TODO(crbug.com/534574559): Observe NetworkChangeNotifier: reconnect
// immediately on a network change instead of waiting out the backoff, and
// suspend reconnect attempts entirely while offline.
class ProtoStreamClient : public MessageStreamClient,
                          public network::SimpleURLLoaderStreamConsumer {
 public:
  // `delegate` decides how the request is decorated on (re)connect and how
  // resume state is communicated. Delegates are policy hooks, not
  // observers: they receive no reference to this client and must not
  // reenter it (which is why delegate calls sit outside the
  // notifying_observers_ guards). `framer_factory` mints the framer that
  // interprets the untrusted stream bytes; a fresh framer is created per
  // connection. `traffic_annotation` documents the owning feature's
  // traffic; it is provided by the embedder because this transport is
  // feature-agnostic.
  ProtoStreamClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL endpoint,
      std::unique_ptr<StreamConnectionDelegate> delegate,
      StreamFramerFactory framer_factory,
      net::NetworkTrafficAnnotationTag traffic_annotation);
  ProtoStreamClient(const ProtoStreamClient&) = delete;
  ProtoStreamClient& operator=(const ProtoStreamClient&) = delete;
  ~ProtoStreamClient() override;

  // MessageStreamClient:
  void Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // network::SimpleURLLoaderStreamConsumer:
  void OnDataReceived(std::string_view chunk,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start) override;

 private:
  void StartRequest();
  // Completion of the delegate's (possibly asynchronous) PrepareRequest.
  void OnRequestPrepared(std::unique_ptr<network::ResourceRequest> request);
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);
  // Handles an HTTP-level rejection of a connection attempt: retries with
  // backoff if the delegate asks for it, otherwise fails permanently.
  void HandleHttpRejection(int response_code);
  // The stream produced no bytes for the stall timeout; assume it is
  // dead.
  void OnStallTimeout();
  void ScheduleReconnect();
  // Tears down the connection and any pending reconnect. Notifies
  // observers if the client was connected.
  void TearDown();
  void SetConnected(bool connected);
  // Runs a teardown that an observer requested (via Disconnect()) from
  // inside a notification, once that notification has completed. Returns
  // true if the client tore down.
  bool RunDeferredTeardown();

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL endpoint_;
  const std::unique_ptr<StreamConnectionDelegate> delegate_;
  const StreamFramerFactory framer_factory_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Never notified reentrantly: a Disconnect() from inside a
  // notification is deferred to that notification's boundary (see
  // teardown_deferred_), so no observer ever sees the disconnect state
  // change before the in-flight message.
  base::ObserverList<Observer> observers_;

  bool connected_ = false;

  // True while observers are being notified. A Disconnect() arriving
  // during a notification is recorded in teardown_deferred_ and runs at
  // the notification boundary instead of tearing the client down (and
  // notifying observers again) mid fan-out.
  bool notifying_observers_ = false;
  bool teardown_deferred_ = false;

  // True when Connect() was called while a teardown was deferred: the
  // observer asked for a restart. Consumed (or cancelled by a later
  // Disconnect()) with the deferred teardown.
  bool connect_after_teardown_ = false;

  // True while a delegate hook is being called. Delegates must not
  // reenter the client (see the constructor comment); Connect() and
  // Disconnect() DCHECK against it.
  bool calling_delegate_ = false;

  // True while waiting for the delegate's PrepareRequest callback.
  bool preparing_request_ = false;

  // A terminal `status` field arrived: the RPC completed, so the stream's
  // end must not trigger a reconnect.
  bool status_received_ = false;

  // Number of consecutive connection attempts that failed before producing
  // a valid stream; drives exponential backoff.
  int consecutive_failed_attempts_ = 0;

  // Live while a request is in flight or streaming.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Frames the untrusted stream bytes; exists while a valid stream is
  // open. Framing state pending at end-of-stream was never completed and
  // is discarded with the framer.
  std::unique_ptr<StreamFramer> framer_;

  base::OneShotTimer reconnect_timer_;
  base::OneShotTimer stall_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Invalidated on teardown to cancel in-flight PrepareRequest callbacks.
  base::WeakPtrFactory<ProtoStreamClient> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROTO_STREAM_CLIENT_PROTO_STREAM_CLIENT_H_
