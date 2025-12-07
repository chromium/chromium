// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/agent_signals_collector.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/device_signals/core/browser/crowdstrike_client.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/signals_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace device_signals {

namespace {
class MockCrowdStrikeClient : public CrowdStrikeClient {
 public:
  MockCrowdStrikeClient();
  ~MockCrowdStrikeClient() override;

  MOCK_METHOD(void,
              GetIdentifiers,
              (base::OnceCallback<void(std::optional<CrowdStrikeSignals>,
                                       std::optional<SignalCollectionError>)>),
              (override));
};

MockCrowdStrikeClient::MockCrowdStrikeClient() = default;
MockCrowdStrikeClient::~MockCrowdStrikeClient() = default;

SignalsAggregationRequest CreateRequest(bool add_crowdstrike_ids = true,
                                        bool add_detected_agents = true) {
  SignalsAggregationRequest request;
  if (add_crowdstrike_ids) {
    request.agent_signal_parameters.emplace(
        device_signals::AgentSignalCollectionType::kCrowdstrikeIdentifiers);
  }

  if (add_detected_agents) {
    request.agent_signal_parameters.emplace(
        device_signals::AgentSignalCollectionType::kDetectedAgents);
  }

  return request;
}

CrowdStrikeSignals GetValidSignals() {
  CrowdStrikeSignals valid_signals;
  valid_signals.agent_id = "1234";
  valid_signals.customer_id = "abcd";
  return valid_signals;
}

}  // namespace

class AgentSignalsCollectorTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {
 protected:
  AgentSignalsCollectorTest() {
    scoped_feature_list_.InitWithFeatureState(
        enterprise_signals::features::kDetectedAgentSignalCollectionEnabled,
        is_detected_agent_signal_collection_enabled());
  }
  bool is_detected_agent_signal_collection_enabled() { return GetParam(); }

  void CreateCollector() {
    auto mocked_crowdstrike_client =
        std::make_unique<StrictMock<MockCrowdStrikeClient>>();
    mocked_crowdstrike_client_ = mocked_crowdstrike_client.get();

    collector_ = std::make_unique<AgentSignalsCollector>(
        std::move(mocked_crowdstrike_client));
  }

  void SetCrowdStrikeSignals(
      const std::optional<CrowdStrikeSignals>& crowdstrike_signal,
      const std::optional<SignalCollectionError>& crowdstrike_signal_error) {
    EXPECT_CALL(*mocked_crowdstrike_client_, GetIdentifiers(_))
        .WillOnce(
            [crowdstrike_signal, crowdstrike_signal_error](
                base::OnceCallback<void(std::optional<CrowdStrikeSignals>,
                                        std::optional<SignalCollectionError>)>
                    callback) {
              std::move(callback).Run(crowdstrike_signal,
                                      crowdstrike_signal_error);
            });
  }

  void RunTest(std::optional<CrowdStrikeSignals> crowdstrike_signal,
               std::vector<Agents> detected_agents,
               std::optional<SignalCollectionError> crowdstrike_signal_error) {
    CreateCollector();
    SetCrowdStrikeSignals(crowdstrike_signal, crowdstrike_signal_error);

    SignalsAggregationResponse captured_response;

    base::RunLoop run_loop;
    collector_->GetSignal(SignalName::kAgent, UserPermission::kGranted,
                          CreateRequest(), captured_response,
                          run_loop.QuitClosure());

    run_loop.Run();

    if (crowdstrike_signal) {
      ASSERT_TRUE(captured_response.agent_signals_response);
      ASSERT_TRUE(
          captured_response.agent_signals_response->crowdstrike_signals);
      EXPECT_EQ(
          captured_response.agent_signals_response->crowdstrike_signals.value(),
          crowdstrike_signal.value());
    }

    if (!detected_agents.empty() &&
        is_detected_agent_signal_collection_enabled()) {
      ASSERT_TRUE(captured_response.agent_signals_response);
      ASSERT_TRUE(captured_response.agent_signals_response->detected_agents ==
                  detected_agents);
    }

    if (crowdstrike_signal_error) {
      ASSERT_TRUE(captured_response.agent_signals_response);
      ASSERT_TRUE(captured_response.agent_signals_response->collection_error);
      EXPECT_EQ(
          captured_response.agent_signals_response->collection_error.value(),
          crowdstrike_signal_error.value());

      histogram_tester_.ExpectTotalCount(
          "Enterprise.DeviceSignals.Collection.Success", 0);
      histogram_tester_.ExpectUniqueSample(
          "Enterprise.DeviceSignals.Collection.Failure", SignalName::kAgent, 1);
      histogram_tester_.ExpectTotalCount(
          "Enterprise.DeviceSignals.Collection.Failure.Agent.Latency", 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "Enterprise.DeviceSignals.Collection.Failure", 0);
      histogram_tester_.ExpectTotalCount(
          "Enterprise.DeviceSignals.Collection.Failure.Agent.Latency", 0);
    }

    if (crowdstrike_signal && !crowdstrike_signal_error) {
      histogram_tester_.ExpectUniqueSample(
          "Enterprise.DeviceSignals.Collection.Success", SignalName::kAgent, 1);
    }

    if (!crowdstrike_signal && !crowdstrike_signal_error &&
        detected_agents.empty()) {
      ASSERT_FALSE(captured_response.agent_signals_response);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<AgentSignalsCollector> collector_;
  raw_ptr<StrictMock<MockCrowdStrikeClient>> mocked_crowdstrike_client_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_P(AgentSignalsCollectorTest, SupportedSignalNames) {
  CreateCollector();
  const std::array<SignalName, 1> supported_signals{{SignalName::kAgent}};

  const auto names_set = collector_->GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Tests that an unsupported signal is marked as unsupported.
TEST_P(AgentSignalsCollectorTest, GetSignal_Unsupported) {
  CreateCollector();
  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  collector_->GetSignal(signal_name, UserPermission::kGranted, CreateRequest(),
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests the scenario where the CrowdStrikeIdentifier signal request parameter
// is missing but the DetectedAgents signal request parameter is present.
TEST_P(AgentSignalsCollectorTest,
       GetSignal_MissingCrowdstrikeIdentifierSignalCollectionTypeOnly) {
  CreateCollector();
  SignalName signal_name = SignalName::kAgent;
  SignalsAggregationResponse response;
  std::vector<Agents> detected_agents = {Agents::kCrowdStrikeFalcon};
  base::RunLoop run_loop;
  if (is_detected_agent_signal_collection_enabled()) {
    SetCrowdStrikeSignals(GetValidSignals(), std::nullopt);
  }

  collector_->GetSignal(signal_name, UserPermission::kGranted,
                        CreateRequest(/*add_crowdstrike_ids=*/false,
                                      /*add_detected_agents=*/true),
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  if (is_detected_agent_signal_collection_enabled()) {
    ASSERT_TRUE(response.agent_signals_response);
    ASSERT_FALSE(response.agent_signals_response->crowdstrike_signals);
    EXPECT_EQ(response.agent_signals_response->detected_agents,
              detected_agents);
  } else {
    ASSERT_FALSE(response.agent_signals_response);
  }
}

// Tests the scenario where the DetectedAgents signal request parameter is
// missing but the CrowdStrikeIdentifier signal request parameter is present.
TEST_P(AgentSignalsCollectorTest,
       GetSignal_MissingDetectedAgentSignalCollectionTypeOnly) {
  CreateCollector();
  SignalName signal_name = SignalName::kAgent;
  SignalsAggregationResponse response;
  CrowdStrikeSignals crowdstrike_signal = GetValidSignals();
  base::RunLoop run_loop;
  SetCrowdStrikeSignals(crowdstrike_signal, std::nullopt);

  collector_->GetSignal(signal_name, UserPermission::kGranted,
                        CreateRequest(/*add_crowdstrike_ids=*/true,
                                      /*add_detected_agents=*/false),
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.agent_signals_response);
  ASSERT_TRUE(response.agent_signals_response->detected_agents.empty());
  ASSERT_TRUE(response.agent_signals_response->crowdstrike_signals);
  EXPECT_EQ(response.agent_signals_response->crowdstrike_signals.value(),
            crowdstrike_signal);
}

// Tests the scenario where CrowdStrike signal collection fails due to
// insufficient permissions, but DetectedAgent signals are still collected and
// are not empty.
TEST_P(AgentSignalsCollectorTest,
       GetSignal_MissingConsent_DetectedAgentSignalPresent) {
  CreateCollector();
  SignalName signal_name = SignalName::kAgent;
  SignalsAggregationResponse response;
  std::vector<Agents> detected_agents = {Agents::kCrowdStrikeFalcon};
  base::RunLoop run_loop;
  if (is_detected_agent_signal_collection_enabled()) {
    SetCrowdStrikeSignals(GetValidSignals(), std::nullopt);
  }

  collector_->GetSignal(signal_name, UserPermission::kMissingConsent,
                        CreateRequest(), response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  if (is_detected_agent_signal_collection_enabled()) {
    ASSERT_TRUE(response.agent_signals_response);
    ASSERT_FALSE(response.agent_signals_response->crowdstrike_signals);
    EXPECT_EQ(response.agent_signals_response->detected_agents,
              detected_agents);
  } else {
    ASSERT_FALSE(response.agent_signals_response);
  }
}

// Tests the scenario where CrowdStrike signal collection fails due to
// insufficient permissions, but DetectedAgent signals are still collected and
// are empty.
TEST_P(AgentSignalsCollectorTest,
       GetSignal_MissingConsent_DetectedAgentSignalEmpty) {
  CreateCollector();
  SignalName signal_name = SignalName::kAgent;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  if (is_detected_agent_signal_collection_enabled()) {
    SetCrowdStrikeSignals(std::nullopt, std::nullopt);
  }

  collector_->GetSignal(signal_name, UserPermission::kMissingConsent,
                        CreateRequest(), response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_FALSE(response.agent_signals_response);
}

TEST_P(AgentSignalsCollectorTest, GetSignal_Success) {
  RunTest(/*crowdstrike_signal=*/GetValidSignals(),
          /*detected_agents_signal=*/{Agents::kCrowdStrikeFalcon},
          /*crowdstrike_signal_error=*/std::nullopt);
}

TEST_P(AgentSignalsCollectorTest, GetSignal_NoSignalNoError) {
  RunTest(/*crowdstrike_signal=*/std::nullopt, /*detected_agents_signal=*/{},
          /*crowdstrike_signal_error=*/std::nullopt);
}

TEST_P(AgentSignalsCollectorTest, GetSignal_NoSignalWithError) {
  RunTest(/*crowdstrike_signal=*/std::nullopt, /*detected_agents_signal=*/{},
          SignalCollectionError::kParsingFailed);
}

INSTANTIATE_TEST_SUITE_P(, AgentSignalsCollectorTest, testing::Bool());

}  // namespace device_signals
