// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_UIA_ELEMENT_LOCATOR_WIN_H_
#define CHROME_BROWSER_WIN_UIA_ELEMENT_LOCATOR_WIN_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/services/util_win/public/cpp/uia_element_probe_types.h"
#include "chrome/services/util_win/public/mojom/uia_element_probe.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// Locates an element in another app's window through the UiaElementProbe
// utility service: owns the service process, bounds each request with a
// deadline, and drops a request a newer one supersedes. Single sequence.
class UiaElementLocatorWin {
 public:
  // Deadline for the service's answer on top of Query::wait_for.
  static constexpr base::TimeDelta kLocateTimeout = base::Seconds(2);
  // Matches the other util_win service clients.
  static constexpr base::TimeDelta kIdleTimeout = base::Seconds(5);

  // Why a request ended without a measurement.
  enum class FailureReason {
    // The service found no element matching the query.
    kNotFound,
    // The service found one, but its bounds do not sit inside the window.
    kUnusable,
    // The service did not answer within the deadline.
    kTimedOut,
    // The service could not be started, or went away mid-request.
    kDisconnected,
    // A newer Locate() call replaced this one.
    kSuperseded,
    // The window was gone, or no longer the target process's, before asking.
    kWindowGone,
  };

  // A window as the caller found it; the service re-checks all three. Callers
  // pass a Duplicate() of their process handle, which needs at least
  // PROCESS_QUERY_LIMITED_INFORMATION and is handed to the service.
  using Target = UiaWindowIdentity;

  // What to find. The request's deadline is `wait_for` plus kLocateTimeout.
  using Query = UiaElementQuery;

  // Physical screen pixels.
  using Measurement = UiaElementMeasurement;

  using Result = base::expected<Measurement, FailureReason>;
  using LocateCallback = base::OnceCallback<void(Result)>;

  UiaElementLocatorWin();

  UiaElementLocatorWin(const UiaElementLocatorWin&) = delete;
  UiaElementLocatorWin& operator=(const UiaElementLocatorWin&) = delete;

  virtual ~UiaElementLocatorWin();

  // Locates the element `query` names in `target`'s window and runs `callback`
  // exactly once, always asynchronously, with the measurement or the reason
  // there is none. Answers kWindowGone without asking the service if the
  // window is gone or is no longer owned by `target.process`. Either way, a
  // request still in flight is answered with kSuperseded and its reply
  // dropped. Destruction abandons a request still in flight; an answer already
  // posted is still delivered.
  virtual void Locate(Target target,
                      const Query& query,
                      LocateCallback callback);

  // Runs `callback` once, the next time the service pipe is seen to drop.
  void SetOnDisconnectedForTesting(base::OnceClosure callback);

 protected:
  // Returns a Remote to a newly launched UiaElementProbe service. Virtual for
  // testing.
  virtual mojo::Remote<chrome::mojom::UiaElementProbe> LaunchService();

 private:
  void Send(Target target, const Query& query)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Handles a reply from the service. Timeouts and disconnects are reported
  // directly by OnTimeout() and OnDisconnected() instead.
  void OnFound(const std::optional<Measurement>& measurement)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnTimeout() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnDisconnected() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Answers the request in flight, which there must be one of.
  void Finish(Result result) VALID_CONTEXT_REQUIRED(sequence_checker_);

  mojo::Remote<chrome::mojom::UiaElementProbe> probe_
      GUARDED_BY_CONTEXT(sequence_checker_);

  LocateCallback in_flight_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer timeout_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure on_disconnected_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  // Binds OnFound(). Invalidated when the request in flight is superseded, so
  // the service's reply to it is dropped rather than taken for the new one's.
  base::WeakPtrFactory<UiaElementLocatorWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WIN_UIA_ELEMENT_LOCATOR_WIN_H_
