// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_test_base.h"

namespace autofill_assistant {

namespace {

using ::testing::WithArgs;

}

WaitForDomTestBase::WaitForDomTestBase()
    : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  ON_CALL(mock_web_controller_, FindElement)
      .WillByDefault(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_action_delegate_, WaitForDom)
      .WillRepeatedly(Invoke(this, &WaitForDomTestBase::FakeWaitForDom));
}

WaitForDomTestBase::~WaitForDomTestBase() = default;

void WaitForDomTestBase::FakeWaitForDom(
    base::TimeDelta max_wait_time,
    bool allow_observer_mode,
    bool allow_interrupt,
    WaitForDomObserver* observer,
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
        done_waiting_callback) {
  fake_wait_for_dom_done_ = std::move(done_waiting_callback);
  RunFakeWaitForDom(check_elements);
}

void WaitForDomTestBase::RunFakeWaitForDom(
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements) {
  if (!fake_wait_for_dom_done_)
    return;

  checker_ = std::make_unique<BatchElementChecker>();
  has_check_elements_result_ = false;
  check_elements.Run(checker_.get(),
                     base::BindOnce(&WaitForDomTestBase::OnCheckElementsDone,
                                    base::Unretained(this)));
  task_env_.FastForwardBy(base::Milliseconds(fake_check_time_));
  checker_->AddAllDoneCallback(
      base::BindOnce(&WaitForDomTestBase::OnWaitForDomDone,
                     base::Unretained(this), check_elements));
  checker_->Run(&mock_web_controller_);
}

void WaitForDomTestBase::OnCheckElementsDone(const ClientStatus& result) {
  ASSERT_FALSE(has_check_elements_result_);  // Duplicate calls
  has_check_elements_result_ = true;
  check_elements_result_ = result;
}

void WaitForDomTestBase::OnWaitForDomDone(
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements) {
  ASSERT_TRUE(has_check_elements_result_);  // OnCheckElementsDone() not called

  if (!fake_wait_for_dom_done_)
    return;

  // If we manually set |wait_for_dom_status_| to an error status we simulate
  // the WaitForDom ending in an error.
  if (!wait_for_dom_status_.ok()) {
    std::move(fake_wait_for_dom_done_)
        .Run(wait_for_dom_status_, base::Milliseconds(0));
    return;
  }

  if (check_elements_result_.ok()) {
    std::move(fake_wait_for_dom_done_)
        .Run(check_elements_result_, base::Milliseconds(fake_wait_time_));
  } else {
    wait_for_dom_timer_ = std::make_unique<base::OneShotTimer>();
    wait_for_dom_timer_->Start(
        FROM_HERE, base::Seconds(1),
        base::BindOnce(&WaitForDomTestBase::RunFakeWaitForDom,
                       base::Unretained(this), check_elements));
  }
}

}  // namespace autofill_assistant
