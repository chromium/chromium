// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_

#include <list>
#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Records AutofillManager::Observer::OnBeforeFoo() events and blocks until the
// corresponding OnAfterFoo() events have happened.
//
// This mechanism relies on AutofillManager::Observer's guarantee that
// OnBeforeFoo() is followed by OnAfterFoo() in normal circumstances.
//
// If an OnBeforeFoo() event happens multiple times, the waiter expects multiple
// OnAfterFoo() events.
//
// Which events Wait() should be waiting for can be limited by providing a list
// of `relevant_events` to the constructor. This list should contain the
// OnAfterFoo(), *not* the OnBeforeFoo() events.
//
// By default, Wait() succeeds immediately if there are no pending calls, that
// is, if no OnBeforeFoo() without matching OnAfterFoo() has been observed.
// Calling Wait(k) with an integer argument `k` overrides this behaviour: in
// this case, it expects a total of at least `k` OnAfterFoo() events to happen
// or have happened.
//
// The waiter resets itself on OnAutofillManagerDestroyed() events. This makes
// it suitable for use with TestAutofillManagerInjector.
//
// Typical usage is as follows:
//
//   TestAutofillManagerWaiter waiter(manager,
//                                    {&AutofillManager::Observer::OnAfterFoo,
//                                     &AutofillManager::Observer::OnAfterBar,
//                                     ...});
//   ... trigger events ...
//   ASSERT_TRUE(waiter.Wait());  // Blocks.
//
// In browser tests, it may be necessary to tell Wait() to wait for at least,
// say, 1 event because triggering events is asynchronous due to Mojo:
//
//   TestAutofillManagerWaiter waiter(manager,
//                                    {&AutofillManager::Observer::OnAfterFoo,
//                                     ...});
//   ... trigger asynchronous OnFoo event ...
//   ASSERT_TRUE(waiter.Wait(1));  // Blocks until at least one OnFoo() event
//                                 // has happened since the creation of
//                                 // `waiter`.
//
// In case of failure, the error message of Wait() informs about the pending
// OnAfterFoo() calls.
class TestAutofillManagerWaiter : public AutofillManager::Observer {
 public:
  // An OnFooAfter() event. As a convention, throughout this class we use the
  // OnAfterFoo() events to identify the pair of OnAfterFoo() / OnBeforeFoo().
  using AfterEvent = void (AutofillManager::Observer::*)();

  explicit TestAutofillManagerWaiter(
      AutofillManager& manager,
      std::initializer_list<AfterEvent> relevant_events = {});
  TestAutofillManagerWaiter(const TestAutofillManagerWaiter&) = delete;
  TestAutofillManagerWaiter& operator=(const TestAutofillManagerWaiter&) =
      delete;
  ~TestAutofillManagerWaiter() override;

  // Blocks until all pending OnAfterFoo() events have been observed and at
  // least `num_awaiting_calls` relevant events have been observed.
  [[nodiscard]] testing::AssertionResult Wait(size_t num_awaiting_calls = 0);

  // Equivalent to re-initialization.
  void Reset();

  // The timeout of the RunLoop. Since asynchronous-parsing thread in
  // AutofillManager runs at relatively low priority, a high timeout may be
  // necessary on slow bots.
  base::TimeDelta timeout() const { return timeout_; }
  void set_timeout(base::TimeDelta timeout) { timeout_ = timeout; }

 private:
  struct EventCount {
    // An AutofillManager::Observer::OnAfterFoo() event.
    AfterEvent event;
    // The OnBeforeFoo() function. Used for meaningful error messages.
    base::Location location;
    // The total number of recorded OnBeforeFoo() events.
    size_t num_total_calls = 0;
    // The number of recorded OnBeforeFoo() events without a corresponding
    // OnAfterFoo() event.
    size_t num_pending_calls = 0;
  };

  // State variables for easy resetting.
  struct State {
    State();
    State(State&) = delete;
    State& operator=(State&) = delete;
    ~State();

    EventCount& GetOrCreate(AfterEvent event, base::Location location);
    EventCount* Get(AfterEvent event);

    size_t num_total_calls() const;
    size_t num_pending_calls() const;

    std::string Describe() const;

    // Effectively a map from `AfterEvent` to its count. Since `AfterEvent` is
    // only equality-comparable, we use a list. (The list, rather than a vector,
    // avoids invalidation of the references returned by GetOrCreate().)
    std::list<EventCount> events;
    // Decrement() unblocks Wait() when the number of awaited calls reaches 0.
    size_t num_awaiting_total_calls = std::numeric_limits<size_t>::max();
    // Running iff there are no awaited and no pending calls.
    base::RunLoop run_loop = base::RunLoop();
    // True iff the `run_loop` ran and timed out.
    bool timed_out = false;
    // Functions that access the state should be mutually exclusive.
    base::Lock lock;
  };

  bool IsRelevant(AfterEvent event) const;
  void Increment(AfterEvent event,
                 base::Location location = base::Location::Current());
  void Decrement(AfterEvent event,
                 base::Location location = base::Location::Current());

  void OnAutofillManagerDestroyed() override;
  void OnAutofillManagerReset() override;

  void OnBeforeLanguageDetermined() override;
  void OnAfterLanguageDetermined() override;

  void OnBeforeFormsSeen() override;
  void OnAfterFormsSeen() override;

  void OnBeforeTextFieldDidChange() override;
  void OnAfterTextFieldDidChange() override;

  void OnBeforeAskForValuesToFill() override;
  void OnAfterAskForValuesToFill() override;

  void OnBeforeDidFillAutofillFormData() override;
  void OnAfterDidFillAutofillFormData() override;

  void OnBeforeJavaScriptChangedAutofilledValue() override;
  void OnAfterJavaScriptChangedAutofilledValue() override;

  void OnBeforeFormSubmitted() override;
  void OnAfterFormSubmitted() override;

  std::vector<AfterEvent> relevant_events_;
  std::unique_ptr<State> state_ = std::make_unique<State>();
  base::TimeDelta timeout_ = base::Seconds(30);
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation_{this};
};

// Returns a FormStructure that satisfies `pred` if such a form exists at call
// time or appears within a RunLoop's timeout. Returns nullptr if no such form
// exists.
const FormStructure* WaitForMatchingForm(
    AutofillManager* manager,
    base::RepeatingCallback<bool(const FormStructure&)> pred,
    base::TimeDelta timeout = base::Seconds(30));

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_
