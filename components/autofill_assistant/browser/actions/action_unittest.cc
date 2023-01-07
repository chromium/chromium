// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::SaveArgPointee;

class TimeTicksOverride {
 public:
  static base::TimeTicks Now() { return now_ticks_; }

  static base::TimeTicks now_ticks_;
};

// static
base::TimeTicks TimeTicksOverride::now_ticks_ = base::TimeTicks::Now();

class FakeAction : public Action {
 public:
  FakeAction(ActionDelegate* delegate, const ActionProto& proto)
      : Action(delegate, proto) {}

 private:
  void InternalProcessAction(ProcessActionCallback callback) override {
    Selector selector;
    delegate_->ShortWaitForElementWithSlowWarning(
        selector,
        base::BindOnce(
            &FakeAction::OnWaitForElementTimed, weak_ptr_factory_.GetWeakPtr(),
            base::BindOnce(&FakeAction::OnWaitForElement,
                           base::Unretained(this), std::move(callback))));
  }

  void OnWaitForElement(ProcessActionCallback callback,
                        const ClientStatus& status) {
    UpdateProcessedAction(status);
    std::move(callback).Run(std::move(processed_action_proto_));
  }
};

class FakeActionTest : public testing::Test {
 public:
  void SetUp() override {}

 protected:
  ClientStatus ClientStatusWithWarning(SlowWarningStatus warning_status) {
    ClientStatus status = OkClientStatus();
    status.set_slow_warning_status(warning_status);
    return status;
  }

  void Run() {
    ActionProto action_proto;
    action_proto.set_action_delay_ms(1000);
    FakeAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
};

ACTION_P(Delay, delay) {
  TimeTicksOverride::now_ticks_ += base::Seconds(delay);
}

TEST_F(FakeActionTest, WaitForDomActionTest) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  InSequence sequence;

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
      .WillOnce(DoAll(Delay(2), RunOnceCallback<1>(OkClientStatus(),
                                                   base::Milliseconds(500))));

  ProcessedActionProto processed_proto;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_proto));
  Run();

  EXPECT_EQ(processed_proto.timing_stats().delay_ms(), 1000);
  EXPECT_EQ(processed_proto.timing_stats().active_time_ms(), 1500);
  EXPECT_EQ(processed_proto.timing_stats().wait_time_ms(), 500);
}

TEST_F(FakeActionTest, SlowWarningShownTest) {
  base::subtle::ScopedTimeClockOverrides overrides(
      nullptr, &TimeTicksOverride::Now, nullptr);
  InSequence sequence;

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
      .WillOnce(DoAll(Delay(2),
                      RunOnceCallback<1>(ClientStatusWithWarning(WARNING_SHOWN),
                                         base::Milliseconds(500))));

  ProcessedActionProto processed_proto;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_proto));
  Run();

  EXPECT_EQ(processed_proto.slow_warning_status(), WARNING_SHOWN);
}

}  // namespace
}  // namespace autofill_assistant
