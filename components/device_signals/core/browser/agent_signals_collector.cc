// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/agent_signals_collector.h"

#include <utility>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/device_signals/core/browser/crowdstrike_client.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_features.h"

namespace device_signals {

namespace {

bool CanReportIdentifiers(UserPermission permission) {
  return permission == UserPermission::kGranted;
}

bool ShouldReturnDetectedAgents(const SignalsAggregationRequest& request) {
  return enterprise_signals::features::
             IsDetectedAgentSignalCollectionEnabled() &&
         request.agent_signal_parameters.contains(
             AgentSignalCollectionType::kDetectedAgents);
}

bool ShouldReturnCrowdStrikeIdentifiers(
    const SignalsAggregationRequest& request,
    UserPermission permission) {
  return request.agent_signal_parameters.contains(
             AgentSignalCollectionType::kCrowdstrikeIdentifiers) &&
         CanReportIdentifiers(permission);
}

}  // namespace

AgentSignalsCollector::AgentSignalsCollector(
    std::unique_ptr<CrowdStrikeClient> crowdstrike_client)
    : BaseSignalsCollector({
          {SignalName::kAgent,
           base::BindRepeating(&AgentSignalsCollector::GetAgentSignal,
                               base::Unretained(this))},
      }),
      crowdstrike_client_(std::move(crowdstrike_client)) {
  CHECK(crowdstrike_client_);
}

AgentSignalsCollector::~AgentSignalsCollector() = default;

void AgentSignalsCollector::GetAgentSignal(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  bool should_return_detected_agents = ShouldReturnDetectedAgents(request);
  bool should_return_crowdstrike_identifiers =
      ShouldReturnCrowdStrikeIdentifiers(request, permission);
  if (!should_return_detected_agents &&
      !should_return_crowdstrike_identifiers) {
    std::move(done_closure).Run();
    return;
  }

  crowdstrike_client_->GetIdentifiers(base::BindOnce(
      &AgentSignalsCollector::OnCrowdStrikeSignalCollected,
      weak_factory_.GetWeakPtr(), should_return_detected_agents,
      should_return_crowdstrike_identifiers,
      base::BindOnce(&AgentSignalsCollector::OnSignalsCollected,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::ref(response), std::move(done_closure))));
}

void AgentSignalsCollector::OnCrowdStrikeSignalCollected(
    bool should_return_detected_agents,
    bool should_return_crowdstrike_identifiers,
    base::OnceCallback<void(AgentSignalsResponse)> callback,
    std::optional<CrowdStrikeSignals> agent_signals,
    std::optional<SignalCollectionError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AgentSignalsResponse signal_response;

  if (should_return_detected_agents && agent_signals &&
      !agent_signals->IsEmpty()) {
    signal_response.detected_agents.push_back(Agents::kCrowdStrikeFalcon);
  }

  if (should_return_crowdstrike_identifiers && agent_signals) {
    signal_response.crowdstrike_signals = std::move(agent_signals);
  }

  if (error) {
    signal_response.collection_error = error.value();
  }

  std::move(callback).Run(std::move(signal_response));
}

void AgentSignalsCollector::OnSignalsCollected(
    base::TimeTicks start_time,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    AgentSignalsResponse agent_signals_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (agent_signals_response.collection_error) {
    LogSignalCollectionFailed(SignalName::kAgent, start_time,
                              agent_signals_response.collection_error.value(),
                              /*is_top_level_error=*/false);
  } else {
    LogSignalCollectionSucceeded(SignalName::kAgent, start_time,
                                 /*signal_collection_size=*/std::nullopt);
  }

  if (agent_signals_response != AgentSignalsResponse()) {
    response.agent_signals_response = std::move(agent_signals_response);
  }
  std::move(done_closure).Run();
}

}  // namespace device_signals
