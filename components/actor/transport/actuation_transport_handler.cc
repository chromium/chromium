// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/transport/actuation_transport_handler.h"

#include "base/notimplemented.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

ActuationTransportHandler::ActuationTransportHandler(
    browser_actuator::TransportSession* session,
    std::unique_ptr<ActuationDelegate> delegate)
    : browser_actuator::TransportHandler(session) {
  // TODO(crbug.com/565388773): Store delegate and validate non-null session and
  // delegate.
  NOTIMPLEMENTED();
}

ActuationTransportHandler::~ActuationTransportHandler() {
  // TODO(crbug.com/565389256): Stop all tasks for the session on destruction.
  NOTIMPLEMENTED();
}

void ActuationTransportHandler::OnMessage(
    browser_actuator::PayloadType payload_type,
    std::string_view serialized_payload) {
  // TODO(crbug.com/565388773): Parse ActuationDownstreamMessage and dispatch to
  // command handlers.
  NOTIMPLEMENTED();
}

void ActuationTransportHandler::HandleStartTask(
    const std::string& request_id,
    const optimization_guide::proto::BrowserStartTask& request) {
  // TODO(crbug.com/565389255): Forward StartTask to ActuationDelegate.
  NOTIMPLEMENTED();
}

void ActuationTransportHandler::OnStartTaskComplete(
    const std::string& request_id,
    optimization_guide::proto::BrowserStartTaskResult result) {
  // TODO(crbug.com/565389255): Send BrowserStartTaskResult upstream.
  NOTIMPLEMENTED();
}

void ActuationTransportHandler::HandleStopTask(const std::string& request_id,
                                               const proto::StopTask& request) {
  // TODO(crbug.com/565389256): Forward StopTask to ActuationDelegate.
  NOTIMPLEMENTED();
}

void ActuationTransportHandler::OnStopTaskComplete(
    const std::string& request_id,
    TaskId task_id,
    bool success) {
  // TODO(crbug.com/565389256): Send StopTaskResult upstream.
  NOTIMPLEMENTED();
}

void ActuationTransportHandler::HandleAct(
    const std::string& request_id,
    const optimization_guide::proto::Actions& actions) {
  // TODO(crbug.com/565389615): Forward Act to ActuationDelegate.
  NOTIMPLEMENTED();
}

void ActuationTransportHandler::OnActComplete(
    const std::string& request_id,
    optimization_guide::proto::ActionsResult result) {
  // TODO(crbug.com/565389615): Send ActionsResult upstream.
  NOTIMPLEMENTED();
}

}  // namespace actor
