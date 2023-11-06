// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_

#include <map>
#include <memory>

#include "base/containers/span.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
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
                                 FieldGlobalId field,
                                 std::u16string text_value) override;

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
                                  FieldGlobalId field,
                                  const FormData& form_data) override;

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

namespace internal {

// Observes a single event and quits a base::RunLoop on the first event that
// matches a given predicate.
//
// Helper class for WaitForEvent().
//
// For every AutofillManager::Observer::OnFoo(Args... args) event, this class
// should have an override of the form
//
//   void OnFoo(Args... args) {
//     MaybeQuit(&Observer::OnFoo, args...);
//   }
template <typename... Args>
class AutofillManagerSingleEventWaiter : public AutofillManager::Observer {
 public:
  using Event = void (Observer::*)(Args...);

  explicit AutofillManagerSingleEventWaiter(
      AutofillManager& manager,
      Event event,
      testing::Matcher<std::tuple<Args...>> matcher)
      : event_(std::forward<Event>(event)), matcher_(matcher) {
    observation_.Observe(&manager);
  }

  ~AutofillManagerSingleEventWaiter() override = default;

  testing::AssertionResult Wait(base::TimeDelta timeout,
                                const base::Location& location) && {
    bool timed_out = false;
    base::test::ScopedRunLoopTimeout run_loop_timeout(
        FROM_HERE, timeout,
        base::BindRepeating(
            [](bool* timed_out, const base::Location& location) {
              *timed_out = true;
              return std::string("Timeout callback from WaitForEvent() in ") +
                     location.ToString();
            },
            base::Unretained(&timed_out), location));
    run_loop_.Run(location);
    return !timed_out ? testing::AssertionSuccess()
                      : testing::AssertionFailure()
                            << "Timeout of callback from WaitForEvent() in "
                            << location.ToString();
  }

 private:
  // AutofillManager::Observer:
  void OnAutofillManagerDestroyed(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnAutofillManagerDestroyed, manager);
  }
  void OnAutofillManagerReset(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnAutofillManagerReset, manager);
  }
  void OnBeforeLanguageDetermined(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnBeforeLanguageDetermined, manager);
  }
  void OnAfterLanguageDetermined(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnAfterLanguageDetermined, manager);
  }
  void OnBeforeFormsSeen(AutofillManager& manager,
                         base::span<const FormGlobalId> forms) override {
    MaybeQuit(&Observer::OnBeforeFormsSeen, manager, forms);
  }
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> forms) override {
    MaybeQuit(&Observer::OnAfterFormsSeen, manager, forms);
  }
  void OnBeforeTextFieldDidChange(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field) override {
    MaybeQuit(&Observer::OnBeforeTextFieldDidChange, manager, form, field);
  }
  void OnAfterTextFieldDidChange(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field,
                                 std::u16string text_value) override {
    MaybeQuit(&Observer::OnAfterTextFieldDidChange, manager, form, field,
              text_value);
  }
  void OnBeforeTextFieldDidScroll(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field) override {
    MaybeQuit(&Observer::OnBeforeTextFieldDidScroll, manager, form, field);
  }
  void OnAfterTextFieldDidScroll(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field) override {
    MaybeQuit(&Observer::OnAfterTextFieldDidScroll, manager, form, field);
  }
  void OnBeforeSelectControlDidChange(AutofillManager& manager,
                                      FormGlobalId form,
                                      FieldGlobalId field) override {
    MaybeQuit(&Observer::OnBeforeSelectControlDidChange, manager, form, field);
  }
  void OnAfterSelectControlDidChange(AutofillManager& manager,
                                     FormGlobalId form,
                                     FieldGlobalId field) override {
    MaybeQuit(&Observer::OnAfterSelectControlDidChange, manager, form, field);
  }
  void OnBeforeAskForValuesToFill(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field,
                                  const FormData& form_data) override {
    MaybeQuit(&Observer::OnBeforeAskForValuesToFill, manager, form, field,
              form_data);
  }
  void OnAfterAskForValuesToFill(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field) override {
    MaybeQuit(&Observer::OnAfterAskForValuesToFill, manager, form, field);
  }
  void OnBeforeDidFillAutofillFormData(AutofillManager& manager,
                                       FormGlobalId form) override {
    MaybeQuit(&Observer::OnBeforeDidFillAutofillFormData, manager, form);
  }
  void OnAfterDidFillAutofillFormData(AutofillManager& manager,
                                      FormGlobalId form) override {
    MaybeQuit(&Observer::OnAfterDidFillAutofillFormData, manager, form);
  }
  void OnBeforeJavaScriptChangedAutofilledValue(AutofillManager& manager,
                                                FormGlobalId form,
                                                FieldGlobalId field) override {
    MaybeQuit(&Observer::OnBeforeDidFillAutofillFormData, manager, form, field);
  }
  void OnAfterJavaScriptChangedAutofilledValue(AutofillManager& manager,
                                               FormGlobalId form,
                                               FieldGlobalId field) override {
    MaybeQuit(&Observer::OnAfterDidFillAutofillFormData, manager, form, field);
  }
  void OnBeforeLoadedServerPredictions(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnBeforeLoadedServerPredictions, manager);
  }
  void OnAfterLoadedServerPredictions(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnAfterLoadedServerPredictions, manager);
  }
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              FieldTypeSource source) override {
    MaybeQuit(&Observer::OnFieldTypesDetermined, manager, form, source);
  }
  void OnSuggestionsShown(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnSuggestionsShown, manager);
  }
  void OnSuggestionsHidden(AutofillManager& manager) override {
    MaybeQuit(&Observer::OnSuggestionsHidden, manager);
  }
  void OnFillOrPreviewDataModelForm(
      AutofillManager& manager,
      autofill::FormGlobalId form,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData* const> filled_fields,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card) override {
    MaybeQuit(&Observer::OnFillOrPreviewDataModelForm, manager, form,
              action_persistence, filled_fields, profile_or_credit_card);
  }
  void OnFormSubmitted(AutofillManager& manager, FormGlobalId form) override {
    MaybeQuit(&Observer::OnFormSubmitted, manager, form);
  }

  // Quits the `run_loop_` if `event` matches `event_`.
  //
  // `event` must be a pointer to an AutofillManager::Observer member function.
  template <typename CandidateEvent, typename... CandidateArgs>
  void MaybeQuit(const CandidateEvent& event, CandidateArgs&&... args) {
    if constexpr (std::is_same_v<Event, CandidateEvent>) {
      if (event_ == event && matcher_.Matches(std::forward_as_tuple(
                                 std::forward<CandidateArgs>(args)...))) {
        run_loop_.Quit();
      }
    }
  }

  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation_{this};
  Event event_ = nullptr;
  testing::Matcher<std::tuple<Args...>> matcher_;
  base::RunLoop run_loop_;
};

}  // namespace internal

// Returns a callback that waits for a given `event` whose arguments are matched
// by `matcher`.
//
// The `event` must be a pointer to an AutofillManager::Observer member
// function. When adding a new event to AutofillManager::Observer, a
// corresponding override must be added to AutofillManagerSingleEventWaiter.
//
// Typical usage is as follows:
//
//   base::OnceCallback<bool()> preview_waiter = WaitForEvent(
//      *autofill_manager,
//      &AutofillManager::Observer::OnFillOrPreviewDataModelForm,
//      testing::Args<2>(mojom::ActionPersistence::kPreview));
//   ...
//   EXPECT_TRUE(std::move(preview_waiter).Run());
template <typename Matcher, typename R, typename... Args>
base::OnceCallback<testing::AssertionResult()> WaitForEvent(
    AutofillManager& manager,
    R (AutofillManager::Observer::*event)(Args...),
    Matcher matcher,
    base::TimeDelta timeout = base::Seconds(1),
    const base::Location& location = FROM_HERE) {
  using T = internal::AutofillManagerSingleEventWaiter<Args...>;
  return base::BindOnce(
      [](std::unique_ptr<T> observer, base::TimeDelta timeout,
         const base::Location& location) {
        return std::move(*observer).Wait(timeout, std::move(location));
      },
      std::make_unique<T>(manager, event, matcher), timeout, location);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_
