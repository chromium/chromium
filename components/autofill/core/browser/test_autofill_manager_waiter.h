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
  kCaretMovedInFormField,
  kTextFieldDidChange,
  kTextFieldDidScroll,
  kSelectControlDidChange,
  kAskForValuesToFill,
  kFocusOnFormField,
  kDidFillAutofillFormData,
  kJavaScriptChangedAutofilledValue,
  kFormSubmitted,
  kLoadedServerPredictions,
  kMaxValue = kLoadedServerPredictions
};

// Records AutofillManager::Observer::OnBeforeFoo() events and blocks until the
// corresponding OnAfterFoo() events have happened.
//
// This mechanism relies on AutofillManager::Observer's guarantee that
// OnBeforeFoo() is followed by OnAfterFoo() in normal circumstances.
//
// If an OnBeforeFoo() event happens multiple times, the waiter expects multiple
// OnAfterFoo() events. Which events Wait() should be waiting for can be limited
// by providing a list of `relevant_events` to the constructor.
//
// `Wait(k)` blocks until
// - at least `k` relevant OnBeforeFoo() events have been seen and
// - for every observed (relevant or non-relevant) OnBeforeFoo() event the
//   associated OnAfterFoo() event has happened.
//
// The waiter resets itself on OnAutofillManagerStateChanged() events for
// kPendingReset. This makes it suitable for use with
// TestAutofillManagerInjector.
//
// Typical usage in unit tests is as follows:
//
//   TestAutofillManagerWaiter waiter(manager,
//                                    {AutofillManagerEvent::kFoo,
//                                     AutofillManagerEvent::kBar,
//                                     ...});
//   ... trigger events ...
//   ASSERT_TRUE(waiter.Wait());  // Blocks.
//
// In browser tests, it is important to create the waiter soon enough and not
// create it between two On{Before,After}Foo() events:
//
//   class MyAutofillManager : public BrowserAutofillManager {
//    public:
//     ...
//     TestAutofillManagerWaiter waiter_{*this,
//                                       {AutofillManagerEvent::kFormsSeen,
//                                        ...}};
//   };
//
//   class MyFixture : public InProcessBrowserTest {
//    public:
//     ...
//     TestAutofillManagerInjector<MyAutofillManager> injector_;
//   };
//
//   TEST_F(MyFixture, MyTest) {
//     NavigateToUrl("https://foo.com");
//     ASSERT_TRUE(injector_[main_rfh()].waiter_.Wait(
//         /*num_expected_relevant_events=*/1));
//   }
//
// In case of failure, the error message of Wait() informs about the pending
// OnAfterFoo() calls.
class TestAutofillManagerWaiter : public AutofillManager::Observer {
 public:
  using Event = AutofillManagerEvent;

  explicit TestAutofillManagerWaiter(AutofillManager& manager,
                                     DenseSet<Event> relevant_events = {},
                                     base::Location location = FROM_HERE);
  TestAutofillManagerWaiter(const TestAutofillManagerWaiter&) = delete;
  TestAutofillManagerWaiter& operator=(const TestAutofillManagerWaiter&) =
      delete;
  ~TestAutofillManagerWaiter() override;

  // Blocks until all pending OnAfterFoo() events have been observed and at
  // least `num_expected_relevant_events` relevant events have been observed
  // since the waiter's creation or last Wait().
  //
  // Since the asynchronous-parsing task runner in AutofillManager has
  // relatively low priority, a high timeout may be necessary on slow bots.
  [[nodiscard]] testing::AssertionResult Wait(
      size_t num_expected_relevant_events = 0,
      base::TimeDelta timeout = base::Seconds(30),
      const base::Location& location = FROM_HERE);

 private:
  struct EventCount {
    // The OnBeforeFoo() function. Used for meaningful error messages.
    base::Location location;
    // The total number of recorded OnBeforeFoo() events.
    size_t num_before_events = 0;
    // The total number of recorded OnAfterFoo() events.
    size_t num_after_events = 0;
  };

  // State variables for easy resetting.
  struct State {
    State();
    State(State&) = delete;
    State& operator=(State&) = delete;
    ~State();

    EventCount& GetOrCreate(Event event, const base::Location& location);
    EventCount* Get(Event event);

    // The std::map guarantees that references aren't invalidated by
    // GetOrCreate().
    std::map<Event, EventCount> events;
    // OnAfter() unblocks Wait() only once all expected events have been seen.
    size_t num_expected_relevant_events = std::numeric_limits<size_t>::max();
    // Runs iff there are open expectations (i.e., not enough relevant events
    // have been seen) or pending events (i.e., we're in between OnBeforeFoo()
    // and OnAfterFoo()).
    base::RunLoop run_loop;
    // True iff the `run_loop` ran and timed out.
    bool timed_out = false;
    // Functions that access the state should be mutually exclusive.
    base::Lock lock;
  };

  std::string DescribeState() const;

  size_t num_pending_events() const;
  size_t num_completed_relevant_events() const;

  bool IsRelevant(Event event) const;
  void OnBefore(Event event, const base::Location& location = FROM_HERE);
  void OnAfter(Event event, const base::Location& location = FROM_HERE);

  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillManager::LifecycleState old_state,
      AutofillManager::LifecycleState new_state) override;

  void OnBeforeLanguageDetermined(AutofillManager& manager) override;
  void OnAfterLanguageDetermined(AutofillManager& manager) override;

  void OnBeforeFormsSeen(AutofillManager& manager,
                         base::span<const FormGlobalId> updated_forms,
                         base::span<const FormGlobalId> removed_forms) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;

  void OnBeforeCaretMovedInFormField(AutofillManager& manager,
                                     const FormGlobalId& form,
                                     const FieldGlobalId& field_id,
                                     const std::u16string& selection,
                                     const gfx::Rect& caret_bounds) override;
  void OnAfterCaretMovedInFormField(AutofillManager& manager,
                                    const FormGlobalId& form,
                                    const FieldGlobalId& field_id,
                                    const std::u16string& selection,
                                    const gfx::Rect& caret_bounds) override;

  void OnBeforeTextFieldDidChange(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field) override;
  void OnAfterTextFieldDidChange(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field,
                                 const std::u16string& text_value) override;

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

  void OnBeforeFocusOnFormField(AutofillManager& manager,
                                FormGlobalId form,
                                FieldGlobalId field) override;
  void OnAfterFocusOnFormField(AutofillManager& manager,
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

  void OnFormSubmitted(AutofillManager& manager, const FormData& form) override;

  void OnBeforeLoadedServerPredictions(AutofillManager& manager) override;
  void OnAfterLoadedServerPredictions(AutofillManager& manager) override;

  DenseSet<Event> relevant_events_;
  std::unique_ptr<State> state_ = std::make_unique<State>();
  base::TimeDelta timeout_ = base::Seconds(30);
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation_{this};
  base::Location waiter_location_;
};

// Returns a FormStructure that satisfies `pred` if such a form exists at call
// time or appears within a RunLoop's timeout. Returns nullptr otherwise.
const FormStructure* WaitForMatchingForm(
    AutofillManager* manager,
    base::RepeatingCallback<bool(const FormStructure&)> pred,
    base::TimeDelta timeout = base::Seconds(30),
    const base::Location& location = FROM_HERE);

// Returns a waiter for a single for a given `event` whose arguments match
// given `matchers`.
//
// Unlike TestAutofillManagerWaiter, this waiter does not block on pending
// OnAfterFoo() events. To wait for On{Before,After}Foo() events, strongly
// consider using TestAutofillManagerWaiter instead.
//
// The `event` must be a pointer to an AutofillManager::Observer member
// function. When adding a new event to AutofillManager::Observer, a
// corresponding override must be added to the nested Impl class.
//
// Typical usage is as follows:
//
//   TEST_F(MyFixture, MyTest) {
//     ...
//     TestAutofillManagerSingleEventWaiter waiter(
//        *autofill_manager,
//        &AutofillManager::Observer::OnFillOrPreviewDataModelForm,
//        _, mojom::ActionPersistence::kPreview, _, _);
//     ...
//     EXPECT_TRUE(std::move(waiter).Wait());
//   }
class TestAutofillManagerSingleEventWaiter {
 public:
  // Creates a waiter for `event` without matchers.
  template <typename R, typename... Args>
  explicit TestAutofillManagerSingleEventWaiter(
      AutofillManager& manager,
      R (AutofillManager::Observer::*event)(AutofillManager&, Args...))
      : TestAutofillManagerSingleEventWaiter(manager,
                                             event,
                                             anything_matcher<Args>...) {}

  // Creates a waiter for `event` whose arguments (with the first
  // `AutofillManager&` parameter skipped) match `matchers`.
  template <typename R, typename... Args, typename... Matchers>
  explicit TestAutofillManagerSingleEventWaiter(
      AutofillManager& manager,
      R (AutofillManager::Observer::*event)(AutofillManager&, Args...),
      Matchers... matchers)
      : wait_(base::BindOnce(
            [](std::unique_ptr<Impl<Args...>> observer,
               base::TimeDelta timeout,
               const base::Location& location) {
              return std::move(*observer).Wait(timeout, std::move(location));
            },
            std::make_unique<Impl<Args...>>(
                manager,
                event,
                std::forward_as_tuple(std::forward<Matchers>(matchers)...)))) {}

  TestAutofillManagerSingleEventWaiter(TestAutofillManagerSingleEventWaiter&&);
  TestAutofillManagerSingleEventWaiter& operator=(
      TestAutofillManagerSingleEventWaiter&&);

  ~TestAutofillManagerSingleEventWaiter();

  // Blocks until the first `event` is fired since the waiters construction.
  //
  // Since the asynchronous-parsing task runner in AutofillManager has
  // relatively low priority, a high timeout may be necessary on slow bots.
  testing::AssertionResult Wait(base::TimeDelta timeout = base::Seconds(30),
                                const base::Location& location = FROM_HERE) && {
    return std::move(wait_).Run(timeout, location);
  }

 private:
  // Observes a single event and quits a base::RunLoop on the first event that
  // matches a given predicate.
  //
  // For every AutofillManager::Observer::OnFoo(Args... args) event, this class
  // should have an override of the form
  //
  //   void OnFoo(Args... args) {
  //     MaybeQuit(&Observer::OnFoo, args...);
  //   }
  template <typename... Args>
  class Impl : public AutofillManager::Observer {
   public:
    using Event = void (Observer::*)(AutofillManager&, Args...);

    explicit Impl(AutofillManager& manager,
                  Event event,
                  std::tuple<testing::Matcher<Args>...> matchers)
        : event_(std::forward<Event>(event)), matchers_(std::move(matchers)) {
      observation_.Observe(&manager);
    }

    ~Impl() override = default;

    testing::AssertionResult Wait(base::TimeDelta timeout,
                                  const base::Location& location) && {
      static const char kTimeoutMessage[] =
          "Timeout of callback from TestAutofillManagerSingleEventWaiter() in ";
      bool timed_out = false;
      base::test::ScopedRunLoopTimeout run_loop_timeout(
          FROM_HERE, timeout,
          base::BindRepeating(
              [](bool* timed_out, const base::Location& location) {
                *timed_out = true;
                return std::string(kTimeoutMessage) + location.ToString();
              },
              base::Unretained(&timed_out), location));
      run_loop_.Run(location);
      return !timed_out ? testing::AssertionSuccess()
                        : testing::AssertionFailure()
                              << kTimeoutMessage << location.ToString();
    }

   private:
    // AutofillManager::Observer:
    void OnAutofillManagerStateChanged(
        AutofillManager& manager,
        AutofillManager::LifecycleState old_state,
        AutofillManager::LifecycleState new_state) override {
      MaybeQuit(&Observer::OnAutofillManagerStateChanged, manager, old_state,
                new_state);
    }
    void OnBeforeLanguageDetermined(AutofillManager& manager) override {
      MaybeQuit(&Observer::OnBeforeLanguageDetermined, manager);
    }
    void OnAfterLanguageDetermined(AutofillManager& manager) override {
      MaybeQuit(&Observer::OnAfterLanguageDetermined, manager);
    }
    void OnBeforeFormsSeen(
        AutofillManager& manager,
        base::span<const FormGlobalId> updated_forms,
        base::span<const FormGlobalId> removed_forms) override {
      MaybeQuit(&Observer::OnBeforeFormsSeen, manager, updated_forms,
                removed_forms);
    }
    void OnAfterFormsSeen(
        AutofillManager& manager,
        base::span<const FormGlobalId> updated_forms,
        base::span<const FormGlobalId> removed_forms) override {
      MaybeQuit(&Observer::OnAfterFormsSeen, manager, updated_forms,
                removed_forms);
    }
    void OnBeforeCaretMovedInFormField(AutofillManager& manager,
                                       const FormGlobalId& form,
                                       const FieldGlobalId& field_id,
                                       const std::u16string& selection,
                                       const gfx::Rect& caret_bounds) override {
      MaybeQuit(&Observer::OnBeforeCaretMovedInFormField, manager, form,
                field_id, selection, caret_bounds);
    }
    void OnAfterCaretMovedInFormField(AutofillManager& manager,
                                      const FormGlobalId& form,
                                      const FieldGlobalId& field_id,
                                      const std::u16string& selection,
                                      const gfx::Rect& caret_bounds) override {
      MaybeQuit(&Observer::OnAfterCaretMovedInFormField, manager, form,
                field_id, selection, caret_bounds);
    }
    void OnBeforeTextFieldDidChange(AutofillManager& manager,
                                    FormGlobalId form,
                                    FieldGlobalId field) override {
      MaybeQuit(&Observer::OnBeforeTextFieldDidChange, manager, form, field);
    }
    void OnAfterTextFieldDidChange(AutofillManager& manager,
                                   FormGlobalId form,
                                   FieldGlobalId field,
                                   const std::u16string& text_value) override {
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
      MaybeQuit(&Observer::OnBeforeSelectControlDidChange, manager, form,
                field);
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
    void OnBeforeFocusOnFormField(AutofillManager& manager,
                                  FormGlobalId form,
                                  FieldGlobalId field) override {
      MaybeQuit(&Observer::OnBeforeFocusOnFormField, manager, form, field);
    }
    void OnAfterFocusOnFormField(AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field) override {
      MaybeQuit(&Observer::OnAfterFocusOnFormField, manager, form, field);
    }
    void OnBeforeDidFillAutofillFormData(AutofillManager& manager,
                                         FormGlobalId form) override {
      MaybeQuit(&Observer::OnBeforeDidFillAutofillFormData, manager, form);
    }
    void OnAfterDidFillAutofillFormData(AutofillManager& manager,
                                        FormGlobalId form) override {
      MaybeQuit(&Observer::OnAfterDidFillAutofillFormData, manager, form);
    }
    void OnBeforeJavaScriptChangedAutofilledValue(
        AutofillManager& manager,
        FormGlobalId form,
        FieldGlobalId field) override {
      MaybeQuit(&Observer::OnBeforeJavaScriptChangedAutofilledValue, manager,
                form, field);
    }
    void OnAfterJavaScriptChangedAutofilledValue(AutofillManager& manager,
                                                 FormGlobalId form,
                                                 FieldGlobalId field) override {
      MaybeQuit(&Observer::OnAfterJavaScriptChangedAutofilledValue, manager,
                form, field);
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
    void OnFormSubmitted(AutofillManager& manager,
                         const FormData& form) override {
      MaybeQuit(&Observer::OnFormSubmitted, manager, form);
    }
    void OnBeforeLoadedServerPredictions(AutofillManager& manager) override {
      MaybeQuit(&Observer::OnBeforeLoadedServerPredictions, manager);
    }
    void OnAfterLoadedServerPredictions(AutofillManager& manager) override {
      MaybeQuit(&Observer::OnAfterLoadedServerPredictions, manager);
    }

    // Quits the `run_loop_` if `event` matches `event_`.
    //
    // `event` must be a pointer to an AutofillManager::Observer member
    // function.
    template <typename CandidateEvent, typename... CandidateArgs>
    void MaybeQuit(const CandidateEvent& event,
                   AutofillManager&,
                   CandidateArgs&&... args) {
      if constexpr (std::is_same_v<Event, CandidateEvent>) {
        if (event_ == event &&
            Matches<0>(std::forward<CandidateArgs>(args)...)) {
          run_loop_.Quit();
        }
      }
    }

    template <size_t Index>
    bool Matches() const {
      return true;
    }

    template <size_t Index, typename CandidateArg, typename... CandidateArgs>
    bool Matches(CandidateArg&& arg, CandidateArgs&&... args) {
      return std::get<Index>(matchers_).Matches(arg) &&
             Matches<Index + 1>(std::forward<CandidateArgs>(args)...);
    }

    base::ScopedObservation<AutofillManager, AutofillManager::Observer>
        observation_{this};
    Event event_ = nullptr;
    std::tuple<testing::Matcher<Args>...> matchers_;
    base::RunLoop run_loop_;
  };

  // Helper to create `sizeof...(Args)` many `_` matchers.
  template <typename>
  constexpr static auto anything_matcher = testing::_;

  base::OnceCallback<testing::AssertionResult(base::TimeDelta,
                                              const base::Location&)>
      wait_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_WAITER_H_
