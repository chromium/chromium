// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_CALLBACK_UTILS_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_CALLBACK_UTILS_H_

#include <functional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace ash::libassistant {

namespace internal {

template <typename CALLBACK_TYPE>
class RefCountedCallback
    : public base::RefCountedThreadSafe<RefCountedCallback<CALLBACK_TYPE>> {
 public:
  RefCountedCallback(CALLBACK_TYPE callback) : callback_(std::move(callback)) {}
  RefCountedCallback(const RefCountedCallback&) = delete;
  RefCountedCallback& operator=(const RefCountedCallback&) = delete;

  CALLBACK_TYPE& callback() { return callback_; }

 private:
  friend class base::RefCountedThreadSafe<RefCountedCallback<CALLBACK_TYPE>>;

  ~RefCountedCallback() = default;
  CALLBACK_TYPE callback_;
};

}  // namespace internal

// Wrapper around a |base::OnceCallback| that converts it to a std::function.
// Crashes if called more than once.
template <typename... Args>
std::function<void(Args...)> ToStdFunction(
    base::OnceCallback<void(Args...)> once_callback) {
  // Note we need to wrap the move-only once callback,
  // as std::function must always be copyable.
  using CallbackType = base::OnceCallback<void(Args...)>;
  using RefCountedCallbackType = internal::RefCountedCallback<CallbackType>;

  return [callback_ref = base::MakeRefCounted<RefCountedCallbackType>(
              std::move(once_callback))](Args... args) {
    std::move(callback_ref->callback()).Run(std::forward<Args>(args)...);
  };
}

// Wrapper around a |base::RepeatingCallback| that converts it to a
// std::function.
template <typename... Args>
std::function<void(Args...)> ToStdFunctionRepeating(
    base::RepeatingCallback<void(Args...)> repeating_callback) {
  return [callback = repeating_callback](Args... args) {
    callback.Run(std::forward<Args>(args)...);
  };
}

// Wraps a |base::OnceCallback| callback1<Args1> that changes its argument to
// Args2, where Args2 is transformed from Args1 applying the |transformer| rule.
template <typename... Args1, typename... Args2, typename Functor>
base::OnceCallback<void(Args1...)> AdaptCallback(
    base::OnceCallback<void(Args2...)> once_callback,
    Functor&& transformer) {
  return base::BindOnce(
      [](base::OnceCallback<void(Args2...)> callback, Functor&& transformer,
         Args1&&... args) {
        std::move(callback).Run(transformer(std::forward<Args1>(args)...));
      },
      std::move(once_callback), std::forward<Functor>(transformer));
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
      std::move(callback), base::SequencedTaskRunner::GetCurrentDefault());
}

// Binds a method call to the current sequence, meaning we ensure |callback|
// will always be called from the current sequence. If the call comes from a
// different sequence it will be posted to the correct one.
template <typename... Args>
base::RepeatingCallback<void(Args...)> BindToCurrentSequenceRepeating(
    base::RepeatingCallback<void(Args...)> callback) {
  return base::BindRepeating(
      [](base::RepeatingCallback<void(Args...)> callback,
         scoped_refptr<base::SequencedTaskRunner> sequence_runner,
         Args&&... args) {
        // Invoke the callback on the original sequence.
        if (sequence_runner->RunsTasksInCurrentSequence()) {
          callback.Run(std::forward<Args>(args)...);
        } else {
          sequence_runner->PostTask(
              FROM_HERE,
              base::BindRepeating(callback, std::forward<Args>(args)...));
        }
      },
      callback, base::SequencedTaskRunner::GetCurrentDefault());
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

// Binds a repeating callback to the current sequence.
// This is a convenience version, that allows you to use the same simplified
// style noted above on a repeating callback.
template <typename Functor, typename... Args>
auto BindToCurrentSequenceRepeating(Functor&& functor, Args&&... args) {
  auto callback = base::BindRepeating(std::forward<Functor>(functor),
                                      std::forward<Args>(args)...);
  return BindToCurrentSequenceRepeating(callback);
}

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_CALLBACK_UTILS_H_
