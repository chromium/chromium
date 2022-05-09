// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_aggregator_impl.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/browser/mock_signals_collector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Pointee;
using testing::Ref;
using testing::Return;

namespace device_signals {

namespace {

constexpr char kFakeSignalName[] = "signal_name";
constexpr char kOtherFakeSignalName[] = "other_signal_name";

base::Value GetFakeSignalParameter() {
  return base::Value("some parameter needed by the signal collector");
}

base::Value GetFakeSignalValue() {
  base::Value::Dict fake_signal_value;
  fake_signal_value.Set("some_key", "some_value");
  return base::Value(std::move(fake_signal_value));
}

base::Value GetOtherFakeSignalParameter() {
  base::Value::Dict fake_signal_value;
  fake_signal_value.Set("some_file_path", "some/path");
  return base::Value(std::move(fake_signal_value));
}

base::Value GetOtherFakeSignalValue() {
  return base::Value("just a string");
}

std::unique_ptr<MockSignalsCollector> GetCollectorForFakeSignal() {
  auto mock_collector = std::make_unique<MockSignalsCollector>();
  ON_CALL(*mock_collector.get(), GetSupportedSignalNames())
      .WillByDefault(Return(std::vector<std::string>({kFakeSignalName})));

  ON_CALL(*mock_collector.get(), GetSignal(kFakeSignalName, _, _))
      .WillByDefault(
          Invoke([](const std::string& signal_name, const base::Value& params,
                    SignalsCollector::GetSignalCallback callback) {
            EXPECT_EQ(params, GetFakeSignalParameter());
            std::move(callback).Run(GetFakeSignalValue());
          }));

  return mock_collector;
}

std::unique_ptr<MockSignalsCollector> GetCollectorForOtherFakeSignal() {
  auto mock_collector = std::make_unique<MockSignalsCollector>();
  ON_CALL(*mock_collector.get(), GetSupportedSignalNames())
      .WillByDefault(Return(std::vector<std::string>({kOtherFakeSignalName})));

  ON_CALL(*mock_collector.get(), GetSignal(kOtherFakeSignalName, _, _))
      .WillByDefault(
          Invoke([](const std::string& signal_name, const base::Value& params,
                    SignalsCollector::GetSignalCallback callback) {
            EXPECT_EQ(params, GetOtherFakeSignalParameter());
            std::move(callback).Run(GetOtherFakeSignalValue());
          }));

  return mock_collector;
}

}  // namespace

class SignalsAggregatorImplTest : public testing::Test {
 protected:
  SignalsAggregatorImplTest() {
    auto fake_signal_collector = GetCollectorForFakeSignal();
    fake_signal_collector_ = fake_signal_collector.get();

    auto other_fake_signal_collector = GetCollectorForOtherFakeSignal();
    other_fake_signal_collector_ = other_fake_signal_collector.get();

    std::vector<std::unique_ptr<SignalsCollector>> collectors;
    collectors.push_back(std::move(fake_signal_collector));
    collectors.push_back(std::move(other_fake_signal_collector));
    aggregator_ =
        std::make_unique<SignalsAggregatorImpl>(std::move(collectors));
  }

  base::test::TaskEnvironment task_environment_;
  MockSignalsCollector* fake_signal_collector_;
  MockSignalsCollector* other_fake_signal_collector_;
  std::unique_ptr<SignalsAggregatorImpl> aggregator_;
};

// Tests that the aggregator will return an empty value when given an empty
// parameter dictionary.
TEST_F(SignalsAggregatorImplTest, GetSignals_NoSignal) {
  base::test::TestFuture<base::Value::Dict> future;
  base::Value::Dict empty_value;
  aggregator_->GetSignals(empty_value, future.GetCallback());
  EXPECT_EQ(future.Get(), empty_value);
}

// Tests how the aggregator behaves when given a parameter with a single signal
// which is supported by one of the collectors.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_Supported) {
  base::Value::Dict parameters;
  parameters.Set(kFakeSignalName, GetFakeSignalParameter());

  EXPECT_CALL(*fake_signal_collector_, GetSupportedSignalNames()).Times(1);
  EXPECT_CALL(*fake_signal_collector_, GetSignal(kFakeSignalName, _, _))
      .Times(1);

  EXPECT_CALL(*other_fake_signal_collector_, GetSignal(_, _, _)).Times(0);

  base::test::TestFuture<base::Value::Dict> future;
  aggregator_->GetSignals(parameters, future.GetCallback());

  base::Value::Dict expected_value;
  expected_value.Set(kFakeSignalName, GetFakeSignalValue());

  EXPECT_EQ(future.Get(), expected_value);
}

// Tests how the aggregator behaves when given a parameter with a single signal
// which is supported by another collector.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_SupportedOther) {
  base::Value::Dict parameters;
  parameters.Set(kOtherFakeSignalName, GetOtherFakeSignalParameter());

  EXPECT_CALL(*other_fake_signal_collector_, GetSupportedSignalNames())
      .Times(1);
  EXPECT_CALL(*other_fake_signal_collector_,
              GetSignal(kOtherFakeSignalName, _, _))
      .Times(1);

  EXPECT_CALL(*fake_signal_collector_, GetSignal(_, _, _)).Times(0);

  base::test::TestFuture<base::Value::Dict> future;
  aggregator_->GetSignals(parameters, future.GetCallback());

  base::Value::Dict expected_value;
  expected_value.Set(kOtherFakeSignalName, GetOtherFakeSignalValue());

  EXPECT_EQ(future.Get(), expected_value);
}

// Tests how the aggregator behaves when given a parameter with a single signal
// that no collector supports.
TEST_F(SignalsAggregatorImplTest, GetSignals_SingleSignal_Unsupported) {
  base::Value::Dict parameters;
  parameters.Set("something unsupported", base::Value());

  base::test::TestFuture<base::Value::Dict> future;
  aggregator_->GetSignals(parameters, future.GetCallback());
  EXPECT_EQ(future.Get(), base::Value::Dict());
}

}  // namespace device_signals
