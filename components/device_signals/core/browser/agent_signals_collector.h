// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_AGENT_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_AGENT_SIGNALS_COLLECTOR_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace device_signals {

class CrowdStrikeClient;
struct CrowdStrikeSignals;
enum class SignalCollectionError;

// Collector in charge of collecting signals from supported third-party
// agents running on the device.
class AgentSignalsCollector : public BaseSignalsCollector {
 public:
  explicit AgentSignalsCollector(
      std::unique_ptr<CrowdStrikeClient> crowdstrike_client);

  ~AgentSignalsCollector() override;

  AgentSignalsCollector(const AgentSignalsCollector&) = delete;
  AgentSignalsCollector& operator=(const AgentSignalsCollector&) = delete;

 private:
  // Collection function for the Agent signal. `request` is ignored since the
  // agent signal does not require parameters. `response` will be passed along
  // and the signal values will be set on it when available. `done_closure` will
  // be invoked when signal collection is complete.
  void GetAgentSignal(const SignalsAggregationRequest& request,
                      SignalsAggregationResponse& response,
                      base::OnceClosure done_closure);

  // Invoked when the CrowdStrike client returns the collected agent
  // signals as `agent_signals`. Will update `response` with the
  // signal collection outcome, and invoke `done_closure` to asynchronously
  // notify the caller of the completion of this request.
  void OnCrowdStrikeSignalCollected(
      base::TimeTicks start_time,
      SignalsAggregationResponse& response,
      base::OnceClosure done_closure,
      std::optional<CrowdStrikeSignals> agent_signals,
      std::optional<SignalCollectionError> error);

  // Instance used to collect signals from a CrowdStrike agent.
  std::unique_ptr<CrowdStrikeClient> crowdstrike_client_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AgentSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_AGENT_SIGNALS_COLLECTOR_H_
