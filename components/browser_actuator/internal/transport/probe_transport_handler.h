// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROBE_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROBE_TRANSPORT_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
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
class ProbeTransportHandler : public TransportHandler {
 public:
  // `session` may be null; if non-null, it must outlive this
  // `ProbeTransportHandler`. When `session` is null, handling probe requests
  // no-ops.
  explicit ProbeTransportHandler(TransportSession* session);
  ~ProbeTransportHandler() override;

  ProbeTransportHandler(const ProbeTransportHandler&) = delete;
  ProbeTransportHandler& operator=(const ProbeTransportHandler&) = delete;

  // TransportHandler:
  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  void HandleProbeRequest(const TransportProbe::ProbeRequest& request);
  void SendProbeResponse(std::string probe_id,
                         int32_t response_index,
                         int32_t total_responses,
                         int32_t payload_size_bytes);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ProbeTransportHandler> weak_ptr_factory_{this};
};

// Factory for creating `ProbeTransportHandler` instances for sessions.
class ProbeTransportHandlerFactory : public TransportHandlerFactory {
 public:
  ProbeTransportHandlerFactory();
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
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_PROBE_TRANSPORT_HANDLER_H_
