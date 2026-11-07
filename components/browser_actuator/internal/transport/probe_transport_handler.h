// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROBE_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROBE_TRANSPORT_HANDLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_handler.h"
#include "components/browser_actuator/public/transport/transport_handler_factory.h"

namespace browser_actuator::transport {

class TransportSession;

// Processes session-scoped `PayloadType::kProbe` (`TransportProbe`) messages
// for a single `TransportSession`.
//
// Must be constructed, invoked, and destroyed on the owning session's sequence.
// Destroying the handler (for example on session teardown) cancels all pending
// delayed probe timers via `weak_ptr_factory_`.
class ProbeTransportHandler : public TransportHandler {
 public:
  // Returns the downstream sequence number currently being dispatched on
  // `session_id`, or `std::nullopt` outside an active dispatch frame.
  using GetDispatchingSequenceCallback =
      base::RepeatingCallback<std::optional<int64_t>(
          std::string_view session_id)>;

  // Sends a `PayloadType::kProbe` reply for `session_id` carrying an explicit
  // `responding_to_sequence_number` (so delayed probe responses preserve the
  // originating `ProbeRequest` sequence number after synchronous dispatch
  // unwinds). When unset in unit tests, falls back to
  // `TransportHandler::SendUpstreamMessage()`.
  using SendProbeReplyCallback =
      base::RepeatingCallback<base::expected<void, SendUpstreamMessageError>(
          std::string_view session_id,
          const TransportProbe& probe,
          std::optional<int64_t> responding_to_sequence_number)>;

  // `session` may be null; if non-null, it must outlive this
  // `ProbeTransportHandler`. When `session` is null, handling probe requests
  // no-ops.
  explicit ProbeTransportHandler(
      TransportSession* session,
      GetDispatchingSequenceCallback get_dispatching_sequence_cb = {},
      SendProbeReplyCallback send_probe_reply_cb = {});
  ~ProbeTransportHandler() override;

  ProbeTransportHandler(const ProbeTransportHandler&) = delete;
  ProbeTransportHandler& operator=(const ProbeTransportHandler&) = delete;

  // TransportHandler:
  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  void HandleProbeRequest(const TransportProbe::ProbeRequest& request);
  void OnDelayedProbeTimerFired(
      std::string probe_id,
      int32_t response_index,
      int32_t total_responses,
      int32_t payload_size_bytes,
      std::optional<int64_t> responding_to_sequence_number);
  void SendProbeResponse(std::string probe_id,
                         int32_t response_index,
                         int32_t total_responses,
                         int32_t payload_size_bytes,
                         std::optional<int64_t> responding_to_sequence_number);

  SEQUENCE_CHECKER(sequence_checker_);

  GetDispatchingSequenceCallback get_dispatching_sequence_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SendProbeReplyCallback send_probe_reply_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Session-wide in-flight delayed probe counters enforcing
  // `kMaxProbeResponses` and `kMaxTotalProbeResponseBytes` across overlapping
  // `ProbeRequest`s.
  int32_t pending_delayed_responses_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  int64_t pending_delayed_response_bytes_
      GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<ProbeTransportHandler> weak_ptr_factory_{this};
};

// Factory creating per-session `ProbeTransportHandler` instances wired to the
// owning `TransportChannelImpl`.
class ProbeTransportHandlerFactory : public TransportHandlerFactory {
 public:
  explicit ProbeTransportHandlerFactory(
      ProbeTransportHandler::GetDispatchingSequenceCallback
          get_dispatching_sequence_cb = {},
      ProbeTransportHandler::SendProbeReplyCallback send_probe_reply_cb = {});
  ~ProbeTransportHandlerFactory() override;

  ProbeTransportHandlerFactory(const ProbeTransportHandlerFactory&) = delete;
  ProbeTransportHandlerFactory& operator=(const ProbeTransportHandlerFactory&) =
      delete;

  // TransportHandlerFactory:
  FactoryId GetFactoryId() const override;
  std::vector<PayloadType> GetSupportedPayloadTypes() const override;
  std::unique_ptr<TransportHandler> OnNewSession(
      TransportSession* session) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  ProbeTransportHandler::GetDispatchingSequenceCallback
      get_dispatching_sequence_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  ProbeTransportHandler::SendProbeReplyCallback send_probe_reply_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROBE_TRANSPORT_HANDLER_H_
