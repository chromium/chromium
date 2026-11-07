// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/probe_transport_handler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/transport/probe_payload.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_session.h"

namespace browser_actuator::transport {

ProbeTransportHandler::ProbeTransportHandler(TransportSession* session)
    : TransportHandler(session) {}

ProbeTransportHandler::~ProbeTransportHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProbeTransportHandler::OnMessage(PayloadType payload_type,
                                      std::string_view serialized_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportProbe probe;
  if (!probe.ParseFromString(serialized_payload)) {
    DLOG(WARNING) << "Failed to parse TransportProbe payload";
    return;
  }

  switch (probe.payload_case()) {
    case TransportProbe::kRequest:
      HandleProbeRequest(probe.request());
      break;
    case TransportProbe::kResponse:
    case TransportProbe::PAYLOAD_NOT_SET:
      DLOG(WARNING) << "Received unexpected TransportProbe payload case, "
                       "skipping: "
                    << static_cast<int>(probe.payload_case());
      break;
  }
}

void ProbeTransportHandler::HandleProbeRequest(
    const TransportProbe::ProbeRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session()) {
    return;
  }
  base::expected<void, std::string> validation = ValidateProbeRequest(request);
  if (!validation.has_value()) {
    DLOG(WARNING) << "Dropping invalid ProbeRequest: " << validation.error();
    return;
  }

  const int32_t total_responses = request.response_specs_size();
  if (total_responses == 0) {
    return;
  }

  for (const TransportProbe::ProbeRequest::ResponseSpec& spec :
       request.response_specs()) {
    if (spec.delay_ms() > 0) {
      DLOG(WARNING) << "Dropping ProbeRequest with unsupported non-zero "
                       "delay_ms";
      return;
    }
  }

  base::WeakPtr<ProbeTransportHandler> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  for (int32_t index = 0; index < total_responses; ++index) {
    if (!weak_this) {
      return;
    }
    const TransportProbe::ProbeRequest::ResponseSpec& spec =
        request.response_specs(index);
    SendProbeResponse(request.probe_id(), index, total_responses,
                      spec.payload_size_bytes());
  }
}

void ProbeTransportHandler::SendProbeResponse(std::string probe_id,
                                              int32_t response_index,
                                              int32_t total_responses,
                                              int32_t payload_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session()) {
    return;
  }
  base::expected<std::string, std::string> payload =
      GenerateProbeResponsePayload(probe_id, response_index, total_responses,
                                   payload_size_bytes);
  if (!payload.has_value()) {
    DLOG(ERROR) << "Failed to generate ProbeResponse payload: "
                << payload.error();
    return;
  }

  TransportProbe response_probe;
  TransportProbe::ProbeResponse* probe_response =
      response_probe.mutable_response();
  probe_response->set_probe_id(std::move(probe_id));
  probe_response->set_response_index(response_index);
  probe_response->set_total_responses(total_responses);
  probe_response->set_payload(*std::move(payload));

  base::expected<void, SendUpstreamMessageError> result =
      SendUpstreamMessage(PayloadType::kProbe, response_probe);
  if (!result.has_value()) {
    DLOG(ERROR) << "Failed to send ProbeResponse: "
                << static_cast<int>(result.error());
  }
}

ProbeTransportHandlerFactory::ProbeTransportHandlerFactory() = default;

ProbeTransportHandlerFactory::~ProbeTransportHandlerFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FactoryId ProbeTransportHandlerFactory::GetFactoryId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return FactoryId::kProbe;
}

std::vector<PayloadType>
ProbeTransportHandlerFactory::GetSupportedPayloadTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {PayloadType::kProbe};
}

std::unique_ptr<TransportHandler> ProbeTransportHandlerFactory::OnNewSession(
    TransportSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<ProbeTransportHandler>(session);
}

}  // namespace browser_actuator::transport
