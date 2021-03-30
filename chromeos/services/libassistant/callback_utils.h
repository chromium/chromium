// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_CALLBACK_UTILS_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_CALLBACK_UTILS_H_

#include <functional>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {
namespace libassistant {

// Wrapper around a |base::OnceCallback| that converts it to a std::function.
template <typename... Args>
std::function<void(Args...)> ToStdFunction(
    base::OnceCallback<void(Args...)> once_callback) {
  // Note we need to wrap the move-only once callback in a repeating callback,
  // as std::function must always be copyable.
  return [repeating_callback = base::AdaptCallbackForRepeating(
              std::move(once_callback))](Args... args) {
    repeating_callback.Run(std::forward<Args>(args)...);
  };
}

// Binds a method call to the current sequence, meaning we ensure |callback|
// will always be called from the current sequence. If the call comes from a
// different sequence it will be posted to the correct one.
template <typename... Args>
base::OnceCallback<void(Args...)> BindToCurrentSequence(
    base::OnceCallback<void(Args...)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(Args...)> callback,
         scoped_refptr<base::SequencedTaskRunner> sequence_runner,
         Args&&... args) {
        // Invoke the callback on the original sequence.
        if (sequence_runner->RunsTasksInCurrentSequence()) {
          std::move(callback).Run(std::forward<Args>(args)...);
        } else {
          sequence_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), std::forward<Args>(args)...));
        }
      },
      std::move(callback), base::SequencedTaskRunnerHandle::Get());
}

// Binds a method call to the current sequence.
// This is a convenience version, that allows you to easily wrap a callback
// and change the arguments.
//
// This allows you to type
//
//      BindToCurrentSequence(
//         [](CallbackType callback, const ResultType& result) (
//             OtherType other_type = ConvertResultToOtherType(result);
//             std::move(callback).Run(other_type);
//         ),
//         std::move(callback)
//      );
//
// instead of
//
//      BindToCurrentSequence(
//          base::BindOnce(
//              [](CallbackType callback, const ResultType& result) (
//                  OtherType other_type = ConvertResultToOtherType(result);
//                  std::move(callback).Run(other_type);
//              ),
//              std::move(callback)
//          )
//      );
//
template <typename Functor, typename... Args>
auto BindToCurrentSequence(Functor&& functor, Args&&... args) {
  auto callback = base::BindOnce(std::forward<Functor>(functor),
                                 std::forward<Args>(args)...);
  return BindToCurrentSequence(std::move(callback));
}

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_CALLBACK_UTILS_H_
