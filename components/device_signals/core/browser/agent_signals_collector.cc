// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/agent_signals_collector.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/device_signals/core/browser/crowdstrike_client.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"

namespace device_signals {

AgentSignalsCollector::AgentSignalsCollector(
    std::unique_ptr<CrowdStrikeClient> crowdstrike_client)
    : BaseSignalsCollector({
          {SignalName::kAgent,
           base::BindRepeating(&AgentSignalsCollector::GetAgentSignal,
                               base::Unretained(this))},
      }),
      crowdstrike_client_(std::move(crowdstrike_client)) {
  DCHECK(crowdstrike_client_);
}

AgentSignalsCollector::~AgentSignalsCollector() = default;

void AgentSignalsCollector::GetAgentSignal(
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  crowdstrike_client_->GetIdentifiers(base::BindOnce(
      &AgentSignalsCollector::OnCrowdStrikeSignalCollected,
      weak_factory_.GetWeakPtr(), std::ref(response), std::move(done_closure)));
}

void AgentSignalsCollector::OnCrowdStrikeSignalCollected(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    absl::optional<CrowdStrikeSignals> agent_signals) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (agent_signals) {
    AgentSignalsResponse signal_response;
    signal_response.crowdstrike_signals = std::move(agent_signals);
    response.agent_signals_response = std::move(signal_response);
  }
  std::move(done_closure).Run();
}

}  // namespace device_signals
