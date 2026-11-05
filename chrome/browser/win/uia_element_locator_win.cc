// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/uia_element_locator_win.h"

#include <windows.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/win/util_win_service.h"

namespace {

using FailureReason = UiaElementLocatorWin::FailureReason;

// Returns true if the element's center lies inside a non-empty window.
// Re-checked here because this process is the one that acts on it.
bool IsUsable(const UiaElementLocatorWin::Measurement& measurement) {
  const gfx::Rect& window = measurement.window_bounds;
  const gfx::Rect& element = measurement.element_bounds;
  return !window.IsEmpty() && !element.IsEmpty() &&
         window.Contains(element.CenterPoint());
}

// Answers `callback` from a task, never from within Locate(), so callers see
// every answer the same way and a re-entrant Locate() finds a settled state.
void PostFailure(UiaElementLocatorWin::LocateCallback callback,
                 FailureReason reason) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::unexpected(reason)));
}

}  // namespace

UiaElementLocatorWin::UiaElementLocatorWin() = default;

UiaElementLocatorWin::~UiaElementLocatorWin() = default;

void UiaElementLocatorWin::Locate(Target target,
                                  const Query& query,
                                  LocateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  // Taken over before anything is answered, so no reply to an earlier request
  // can arrive after this one has its answer.
  if (in_flight_callback_) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    timeout_timer_.Stop();
    PostFailure(std::move(in_flight_callback_), FailureReason::kSuperseded);
  }

  // Covers an exited process too: it owns no windows, and while `target`
  // holds its handle, its pid cannot be reused.
  DWORD owner_process_id = 0;
  if (!target.hwnd || !target.process.IsValid() ||
      !::GetWindowThreadProcessId(target.hwnd, &owner_process_id) ||
      owner_process_id != target.process.Pid()) {
    PostFailure(std::move(callback), FailureReason::kWindowGone);
    return;
  }

  in_flight_callback_ = std::move(callback);
  Send(std::move(target), query);
}

void UiaElementLocatorWin::Send(Target target, const Query& query) {
  if (!probe_) {
    probe_ = LaunchService();
    if (!probe_) {
      PostFailure(std::move(in_flight_callback_), FailureReason::kDisconnected);
      return;
    }
    probe_.reset_on_idle_timeout(kIdleTimeout);
    probe_.set_disconnect_handler(base::BindOnce(
        &UiaElementLocatorWin::OnDisconnected, base::Unretained(this)));
  }

  timeout_timer_.Start(
      FROM_HERE, kLocateTimeout + query.wait_for,
      base::BindOnce(&UiaElementLocatorWin::OnTimeout, base::Unretained(this)));

  // A plain callback: if the pipe is already dead, Mojo drops it without
  // running it, and the request is then answered by OnDisconnected().
  probe_->FindElement(std::move(target), query,
                      base::BindOnce(&UiaElementLocatorWin::OnFound,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void UiaElementLocatorWin::OnFound(
    const std::optional<Measurement>& measurement) {
  timeout_timer_.Stop();

  if (!measurement) {
    Finish(base::unexpected(FailureReason::kNotFound));
  } else if (!IsUsable(*measurement)) {
    Finish(base::unexpected(FailureReason::kUnusable));
  } else {
    Finish(*measurement);
  }
}

void UiaElementLocatorWin::OnTimeout() {
  // The service may be stuck in the target app; a fresh one is cheaper than
  // finding out. Dropping the pipe also drops the outstanding reply.
  probe_.reset();
  Finish(base::unexpected(FailureReason::kTimedOut));
}

void UiaElementLocatorWin::OnDisconnected() {
  timeout_timer_.Stop();
  probe_.reset();
  // The pipe can also drop while nothing is in flight.
  if (in_flight_callback_) {
    Finish(base::unexpected(FailureReason::kDisconnected));
  }
  if (on_disconnected_for_testing_) {
    std::move(on_disconnected_for_testing_).Run();
  }
}

void UiaElementLocatorWin::SetOnDisconnectedForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_disconnected_for_testing_ = std::move(callback);
}

void UiaElementLocatorWin::Finish(Result result) {
  std::move(in_flight_callback_).Run(std::move(result));
}

mojo::Remote<chrome::mojom::UiaElementProbe>
UiaElementLocatorWin::LaunchService() {
  return LaunchUiaElementProbeService();
}
