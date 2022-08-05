// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager_waiter.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_run_loop_timeout.h"

namespace autofill {

TestAutofillManagerWaiter::State::State() = default;
TestAutofillManagerWaiter::State::~State() = default;

TestAutofillManagerWaiter::EventCount* TestAutofillManagerWaiter::State::Get(
    AfterEvent event) {
  auto it = base::ranges::find(events, event, &EventCount::event);
  return it != events.end() ? &*it : nullptr;
}

TestAutofillManagerWaiter::EventCount&
TestAutofillManagerWaiter::State::GetOrCreate(AfterEvent event,
                                              base::Location location) {
  if (EventCount* e = Get(event))
    return *e;
  return *events.insert(events.end(), {event, location});
}

size_t TestAutofillManagerWaiter::State::num_pending_calls() const {
  size_t pending_calls = 0;
  for (const EventCount& e : events)
    pending_calls += e.num_pending_calls;
  return pending_calls;
}

size_t TestAutofillManagerWaiter::State::num_total_calls() const {
  size_t total_calls = 0;
  for (const EventCount& e : events)
    total_calls += e.num_total_calls;
  return total_calls;
}

std::string TestAutofillManagerWaiter::State::Describe() const {
  std::vector<std::string> strings;
  for (const auto& e : events) {
    strings.push_back(base::StringPrintf(
        "[event=%s, pending=%zu, total=%zu]", e.location.function_name(),
        e.num_pending_calls, e.num_total_calls));
  }
  return base::JoinString(strings, ", ");
}

TestAutofillManagerWaiter::TestAutofillManagerWaiter(
    AutofillManager& manager,
    std::initializer_list<AfterEvent> relevant_events)
    : relevant_events_(relevant_events) {
  observation_.Observe(&manager);
}

TestAutofillManagerWaiter::~TestAutofillManagerWaiter() = default;

void TestAutofillManagerWaiter::OnAutofillManagerDestroyed() {
  observation_.Reset();
}

void TestAutofillManagerWaiter::OnAutofillManagerReset() {
  Reset();
}

void TestAutofillManagerWaiter::OnBeforeLanguageDetermined() {
  Increment(&AutofillManager::Observer::OnAfterLanguageDetermined);
}

void TestAutofillManagerWaiter::OnAfterLanguageDetermined() {
  Decrement(&AutofillManager::Observer::OnAfterLanguageDetermined);
}

void TestAutofillManagerWaiter::OnBeforeFormsSeen() {
  Increment(&AutofillManager::Observer::OnAfterFormsSeen);
}

void TestAutofillManagerWaiter::OnAfterFormsSeen() {
  Decrement(&AutofillManager::Observer::OnAfterFormsSeen);
}

void TestAutofillManagerWaiter::OnBeforeTextFieldDidChange() {
  Increment(&AutofillManager::Observer::OnAfterTextFieldDidChange);
}

void TestAutofillManagerWaiter::OnAfterTextFieldDidChange() {
  Decrement(&AutofillManager::Observer::OnAfterTextFieldDidChange);
}

void TestAutofillManagerWaiter::OnBeforeAskForValuesToFill() {
  Increment(&AutofillManager::Observer::OnAfterAskForValuesToFill);
}

void TestAutofillManagerWaiter::OnAfterAskForValuesToFill() {
  Decrement(&AutofillManager::Observer::OnAfterAskForValuesToFill);
}

void TestAutofillManagerWaiter::OnBeforeDidFillAutofillFormData() {
  Increment(&AutofillManager::Observer::OnAfterDidFillAutofillFormData);
}

void TestAutofillManagerWaiter::OnAfterDidFillAutofillFormData() {
  Decrement(&AutofillManager::Observer::OnAfterDidFillAutofillFormData);
}

void TestAutofillManagerWaiter::OnBeforeJavaScriptChangedAutofilledValue() {
  Increment(
      &AutofillManager::Observer::OnAfterJavaScriptChangedAutofilledValue);
}

void TestAutofillManagerWaiter::OnAfterJavaScriptChangedAutofilledValue() {
  Decrement(
      &AutofillManager::Observer::OnAfterJavaScriptChangedAutofilledValue);
}

void TestAutofillManagerWaiter::Reset() {
  // The declaration order ensures that `lock` is destroyed before `state`, so
  // that `state_->lock` has been released at its own destruction time.
  auto state = std::make_unique<State>();
  base::AutoLock lock(state_->lock);
  VLOG(1) << __func__;
  ASSERT_EQ(state_->num_pending_calls(), 0u) << state_->Describe();
  using std::swap;
  swap(state_, state);
}

bool TestAutofillManagerWaiter::IsRelevant(AfterEvent event) const {
  return relevant_events_.empty() || base::Contains(relevant_events_, event);
}

void TestAutofillManagerWaiter::Increment(AfterEvent event,
                                          base::Location location) {
  base::AutoLock lock(state_->lock);
  if (!IsRelevant(event)) {
    VLOG(1) << "Ignoring irrelevant event: " << __func__ << "("
            << location.function_name() << ")";
    return;
  }
  if (state_->run_loop.AnyQuitCalled()) {
    VLOG(1) << "Ignoring event because no more calls are awaited: " << __func__
            << "(" << location.function_name() << ")";
    return;
  }
  VLOG(1) << __func__ << "(" << location.function_name() << ")";
  EventCount& e = state_->GetOrCreate(event, location);
  e.location = location;
  ++e.num_total_calls;
  ++e.num_pending_calls;
}

void TestAutofillManagerWaiter::Decrement(AfterEvent event,
                                          base::Location location) {
  base::AutoLock lock(state_->lock);
  if (!IsRelevant(event)) {
    VLOG(1) << "Ignoring irrelevant event: " << __func__ << "("
            << location.function_name() << ")";
    return;
  }
  if (state_->run_loop.AnyQuitCalled()) {
    VLOG(1) << "Ignoring event because no more calls are awaited: " << __func__
            << "(" << location.function_name() << ")";
    return;
  }
  VLOG(1) << __func__ << "(" << location.function_name() << ")";
  EventCount* e = state_->Get(event);
  ASSERT_TRUE(e) << state_->Describe();
  ASSERT_GT(e->num_pending_calls, 0u) << state_->Describe();
  if (state_->num_awaiting_total_calls > 0)
    --state_->num_awaiting_total_calls;
  --e->num_pending_calls;
  if (state_->num_pending_calls() == 0 && state_->num_awaiting_total_calls == 0)
    state_->run_loop.Quit();
}

testing::AssertionResult TestAutofillManagerWaiter::Wait(
    size_t num_awaiting_calls) {
  base::ReleasableAutoLock lock(&state_->lock);
  if (state_->run_loop.AnyQuitCalled()) {
    return testing::AssertionFailure()
           << "Waiter has not been Reset() since last Wait().";
  }
  // Some events may already have happened.
  num_awaiting_calls = num_awaiting_calls > state_->num_total_calls()
                           ? num_awaiting_calls - state_->num_total_calls()
                           : 0u;
  if (state_->num_pending_calls() > 0 || num_awaiting_calls > 0) {
    base::test::ScopedRunLoopTimeout run_loop_timeout(
        FROM_HERE, timeout_,
        base::BindRepeating(&State::Describe, base::Unretained(state_.get())));
    state_->num_awaiting_total_calls = num_awaiting_calls;
    lock.Release();
    state_->run_loop.Run();
  }
  return testing::AssertionSuccess();
}

}  // namespace autofill
