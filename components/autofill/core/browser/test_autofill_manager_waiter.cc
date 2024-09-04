// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager_waiter.h"

#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_run_loop_timeout.h"

namespace autofill {

TestAutofillManagerWaiter::State::State() = default;
TestAutofillManagerWaiter::State::~State() = default;

TestAutofillManagerWaiter::EventCount* TestAutofillManagerWaiter::State::Get(
    Event event) {
  auto it = events.find(event);
  return it != events.end() ? &it->second : nullptr;
}

TestAutofillManagerWaiter::EventCount&
TestAutofillManagerWaiter::State::GetOrCreate(Event event,
                                              const base::Location& location) {
  if (EventCount* e = Get(event)) {
    return *e;
  }
  EventCount& e = events[event];
  e = EventCount{.location = location};
  return e;
}

TestAutofillManagerWaiter::TestAutofillManagerWaiter(
    AutofillManager& manager,
    DenseSet<Event> relevant_events,
    base::Location location)
    : relevant_events_(relevant_events), waiter_location_(std::move(location)) {
  observation_.Observe(&manager);
}

TestAutofillManagerWaiter::~TestAutofillManagerWaiter() = default;

std::string TestAutofillManagerWaiter::DescribeState() const {
  std::vector<std::string> events_vector;
  events_vector.reserve(state_->events.size());
  for (const auto& [_, event_count] : state_->events) {
    events_vector.push_back(base::StringPrintf(
        "%s{num_before=%zu, num_after=%zu}",
        event_count.location.function_name(), event_count.num_before_events,
        event_count.num_after_events));
  }
  return base::StringPrintf(
      "TestAutofillManagerWaiter created at %s for %zu relevant events has "
      "seen events [%s] is %stimed out",
      waiter_location_.ToString().c_str(), state_->num_expected_relevant_events,
      base::JoinString(events_vector, ", ").c_str(),
      state_->timed_out ? "" : "not ");
}

size_t TestAutofillManagerWaiter::num_pending_events() const {
  size_t events = 0;
  for (const auto& [event, event_count] : state_->events) {
    CHECK_GE(event_count.num_before_events, event_count.num_after_events)
        << DescribeState();
    events += event_count.num_before_events - event_count.num_after_events;
  }
  return events;
}

size_t TestAutofillManagerWaiter::num_completed_relevant_events() const {
  size_t events = 0;
  for (const auto& [event, event_count] : state_->events) {
    if (IsRelevant(event)) {
      CHECK_GE(event_count.num_before_events, event_count.num_after_events)
          << DescribeState();
      events += event_count.num_after_events;
    }
  }
  return events;
}

void TestAutofillManagerWaiter::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState old_state,
    AutofillManager::LifecycleState new_state) {
  switch (new_state) {
    case AutofillManager::LifecycleState::kInactive:
    case AutofillManager::LifecycleState::kActive:
      break;
    case AutofillManager::LifecycleState::kPendingReset: {
      std::unique_ptr<State> keep_state_alive;
      base::AutoLock lock(state_->lock);
      // Reset the state so Wait() can be called again. Defer the destruction
      // until after `lock` is released.
      keep_state_alive = std::exchange(state_, std::make_unique<State>());
      break;
    }
    case AutofillManager::LifecycleState::kPendingDeletion:
      observation_.Reset();
      break;
  }
}

void TestAutofillManagerWaiter::OnBeforeLanguageDetermined(
    AutofillManager& manager) {
  OnBefore(Event::kLanguageDetermined);
}

void TestAutofillManagerWaiter::OnAfterLanguageDetermined(
    AutofillManager& manager) {
  OnAfter(Event::kLanguageDetermined);
}

void TestAutofillManagerWaiter::OnBeforeFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  OnBefore(Event::kFormsSeen);
}

void TestAutofillManagerWaiter::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  OnAfter(Event::kFormsSeen);
}

void TestAutofillManagerWaiter::OnBeforeCaretMovedInFormField(
    AutofillManager& manager,
    const FormGlobalId& form,
    const FieldGlobalId& field_id,
    const std::u16string& selection,
    const gfx::Rect& caret_bounds) {
  OnBefore(Event::kCaretMovedInFormField);
}

void TestAutofillManagerWaiter::OnAfterCaretMovedInFormField(
    AutofillManager& manager,
    const FormGlobalId& form,
    const FieldGlobalId& field_id,
    const std::u16string& selection,
    const gfx::Rect& caret_bounds) {
  OnAfter(Event::kCaretMovedInFormField);
}

void TestAutofillManagerWaiter::OnBeforeTextFieldDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnBefore(Event::kTextFieldDidChange);
}

void TestAutofillManagerWaiter::OnAfterTextFieldDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field,
    const std::u16string& text_value) {
  OnAfter(Event::kTextFieldDidChange);
}

void TestAutofillManagerWaiter::OnBeforeTextFieldDidScroll(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnBefore(Event::kTextFieldDidScroll);
}

void TestAutofillManagerWaiter::OnAfterTextFieldDidScroll(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnAfter(Event::kTextFieldDidScroll);
}

void TestAutofillManagerWaiter::OnBeforeSelectControlDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnBefore(Event::kSelectControlDidChange);
}

void TestAutofillManagerWaiter::OnAfterSelectControlDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnAfter(Event::kSelectControlDidChange);
}

void TestAutofillManagerWaiter::OnBeforeAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field,
    const FormData& form_data) {
  OnBefore(Event::kAskForValuesToFill);
}

void TestAutofillManagerWaiter::OnAfterAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnAfter(Event::kAskForValuesToFill);
}

void TestAutofillManagerWaiter::OnBeforeFocusOnFormField(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnBefore(Event::kFocusOnFormField);
}

void TestAutofillManagerWaiter::OnAfterFocusOnFormField(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnAfter(Event::kFocusOnFormField);
}

void TestAutofillManagerWaiter::OnBeforeDidFillAutofillFormData(
    AutofillManager& manager,
    FormGlobalId form) {
  OnBefore(Event::kDidFillAutofillFormData);
}

void TestAutofillManagerWaiter::OnAfterDidFillAutofillFormData(
    AutofillManager& manager,
    FormGlobalId form) {
  OnAfter(Event::kDidFillAutofillFormData);
}

void TestAutofillManagerWaiter::OnBeforeJavaScriptChangedAutofilledValue(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnBefore(Event::kJavaScriptChangedAutofilledValue);
}

void TestAutofillManagerWaiter::OnAfterJavaScriptChangedAutofilledValue(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  OnAfter(Event::kJavaScriptChangedAutofilledValue);
}

void TestAutofillManagerWaiter::OnFormSubmitted(AutofillManager& manager,
                                                const FormData& form) {
  OnBefore(Event::kFormSubmitted);
  OnAfter(Event::kFormSubmitted);
}

void TestAutofillManagerWaiter::OnBeforeLoadedServerPredictions(
    AutofillManager& manager) {
  OnBefore(Event::kLoadedServerPredictions);
}

void TestAutofillManagerWaiter::OnAfterLoadedServerPredictions(
    AutofillManager& manager) {
  OnAfter(Event::kLoadedServerPredictions);
}

bool TestAutofillManagerWaiter::IsRelevant(Event event) const {
  return relevant_events_.empty() || relevant_events_.contains(event);
}

void TestAutofillManagerWaiter::OnBefore(Event event,
                                         const base::Location& location) {
  base::AutoLock lock(state_->lock);
  if (state_->run_loop.AnyQuitCalled()) {
    VLOG(1) << "Ignoring event because there are no pending expected events: "
            << __func__ << "(" << location.function_name() << ")";
    return;
  }
  VLOG(1) << __func__ << "(" << location.function_name() << ")";
  EventCount& e = state_->GetOrCreate(event, location);
  e.location = location;
  ++e.num_before_events;
}

void TestAutofillManagerWaiter::OnAfter(Event event,
                                        const base::Location& location) {
  base::AutoLock lock(state_->lock);
  if (state_->run_loop.AnyQuitCalled()) {
    VLOG(1) << "Ignoring event because there are no pending expected events: "
            << __func__ << "(" << location.function_name() << ")";
    return;
  }
  VLOG(1) << __func__ << "(" << location.function_name() << ")";
  EventCount* e = state_->Get(event);
  ASSERT_TRUE(e) << __func__ << "(" << location.function_name()
                 << "): " << DescribeState();
  ++e->num_after_events;
  ASSERT_GE(e->num_before_events, e->num_after_events) << DescribeState();
  if (num_pending_events() == 0 &&
      num_completed_relevant_events() >= state_->num_expected_relevant_events) {
    state_->run_loop.Quit();
  }
}

testing::AssertionResult TestAutofillManagerWaiter::Wait(
    size_t num_expected_relevant_events,
    base::TimeDelta timeout,
    const base::Location& location) {
  // If we want to reset `state_`, it must be destroyed after `lock`.
  std::unique_ptr<State> keep_state_alive;
  base::AutoLock lock(state_->lock);

  if (state_->run_loop.AnyQuitCalled()) {
    return testing::AssertionFailure()
           << "Waiter has not been Reset() since last Wait().";
  }

  // Wait for pending and remaining expected events.
  CHECK(!state_->timed_out);
  while (!state_->timed_out &&
         (num_pending_events() > 0 ||
          num_completed_relevant_events() < num_expected_relevant_events)) {
    base::test::ScopedRunLoopTimeout run_loop_timeout(
        location, timeout,
        base::BindRepeating(
            [](TestAutofillManagerWaiter& waiter) {
              waiter.state_->timed_out = true;
              return waiter.DescribeState();
            },
            std::ref(*this)));
    state_->num_expected_relevant_events = num_expected_relevant_events;
    base::AutoUnlock unlock(state_->lock);
    state_->run_loop.Run();
  }
  CHECK(state_->timed_out || num_pending_events() == 0u) << DescribeState();

  // Reset the state so Wait() can be called again. Defer the destruction until
  // after `lock` is released.
  keep_state_alive = std::exchange(state_, std::make_unique<State>());
  return !state_->timed_out ? testing::AssertionSuccess()
                            : testing::AssertionFailure() << "Waiter timed out";
}

const FormStructure* WaitForMatchingForm(
    AutofillManager* manager,
    base::RepeatingCallback<bool(const FormStructure&)> pred,
    base::TimeDelta timeout,
    const base::Location& location) {
  class Waiter : public AutofillManager::Observer {
   public:
    explicit Waiter(AutofillManager* manager,
                    base::RepeatingCallback<bool(const FormStructure&)> pred)
        : manager_(manager), pred_(std::move(pred)) {
      observation_.Observe(manager);
    }

    const FormStructure* Wait(base::TimeDelta timeout,
                              const base::Location& location) {
      DCHECK(observation_.IsObserving());
      DCHECK(!matching_form_);
      matching_form_ = FindForm();
      if (!matching_form_) {
        base::test::ScopedRunLoopTimeout run_loop_timeout(
            location, timeout,
            base::BindRepeating(
                [](const Waiter* self) {
                  return std::string("Didn't see a matching form ") +
                         (self->observation_.IsObserving()
                              ? "within the timeout"
                              : "before the AutofillManager was reset or "
                                "destroyed");
                },
                base::Unretained(this)));
        run_loop_.Run();
      }
      return std::exchange(matching_form_, nullptr);
    }

   private:
    void OnAutofillManagerStateChanged(
        AutofillManager& manager,
        AutofillManager::LifecycleState old_state,
        AutofillManager::LifecycleState new_state) override {
      using enum AutofillManager::LifecycleState;
      switch (new_state) {
        case kInactive:
        case kActive:
          break;
        case kPendingReset:
        case kPendingDeletion:
          DCHECK_EQ(&manager, manager_.get());
          manager_ = nullptr;
          run_loop_.Quit();
          observation_.Reset();
      }
    }

    void OnAfterFormsSeen(
        AutofillManager& manager,
        base::span<const FormGlobalId> updated_forms,
        base::span<const FormGlobalId> removed_forms) override {
      DCHECK_EQ(&manager, manager_.get());
      if (const auto* form = FindForm()) {
        matching_form_ = form;
        run_loop_.Quit();
      }
    }

    FormStructure* FindForm() const {
      auto it = base::ranges::find_if(
          manager_->form_structures(),
          [&](const auto& p) { return pred_.Run(*p.second); });
      return it != manager_->form_structures().end() ? it->second.get()
                                                     : nullptr;
    }

    base::ScopedObservation<AutofillManager, AutofillManager::Observer>
        observation_{this};
    raw_ptr<AutofillManager> manager_;
    base::RepeatingCallback<bool(const FormStructure&)> pred_;
    base::RunLoop run_loop_;
    raw_ptr<const FormStructure> matching_form_ = nullptr;
  };
  return Waiter(manager, std::move(pred)).Wait(timeout, location);
}

TestAutofillManagerSingleEventWaiter::TestAutofillManagerSingleEventWaiter(
    TestAutofillManagerSingleEventWaiter&&) = default;
TestAutofillManagerSingleEventWaiter&
TestAutofillManagerSingleEventWaiter::operator=(
    TestAutofillManagerSingleEventWaiter&&) = default;
TestAutofillManagerSingleEventWaiter::~TestAutofillManagerSingleEventWaiter() =
    default;

}  // namespace autofill
