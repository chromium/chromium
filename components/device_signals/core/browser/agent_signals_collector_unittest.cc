// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/agent_signals_collector.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/device_signals/core/browser/crowdstrike_client.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
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

}  // namespace

class AgentSignalsCollectorTest : public testing::Test {
 protected:
  AgentSignalsCollectorTest() {
    auto mocked_crowdstrike_client =
        std::make_unique<StrictMock<MockCrowdStrikeClient>>();
    mocked_crowdstrike_client_ = mocked_crowdstrike_client.get();

    collector_ = std::make_unique<AgentSignalsCollector>(
        std::move(mocked_crowdstrike_client));
  }

  void RunTest(
      std::optional<CrowdStrikeSignals> returned_signals,
      std::optional<SignalCollectionError> returned_error = std::nullopt) {
    EXPECT_CALL(*mocked_crowdstrike_client_, GetIdentifiers(_))
        .WillOnce(Invoke(
            [&returned_signals, &returned_error](
                base::OnceCallback<void(std::optional<CrowdStrikeSignals>,
                                        std::optional<SignalCollectionError>)>
                    callback) {
              std::move(callback).Run(returned_signals, returned_error);
            }));

    SignalsAggregationRequest empty_request;
    SignalsAggregationResponse captured_response;

    base::RunLoop run_loop;
    collector_->GetSignal(SignalName::kAgent, empty_request, captured_response,
                          run_loop.QuitClosure());

    run_loop.Run();

    if (returned_signals) {
      ASSERT_TRUE(captured_response.agent_signals_response);
      ASSERT_TRUE(
          captured_response.agent_signals_response->crowdstrike_signals);
      EXPECT_EQ(
          captured_response.agent_signals_response->crowdstrike_signals.value(),
          returned_signals.value());
    }

    if (returned_error) {
      ASSERT_TRUE(captured_response.agent_signals_response);
      ASSERT_TRUE(captured_response.agent_signals_response->collection_error);
      EXPECT_EQ(
          captured_response.agent_signals_response->collection_error.value(),
          returned_error.value());

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

    if (returned_signals && !returned_error) {
      histogram_tester_.ExpectUniqueSample(
          "Enterprise.DeviceSignals.Collection.Success", SignalName::kAgent, 1);
    }

    if (!returned_signals && !returned_error) {
      ASSERT_FALSE(captured_response.agent_signals_response);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<StrictMock<MockCrowdStrikeClient>, DanglingUntriaged>
      mocked_crowdstrike_client_;
  std::unique_ptr<AgentSignalsCollector> collector_;
  base::HistogramTester histogram_tester_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_F(AgentSignalsCollectorTest, SupportedSignalNames) {
  const std::array<SignalName, 1> supported_signals{{SignalName::kAgent}};

  const auto names_set = collector_->GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Tests that an unsupported signal is marked as unsupported.
TEST_F(AgentSignalsCollectorTest, GetSignal_Unsupported) {
  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  collector_->GetSignal(signal_name, empty_request, response,
                        run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

TEST_F(AgentSignalsCollectorTest, GetSignal_Success) {
  CrowdStrikeSignals valid_signals;
  valid_signals.agent_id = "1234";
  valid_signals.customer_id = "abcd";

  RunTest(valid_signals);
}

TEST_F(AgentSignalsCollectorTest, GetSignal_NoSignalNoError) {
  RunTest(std::nullopt);
}

TEST_F(AgentSignalsCollectorTest, GetSignal_NoSignalWithError) {
  RunTest(std::nullopt, SignalCollectionError::kParsingFailed);
}

}  // namespace device_signals
