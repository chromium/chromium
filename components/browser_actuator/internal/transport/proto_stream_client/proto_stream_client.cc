// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/proto_stream_client/proto_stream_client.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace browser_actuator {

ProtoStreamClient::ProtoStreamClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL endpoint,
    std::unique_ptr<StreamConnectionDelegate> delegate,
    StreamFramerFactory framer_factory,
    net::NetworkTrafficAnnotationTag traffic_annotation)
    : url_loader_factory_(std::move(url_loader_factory)),
      endpoint_(std::move(endpoint)),
      delegate_(std::move(delegate)),
      framer_factory_(std::move(framer_factory)),
      traffic_annotation_(traffic_annotation) {
  CHECK(delegate_);
  CHECK(framer_factory_);
}

ProtoStreamClient::~ProtoStreamClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProtoStreamClient::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!calling_delegate_);
  if (teardown_deferred_) {
    // Disconnect() followed by Connect() from the same notification: a
    // restart. The deferred teardown still runs at the notification
    // boundary; a fresh connection starts right after it.
    connect_after_teardown_ = true;
    return;
  }
  if (preparing_request_ || loader_ || reconnect_timer_.IsRunning()) {
    return;
  }
  consecutive_failed_attempts_ = 0;
  status_received_ = false;
  StartRequest();
}

void ProtoStreamClient::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!calling_delegate_);
  if (notifying_observers_) {
    // Called from inside an observer notification. Tearing down here
    // would notify observers from within a notification and hand the
    // disconnect state change to the remaining observers before the
    // in-flight message. Defer to the notification boundary instead.
    teardown_deferred_ = true;
    // A Disconnect() after a queued restart cancels the restart.
    connect_after_teardown_ = false;
    return;
  }
  TearDown();
}

bool ProtoStreamClient::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return connected_;
}

void ProtoStreamClient::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ProtoStreamClient::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void ProtoStreamClient::StartRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!loader_);
  CHECK(!connected_);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = endpoint_;
  request->method = "GET";
  // OnePlatform selects the binary framing from the Accept header (in
  // combination with alt=proto in the URL, which the embedder provides).
  request->headers.SetHeader("Accept", "application/x-protobuf");
  request->headers.SetHeader("Cache-Control", "no-cache");
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // The delegate may prepare the request asynchronously (e.g. minting an
  // OAuth token); the weak pointer cancels the attempt if the client is
  // torn down in the meantime.
  preparing_request_ = true;
  base::AutoReset<bool> calling(&calling_delegate_, true);
  delegate_->PrepareRequest(
      std::move(request), base::BindOnce(&ProtoStreamClient::OnRequestPrepared,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void ProtoStreamClient::OnRequestPrepared(
    std::unique_ptr<network::ResourceRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  preparing_request_ = false;

  if (!request) {
    // The delegate aborted the attempt (e.g. no token available). Treat it
    // like a failed connection: retry with backoff.
    ++consecutive_failed_attempts_;
    ScheduleReconnect();
    return;
  }

  // A delegate may supply a body to upload with the connection request (e.g.
  // the resume state); the request then goes out as a POST.
  std::optional<StreamUploadBody> upload =
      delegate_->GetConnectionRequestBody();
  if (upload) {
    request->method = "POST";
  }
  loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  if (upload) {
    loader_->AttachStringForUpload(std::move(upload->content),
                                   std::move(upload->content_type));
  }
  // base::Unretained is safe: `this` owns and outlives `loader_`, and
  // SimpleURLLoader never invokes callbacks during its own destruction.
  loader_->SetOnResponseStartedCallback(base::BindOnce(
      &ProtoStreamClient::OnResponseStarted, base::Unretained(this)));
  loader_->DownloadAsStream(url_loader_factory_.get(), this);
}

void ProtoStreamClient::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int response_code =
      response_head.headers ? response_head.headers->response_code() : 0;
  if (response_code != 200 ||
      response_head.mime_type != "application/x-protobuf") {
    HandleHttpRejection(response_code);
    return;
  }

  consecutive_failed_attempts_ = 0;
  framer_ = framer_factory_.Run();
  {
    base::AutoReset<bool> calling(&calling_delegate_, true);
    delegate_->OnConnectionEstablished();
  }
  const base::TimeDelta stall_timeout = kProtoStreamStallTimeout.Get();
  if (stall_timeout.is_positive()) {
    // base::Unretained is safe: `this` owns `stall_timer_`.
    stall_timer_.Start(FROM_HERE, stall_timeout,
                       base::BindOnce(&ProtoStreamClient::OnStallTimeout,
                                      base::Unretained(this)));
  }
  SetConnected(true);
  RunDeferredTeardown();
}

void ProtoStreamClient::HandleHttpRejection(int response_code) {
  bool should_retry = false;
  {
    base::AutoReset<bool> calling(&calling_delegate_, true);
    should_retry = delegate_->ShouldRetryOnHttpFailure(response_code);
  }
  if (should_retry) {
    stall_timer_.Stop();
    loader_.reset();
    framer_.reset();
    ++consecutive_failed_attempts_;
    ScheduleReconnect();
    return;
  }
  TearDown();
}

void ProtoStreamClient::OnDataReceived(std::string_view chunk,
                                       base::OnceClosure resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(framer_);

  // Any bytes — including framing the consumer never sees, like noop
  // keep-alives — prove the stream is alive. (Not running means the
  // watchdog is disabled by a zero stall timeout.)
  if (stall_timer_.IsRunning()) {
    stall_timer_.Reset();
  }

  // Hand the untrusted bytes to the framer; C++ never interprets them.
  StreamFramer::FeedResult result = framer_->Feed(chunk);

  for (const std::string& message : result.messages) {
    {
      base::AutoReset<bool> calling(&calling_delegate_, true);
      delegate_->OnMessageDispatched(message);
    }
    {
      base::AutoReset<bool> notifying(&notifying_observers_, true);
      for (Observer& observer : observers_) {
        observer.OnStreamMessage(message);
      }
    }
    // An observer may have called Disconnect(). The in-flight message
    // still reached every observer (ahead of the disconnect state
    // change); the remaining messages of this batch belong to a stream
    // the client no longer owns.
    if (RunDeferredTeardown()) {
      return;
    }
  }

  if (result.status.has_value()) {
    // The RPC completed; the server will close the connection. Deliver the
    // status and make sure the close does not trigger a reconnect.
    status_received_ = true;
    {
      base::AutoReset<bool> notifying(&notifying_observers_, true);
      for (Observer& observer : observers_) {
        observer.OnStreamStatus(*result.status);
      }
    }
    if (RunDeferredTeardown()) {
      return;
    }
  }

  if (result.failed) {
    // The framer rejected the stream: it is malformed or hostile. Fail
    // permanently rather than keep talking to a broken server.
    TearDown();
    return;
  }

  std::move(resume).Run();
}

void ProtoStreamClient::OnComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  stall_timer_.Stop();

  if (status_received_) {
    // Clean RPC completion; not something to retry automatically.
    TearDown();
    return;
  }

  const bool had_stream = connected_;
  if (!had_stream) {
    // The attempt never produced a valid stream. HTTP-level rejection
    // fails the connection permanently (unless the delegate wants a
    // retry); network-level errors trigger reconnection with backoff.
    const network::mojom::URLResponseHead* response_info =
        loader_->ResponseInfo();
    const int response_code = response_info && response_info->headers
                                  ? response_info->headers->response_code()
                                  : 0;
    const bool http_rejection =
        loader_->NetError() == net::ERR_HTTP_RESPONSE_CODE_FAILURE ||
        (response_info && response_info->headers && response_code != 200);
    if (http_rejection) {
      HandleHttpRejection(response_code);
      return;
    }
    ++consecutive_failed_attempts_;
  }

  loader_.reset();
  framer_.reset();
  SetConnected(false);
  // An observer may have called Disconnect() from the state-change
  // notification; scheduling a reconnect over that teardown would
  // override the observer's explicit request.
  if (RunDeferredTeardown()) {
    return;
  }
  ScheduleReconnect();
}

void ProtoStreamClient::OnRetry(base::OnceClosure start) {
  // SimpleURLLoader-level retries are never enabled; reconnection policy
  // belongs to this client so that resume semantics can apply.
  NOTREACHED();
}

void ProtoStreamClient::OnStallTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No bytes — not even noop keep-alives — for kStallTimeout. The
  // connection is likely half-open (e.g. a NAT timeout); it would never
  // report an error on its own, so tear it down and resume.
  loader_.reset();
  framer_.reset();
  SetConnected(false);
  ScheduleReconnect();
}

void ProtoStreamClient::ScheduleReconnect() {
  // The first reconnect waits the base reconnection time, and each
  // further consecutive failure doubles the wait, up to the max.
  // TimeDelta math saturates, so the max param is the only cap needed.
  const base::TimeDelta delay =
      std::min(kProtoStreamBaseReconnectionTime.Get() *
                   std::pow(2.0, std::max(consecutive_failed_attempts_ - 1, 0)),
               kProtoStreamMaxReconnectionTime.Get());
  // base::Unretained is safe: `this` owns `reconnect_timer_`.
  reconnect_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&ProtoStreamClient::StartRequest, base::Unretained(this)));
}

void ProtoStreamClient::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  preparing_request_ = false;
  reconnect_timer_.Stop();
  stall_timer_.Stop();
  loader_.reset();
  framer_.reset();
  SetConnected(false);
  // A Disconnect() arriving during the state-change notification above
  // has nothing further to tear down, and a full stop cancels any queued
  // restart.
  teardown_deferred_ = false;
  connect_after_teardown_ = false;
}

bool ProtoStreamClient::RunDeferredTeardown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!teardown_deferred_) {
    return false;
  }
  const bool restart = connect_after_teardown_;
  TearDown();  // Clears both deferral flags.
  if (restart) {
    // Connect() rather than StartRequest(): a restart gets the same
    // fresh-start state a regular Connect() gets.
    Connect();
  }
  return true;
}

void ProtoStreamClient::SetConnected(bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connected_ == connected) {
    return;
  }
  connected_ = connected;
  base::AutoReset<bool> notifying(&notifying_observers_, true);
  for (Observer& observer : observers_) {
    observer.OnStreamConnectionStateChange(connected);
  }
}

}  // namespace browser_actuator
