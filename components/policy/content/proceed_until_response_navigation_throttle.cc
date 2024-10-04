// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/proceed_until_response_navigation_throttle.h"

#include "base/notimplemented.h"
#include "base/strings/strcat.h"

ProceedUntilResponseNavigationThrottle::Client::Client(
    content::NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

ProceedUntilResponseNavigationThrottle::ProceedUntilResponseNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<Client> client,
    std::optional<DeferredResultCallback> deferred_result_callback)
    : NavigationThrottle(navigation_handle),
      name_(base::StrCat({"ProceedUntilResponseNavigationThrottle::",
                          client ? client->GetNameForLogging() : ""})),
      client_(std::move(client)),
      deferred_result_callback_(std::move(deferred_result_callback)) {
  CHECK(client_);
  client_->SetDeferredResultCallback(base::BindRepeating(
      &ProceedUntilResponseNavigationThrottle::ResolveDeferredResult,
      base::Unretained(this)));
}

ProceedUntilResponseNavigationThrottle::
    ~ProceedUntilResponseNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ProceedUntilResponseNavigationThrottle::WillStartRequest() {
  CHECK_EQ(deferred_state_, DeferredState::kNotDeferred);
  CHECK(!result_.has_value());
  return ProcessEvent(Event::kWillStartRequest);
}

content::NavigationThrottle::ThrottleCheckResult
ProceedUntilResponseNavigationThrottle::WillRedirectRequest() {
  return ProcessEvent(Event::kWillRedirectRequest);
}

content::NavigationThrottle::ThrottleCheckResult
ProceedUntilResponseNavigationThrottle::WillFailRequest() {
  NOTIMPLEMENTED() << "not verified in any test";
  return ProcessEvent(Event::kWillFailRequest);
}

content::NavigationThrottle::ThrottleCheckResult
ProceedUntilResponseNavigationThrottle::WillProcessResponse() {
  return ProcessEvent(Event::kWillProcessResponse);
}

content::NavigationThrottle::ThrottleCheckResult
ProceedUntilResponseNavigationThrottle::WillCommitWithoutUrlLoader() {
  NOTIMPLEMENTED() << "not verified in any test";
  return ProcessEvent(Event::kWillCommitWithoutUrlLoader);
}

const char* ProceedUntilResponseNavigationThrottle::GetNameForLogging() {
  return name_.c_str();
}

content::NavigationThrottle::ThrottleCheckResult
ProceedUntilResponseNavigationThrottle::ProcessEvent(Event event) {
  switch (deferred_state_) {
    case DeferredState::kNotDeferred: {
      // The internal throttle isn't currently running an asynchronous task.
      // If the previous asynchronous task returned non-PROCEED action, return
      // it.
      if (result_.has_value() && result_->action() != PROCEED) {
        return std::move(result_).value();
      }
      // Otherwise, forward the event to the internal throttle.
      content::NavigationThrottle::ThrottleCheckResult result =
          CallInternalThrottle(event);
      if (result.action() == DEFER) {
        if (!CanProceedSpeculatively(event)) {
          // Need to be kDeferredExposed state, but ResolveDeferredResult()
          // need a dedicated implementation for this case.
          NOTIMPLEMENTED();
        }
        // Hides the DEFER action and proceeds.
        deferred_state_ = DeferredState::kDeferredNotExposed;
        pending_event_ = Event::kNoEvent;
        return PROCEED;
      }
      return result;
    }
    case DeferredState::kDeferredNotExposed:
      // The internal throttle is still running an asynchronous task. Change the
      // deferred_state_ to kDeferredExposed to revisit the event when the
      // asynchronous task completes.
      deferred_state_ = DeferredState::kDeferredExposed;
      pending_event_ = event;
      // Returns DEFER to avoid more events happening.
      return DEFER;
    case DeferredState::kDeferredExposed:
      // While we expose DEFER, any event should not be delivered.
      NOTREACHED_NORETURN();
  }
}

bool ProceedUntilResponseNavigationThrottle::CanProceedSpeculatively(
    Event event) {
  switch (event) {
    case Event::kNoEvent:
      NOTREACHED_NORETURN();
    case Event::kWillStartRequest:
    case Event::kWillRedirectRequest:
      return true;
    case Event::kWillFailRequest:
    case Event::kWillProcessResponse:
    case Event::kWillCommitWithoutUrlLoader:
      return false;
  }
  NOTREACHED_NORETURN();
}

content::NavigationThrottle::ThrottleCheckResult
ProceedUntilResponseNavigationThrottle::CallInternalThrottle(Event event) {
  switch (event) {
    case Event::kNoEvent:
      NOTREACHED_NORETURN();
    case Event::kWillStartRequest:
      return client_->WillStartRequest();
    case Event::kWillRedirectRequest:
      return client_->WillRedirectRequest();
    case Event::kWillFailRequest:
      return client_->WillFailRequest();
    case Event::kWillProcessResponse:
      return client_->WillProcessResponse();
    case Event::kWillCommitWithoutUrlLoader:
      return client_->WillCommitWithoutUrlLoader();
  }
  NOTREACHED_NORETURN();
}

void ProceedUntilResponseNavigationThrottle::ResolveDeferredResult(
    bool proceed,
    std::optional<ThrottleCheckResult> result) {
  // Ensure that `proceed` and `result` are consistent with each other.
  CHECK(proceed || (result.has_value() && result->action() != PROCEED));
  switch (deferred_state_) {
    case DeferredState::kNotDeferred:
      NOTREACHED_NORETURN();
    case DeferredState::kDeferredNotExposed:
      // An asynchronous task finished before the next navigation event arrives.
      // Remember the result only if the `proceed` is false, to return it when
      // the next event arrives. If the `proceed` is true, `result` may contain
      // a junk.
      if (!proceed) {
        result_ = std::move(result);
      }
      deferred_state_ = DeferredState::kNotDeferred;
      break;
    case DeferredState::kDeferredExposed:
      // An asynchronous task triggered finished after the next navigation event
      // arrives. The throttle runner is suspended.
      deferred_state_ = DeferredState::kNotDeferred;
      if (proceed) {
        // If the asynchronous task allows to proceed, re-process the next
        // navigation event now after previously immediately deferring it,
        // and resume the runner with the throttle's result.
        // As `ProcessEvent()` can hide the internal DEFER always, there is no
        // case we need to expose DEFER below.
        result = ProcessEvent(pending_event_);
        CHECK_NE(DEFER, result->action());
        proceed = result->action() == PROCEED;
      }
      if (proceed) {
        if (deferred_result_callback_) {
          deferred_result_callback_->Run(proceed, std::nullopt);
        } else {
          Resume();
        }
      } else {
        if (deferred_result_callback_) {
          deferred_result_callback_->Run(proceed, result);
        } else {
          CancelDeferredNavigation(*result);
        }
      }
      break;
  }
}
