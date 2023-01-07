// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_TEST_BASE_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class WaitForDomTestBase : public testing::Test {
 public:
  WaitForDomTestBase();
  ~WaitForDomTestBase() override;

 protected:
  // Fakes ActionDelegate::WaitForDom.
  //
  // This simulates a WaitForDom that calls |check_elements_| every seconds
  // until it gets a successful callback, then calls done_waiting_callback.
  void FakeWaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_observer_mode,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
          done_waiting_callback);

  void RunFakeWaitForDom(
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements);

  // Called from the check_elements callback passed to FakeWaitForDom.
  void OnCheckElementsDone(const ClientStatus& result);

  // Called by |checker_| once it's done and either ends the WaitForDom or
  // schedule another run.
  void OnWaitForDomDone(
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements);

  // task_env_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_env_;

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
      fake_wait_for_dom_done_;
  std::unique_ptr<BatchElementChecker> checker_;
  bool has_check_elements_result_ = false;
  ClientStatus check_elements_result_;
  std::unique_ptr<base::OneShotTimer> wait_for_dom_timer_;
  int fake_wait_time_ = 0;
  int fake_check_time_ = 0;
  ClientStatus wait_for_dom_status_ = OkClientStatus();
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_TEST_BASE_H_
