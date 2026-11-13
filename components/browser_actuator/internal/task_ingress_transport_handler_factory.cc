// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/task_ingress_transport_handler_factory.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/internal/task_ingress_transport_handler.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_session.h"

namespace browser_actuator {

TaskIngressTransportHandlerFactory::TaskIngressTransportHandlerFactory() =
    default;

TaskIngressTransportHandlerFactory::~TaskIngressTransportHandlerFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DetachAllClients();
}

stream_transport::FactoryId TaskIngressTransportHandlerFactory::GetFactoryId()
    const {
  return stream_transport::FactoryId::kTaskIngress;
}

std::vector<stream_transport::PayloadType>
TaskIngressTransportHandlerFactory::GetSupportedPayloadTypes() const {
  return {stream_transport::PayloadType::kTaskIngress};
}

std::unique_ptr<stream_transport::TransportHandler>
TaskIngressTransportHandlerFactory::OnNewSession(
    stream_transport::TransportSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session) {
    return nullptr;
  }

  auto it = session_clients_.find(session->GetSessionId());
  if (it == session_clients_.end()) {
    return nullptr;
  }

  base::WeakPtr<TaskIngressEventDelegate> client = it->second;
  if (!client) {
    session_clients_.erase(it);
    return nullptr;
  }

  return std::make_unique<TaskIngressTransportHandler>(session,
                                                       std::move(client));
}

void TaskIngressTransportHandlerFactory::RegisterSessionClient(
    std::string_view session_id,
    base::WeakPtr<TaskIngressEventDelegate> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!session_id.empty());
  if (!client) {
    return;
  }
  base::EraseIf(session_clients_, [](const auto& p) { return !p.second; });
  session_clients_.insert_or_assign(std::string(session_id), std::move(client));
}

void TaskIngressTransportHandlerFactory::UnregisterSessionClient(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_clients_.erase(session_id);
}

void TaskIngressTransportHandlerFactory::TrackClient(
    base::WeakPtr<TaskIngressEventDelegate> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client) {
    return;
  }
  std::erase_if(tracked_clients_, [](const auto& ptr) { return !ptr; });
  if (std::ranges::contains(tracked_clients_, client.get(),
                            &base::WeakPtr<TaskIngressEventDelegate>::get)) {
    return;
  }
  tracked_clients_.push_back(std::move(client));
}

void TaskIngressTransportHandlerFactory::DetachAllClients() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  session_clients_.clear();
  std::vector<base::WeakPtr<TaskIngressEventDelegate>> clients =
      std::exchange(tracked_clients_, {});
  for (auto& client : clients) {
    if (client) {
      client->DetachForShutdown();
    }
  }
}

base::WeakPtr<TaskIngressTransportHandlerFactory>
TaskIngressTransportHandlerFactory::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace browser_actuator
