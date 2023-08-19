// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// One constant `kFoo` for each event
// `AutofillManager::Observer::On{Before,After}Foo()`.
enum class AutofillManagerEvent {
  kLanguageDetermined,
  kFormsSeen,
  kTextFieldDidChange,
  kTextFieldDidScroll,
  kSelectControlDidChange,
  kAskForValuesToFill,
  kDidFillAutofillFormData,
  kJavaScriptChangedAutofilledValue,
  kFormSubmitted,
  kMaxValue = kFormSubmitted
};

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
//                                    {AutofillManagerEvent::kFoo,
//                                     AutofillManagerEvent::kBar,
//                                     ...});
//   ... trigger events ...
//   ASSERT_TRUE(waiter.Wait());  // Blocks.
//
// In browser tests, it may be necessary to tell Wait() to wait for at least,
// say, 1 event because triggering events is asynchronous due to Mojo:
//
//   TestAutofillManagerWaiter waiter(manager,
//                                    {AutofillManagerEvent::kFoo,
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
  using Event = AutofillManagerEvent;

  explicit TestAutofillManagerWaiter(AutofillManager& manager,
                                     DenseSet<Event> relevant_events = {});
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

    EventCount& GetOrCreate(Event event, base::Location location);
    EventCount* Get(Event event);

    size_t num_total_calls() const;
    size_t num_pending_calls() const;

    std::string Describe() const;

    // The std::map guarantees that references aren't invalidated by
    // GetOrCreate().
    std::map<Event, EventCount> events;
    // Decrement() unblocks Wait() when the number of awaited calls reaches 0.
    size_t num_awaiting_total_calls = std::numeric_limits<size_t>::max();
    // Running iff there are no awaited and no pending calls.
    base::RunLoop run_loop = base::RunLoop();
    // True iff the `run_loop` ran and timed out.
    bool timed_out = false;
    // Functions that access the state should be mutually exclusive.
    base::Lock lock;
  };

  bool IsRelevant(Event event) const;
  void Increment(Event event, base::Location location = FROM_HERE);
  void Decrement(Event event, base::Location location = FROM_HERE);

  void OnAutofillManagerDestroyed(AutofillManager& manager) override;
  void OnAutofillManagerReset(AutofillManager& manager) override;

  void OnBeforeLanguageDetermined(AutofillManager& manager) override;
  void OnAfterLanguageDetermined(AutofillManager& manager) override;

  void OnBeforeFormsSeen(AutofillManager& manager,
                         base::span<const FormGlobalId> forms) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> forms) override;

  void OnBeforeTextFieldDidChange(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field) override;
  void OnAfterTextFieldDidChange(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field) override;

  void OnBeforeTextFieldDidScroll(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field) override;
  void OnAfterTextFieldDidScroll(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field) override;

  void OnBeforeSelectControlDidChange(AutofillManager& manager,
                                      FormGlobalId form,
                                      FieldGlobalId field) override;
  void OnAfterSelectControlDidChange(AutofillManager& manager,
                                     FormGlobalId form,
                                     FieldGlobalId field) override;

  void OnBeforeAskForValuesToFill(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field) override;
  void OnAfterAskForValuesToFill(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field) override;

  void OnBeforeDidFillAutofillFormData(AutofillManager& manager,
                                       FormGlobalId form) override;
  void OnAfterDidFillAutofillFormData(AutofillManager& manager,
                                      FormGlobalId form) override;

  void OnBeforeJavaScriptChangedAutofilledValue(AutofillManager& manager,
                                                FormGlobalId form,
                                                FieldGlobalId field) override;
  void OnAfterJavaScriptChangedAutofilledValue(AutofillManager& manager,
                                               FormGlobalId form,
                                               FieldGlobalId field) override;

  void OnFormSubmitted(AutofillManager& manager, FormGlobalId form) override;

  DenseSet<Event> relevant_events_;
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
