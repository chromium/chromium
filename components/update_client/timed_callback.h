// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TIMED_CALLBACK_H_
#define COMPONENTS_UPDATE_CLIENT_TIMED_CALLBACK_H_

#include <memory>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace update_client {

// MakeTimedCallback posts a task to run `callback` after `timeout` with
// `timeout_args` on the current sequence, and returns a new callback to the
// caller that also calls `callback`. The returned callback must only be run on
// the current sequence.
//
// If the timeout occurs before the returned callback is
// run, the returned callback does nothing. It is safe to discard the returned
// callback in this case, or to call it.
//
// If the returned callback is run before the timeout, the timeout will do
// nothing.
//
// In either case, `callback` is guaranteed to run exactly once.
//
// For example, given:
//   void HandleDecryptedContents(std::optional<std::string> plaintext);
//   decryptAsync(ciphertext, base::BindOnce(&HandleDecryptedContents));
//
// It's possible to add a 30-second timeout:
//   void HandleDecryptedContents(std::optional<std::string> plaintext);
//   decryptAsync(
//       cipher,
//       MakeTimedCallback(
//           base::BindOnce(&HandleDecryptedContents),
//           base::Seconds(30),
//           std::nullopt));
//
template <typename ReturnType, typename... CallbackArgs, typename... Args>
base::OnceCallback<ReturnType(CallbackArgs...)> MakeTimedCallback(
    base::OnceCallback<ReturnType(CallbackArgs...)> callback,
    base::TimeDelta timeout,
    Args&&... timeout_args) {
  auto wrapper = std::make_unique<
      base::CancelableOnceCallback<ReturnType(CallbackArgs...)>>(
      std::move(callback));
  // Both callbacks must be constructed before either has a chance to run.
  // `callback_1` will be run with the `timeout_args` after `timeout`.
  // When either callback is run, they invalidate CancelableCallback's internal
  // weak pointers, and the other callback becomes a no-op.
  base::OnceCallback<ReturnType()> callback_1 =
      base::BindOnce(wrapper->callback(), std::forward<Args>(timeout_args)...);
  base::OnceCallback<ReturnType(CallbackArgs...)> callback_2 =
      wrapper->callback();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<
                 base::CancelableOnceCallback<ReturnType(CallbackArgs...)>>
             /* wrapper */,  // This callback takes ownership of `wrapper`.
             base::OnceCallback<ReturnType()> callback_1) {
            std::move(callback_1).Run();
          },
          std::move(wrapper), std::move(callback_1)),
      timeout);
  return callback_2;
}

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TIMED_CALLBACK_H_
