// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_PROCEED_UNTIL_RESPONSE_NAVIGATION_THROTTLE_H_
#define COMPONENTS_POLICY_CONTENT_PROCEED_UNTIL_RESPONSE_NAVIGATION_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// This class can be used to wrap internal throttles that trigger asynchronous
// tasks on WillStartRequest or WillRedirectRequest and need to make a final
// decision based on the result of the asynchronous tasks, but are actually OK
// for the navigation to proceed up until WillProcessResponse.
// Note that this class ensures that the internal throttle won't run more than 1
// async task at a time.
class ProceedUntilResponseNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Called when the processing result is available after being deferred.
  // if !`proceed`, `result` contains the result to pass to
  // CancelDeferredNavigation(). Otherwise, the
  // ProceedUntilResponseNavigationThrottle will call Resume().
  using DeferredResultCallback =
      base::RepeatingCallback<void(bool proceed,
                                   std::optional<ThrottleCheckResult> result)>;
  // The internal throttle needs to inherit the Client class to support a
  // callback that notifies the ProceedUntilResponseNavigationThrottle after the
  // asynchronous tasks are done.
  class Client : public content::NavigationThrottle {
   public:
    explicit Client(content::NavigationHandle* navigation_handle);
    virtual void SetDeferredResultCallback(
        const DeferredResultCallback& deferred_result_callback) = 0;
  };
  ProceedUntilResponseNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<Client> client,
      std::optional<DeferredResultCallback> deferred_result_callback);
  ProceedUntilResponseNavigationThrottle(
      const ProceedUntilResponseNavigationThrottle&) = delete;
  ProceedUntilResponseNavigationThrottle& operator=(
      const ProceedUntilResponseNavigationThrottle&) = delete;
  ~ProceedUntilResponseNavigationThrottle() override;

 private:
  // This is practically a copy of content::NavigationThrottleRunner::Event that
  // is not published in the //content/public.
  enum class Event {
    kNoEvent,
    kWillStartRequest,
    kWillRedirectRequest,
    kWillFailRequest,
    kWillProcessResponse,
    kWillCommitWithoutUrlLoader,
  };
  // Internal state to express if an internal throttle's doesn't request a DEFER
  // action, or does DEFER and the action is exposed to the external runner or
  // hidden inside this throttle.
  enum class DeferredState {
    // The internal throttle doesn't request a DEFER.
    kNotDeferred,
    // The internal throttle requests a DEFER, but hidden by this throttle.
    kDeferredNotExposed,
    // The internal throttle requested a DEFER, and this throttle exposes a
    // DEFER to the external runner as another event is delivered while the
    // internal throttle continues running an internal asynchronous task behind
    // the hidden DEFER.
    kDeferredExposed,
  };

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;
  const char* GetNameForLogging() override;

  bool CanProceedSpeculatively(Event event);
  ThrottleCheckResult ProcessEvent(Event event);
  ThrottleCheckResult CallInternalThrottle(Event event);
  void ResolveDeferredResult(bool proceed,
                             std::optional<ThrottleCheckResult> result);

  DeferredState deferred_state_ = DeferredState::kNotDeferred;
  Event pending_event_ = Event::kNoEvent;

  // Holds the ThrottleCheckResult that the internal throttle returns after the
  // asynchronous task that it runs finished. The hold value will be used in the
  // next event to return the action.
  std::optional<ThrottleCheckResult> result_;

  const std::string name_;
  const std::unique_ptr<Client> client_;

  // Holds the callback that the owner passed in the constructor in order to
  // override the continuation handling, Resume and CancelDeferredNavigation.
  // With this mechanism, this ProceedUntilResponseNavigationThrottle itself
  // can be an inner throttle of another throttle.
  const std::optional<DeferredResultCallback> deferred_result_callback_;
};

#endif  // COMPONENTS_POLICY_CONTENT_PROCEED_UNTIL_RESPONSE_NAVIGATION_THROTTLE_H_
