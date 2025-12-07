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
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"

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
  using AgentSignalsResponseCallback =
      base::RepeatingCallback<void(AgentSignalsResponse)>;

  explicit AgentSignalsCollector(
      std::unique_ptr<CrowdStrikeClient> crowdstrike_client);

  ~AgentSignalsCollector() override;

  AgentSignalsCollector(const AgentSignalsCollector&) = delete;
  AgentSignalsCollector& operator=(const AgentSignalsCollector&) = delete;

 private:
  // Collection function for the Agent signal. `request` contains the details on
  // which agent signals should be collected. `response` will be passed along
  // and the signal values will be set on it when available. `done_closure` will
  // be invoked when signal collection is complete.
  void GetAgentSignal(UserPermission permission,
                      const SignalsAggregationRequest& request,
                      SignalsAggregationResponse& response,
                      base::OnceClosure done_closure);

  // Invoked when the CrowdStrike client returns the collected agent
  // signals as `agent_signals`. Will invoke `callback` with the signal
  // collection outcome to asynchronously notify the caller of the completion of
  // this request. `should_return_detected_agents` and
  // `should_return_crowdstrike_identifiers` determine what values will get
  // mapped to the generated AgentSignalsResponse.
  void OnCrowdStrikeSignalCollected(
      bool should_return_detected_agents,
      bool should_return_crowdstrike_identifiers,
      base::OnceCallback<void(AgentSignalsResponse)> callback,
      std::optional<CrowdStrikeSignals> agent_signals,
      std::optional<SignalCollectionError> error);

  // Invoked when `agent_signals_response` is collected. Updates the
  // `response` with the collected `agent_signals_response` and invokes the
  // `done_closure` after.
  void OnSignalsCollected(base::TimeTicks start_time,
                          SignalsAggregationResponse& response,
                          base::OnceClosure done_closure,
                          AgentSignalsResponse agent_signals_response);

  // Instance used to collect signals from a CrowdStrike agent.
  std::unique_ptr<CrowdStrikeClient> crowdstrike_client_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AgentSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_AGENT_SIGNALS_COLLECTOR_H_
