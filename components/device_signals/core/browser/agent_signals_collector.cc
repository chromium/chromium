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
#include "components/device_signals/core/browser/detected_agent_client.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_features.h"

namespace device_signals {

AgentSignalsCollector::AgentSignalsCollector(
    std::unique_ptr<CrowdStrikeClient> crowdstrike_client,
    std::unique_ptr<DetectedAgentClient> detected_agent_client)
    : BaseSignalsCollector({
          {SignalName::kAgent,
           base::BindRepeating(&AgentSignalsCollector::GetAgentSignal,
                               base::Unretained(this))},
      }),
      crowdstrike_client_(std::move(crowdstrike_client)),
      detected_agent_client_(std::move(detected_agent_client)) {
  CHECK(crowdstrike_client_);
  CHECK(detected_agent_client_);
}

AgentSignalsCollector::~AgentSignalsCollector() = default;

void AgentSignalsCollector::GetAgentSignal(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  bool is_detected_agent_signal_collection_enabled =
      enterprise_signals::features::IsDetectedAgentSignalCollectionEnabled();
  auto barrier_cb = base::BarrierCallback<AgentSignalsResponse>(
      /*num_callbacks=*/is_detected_agent_signal_collection_enabled ? 2 : 1,
      base::BindOnce(&AgentSignalsCollector::OnSignalsCollected,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::ref(response), std::move(done_closure)));

  GetCrowdstrikeIdentifierSignals(permission, request, response, barrier_cb);

  if (is_detected_agent_signal_collection_enabled) {
    GetDetectedAgentSignal(request, response, barrier_cb);
  }
}

void AgentSignalsCollector::GetDetectedAgentSignal(
    const SignalsAggregationRequest request,
    SignalsAggregationResponse response,
    AgentSignalsResponseCallback agent_response_cb) {
  if (!request.agent_signal_parameters.contains(
          AgentSignalCollectionType::kDetectedAgents)) {
    agent_response_cb.Run(AgentSignalsResponse());
    return;
  }

  detected_agent_client_->GetAgents(base::BindOnce(
      &AgentSignalsCollector::OnDetectedAgentSignalCollected,
      weak_factory_.GetWeakPtr(), std::ref(response), agent_response_cb));
}

void AgentSignalsCollector::OnDetectedAgentSignalCollected(
    SignalsAggregationResponse& response,
    AgentSignalsResponseCallback agent_response_cb,
    std::vector<Agents> agent_signals) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AgentSignalsResponse signal_response;
  signal_response.detected_agents = std::move(agent_signals);
  agent_response_cb.Run(std::move(signal_response));
}

void AgentSignalsCollector::GetCrowdstrikeIdentifierSignals(
    UserPermission permission,
    const SignalsAggregationRequest request,
    SignalsAggregationResponse response,
    AgentSignalsResponseCallback agent_response_cb) {
  if ((permission != UserPermission::kGranted) ||
      (!request.agent_signal_parameters.contains(
          AgentSignalCollectionType::kCrowdstrikeIdentifiers))) {
    agent_response_cb.Run(AgentSignalsResponse());
    return;
  }

  crowdstrike_client_->GetIdentifiers(base::BindOnce(
      &AgentSignalsCollector::OnCrowdStrikeSignalCollected,
      weak_factory_.GetWeakPtr(), std::ref(response), agent_response_cb));
}

void AgentSignalsCollector::OnCrowdStrikeSignalCollected(
    SignalsAggregationResponse& response,
    AgentSignalsResponseCallback agent_response_cb,
    std::optional<CrowdStrikeSignals> agent_signals,
    std::optional<SignalCollectionError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AgentSignalsResponse signal_response;

  if (agent_signals || error) {
    if (agent_signals) {
      signal_response.crowdstrike_signals = std::move(agent_signals);
    }

    if (error) {
      signal_response.collection_error = error.value();
    }
  }

  agent_response_cb.Run(std::move(signal_response));
}

void AgentSignalsCollector::OnSignalsCollected(
    base::TimeTicks start_time,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    std::vector<AgentSignalsResponse> agent_signals_responses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AgentSignalsResponse signal_response;
  for (AgentSignalsResponse agent_signals_response : agent_signals_responses) {
    if (agent_signals_response.collection_error) {
      signal_response.collection_error =
          std::move(agent_signals_response.collection_error);
    }

    if (agent_signals_response.crowdstrike_signals) {
      signal_response.crowdstrike_signals =
          std::move(agent_signals_response.crowdstrike_signals);
    }

    if (!agent_signals_response.detected_agents.empty()) {
      signal_response.detected_agents =
          std::move(agent_signals_response.detected_agents);
    }
  }

  if (signal_response.collection_error) {
    LogSignalCollectionFailed(SignalName::kAgent, start_time,
                              signal_response.collection_error.value(),
                              /*is_top_level_error=*/false);
  } else {
    LogSignalCollectionSucceeded(SignalName::kAgent, start_time,
                                 /*signal_collection_size=*/std::nullopt);
  }

  if (signal_response != AgentSignalsResponse()) {
    response.agent_signals_response = std::move(signal_response);
  }
  std::move(done_closure).Run();
}

}  // namespace device_signals
