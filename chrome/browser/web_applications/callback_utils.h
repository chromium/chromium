// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CALLBACK_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CALLBACK_UTILS_H_

#include "base/functional/callback.h"

namespace web_app {

// RunChainedCallbacks() runs multiple callbacks chained together by
// successively binding the final callback as parameter to the one before it
// until the entire sequence has been bound together.
//
// Example usage:
//
// class CounterStorage {
//  public:
//   void AddToCounter(size_t value, base::OnceClosure callback) {
//     auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
//     RunChainedCallbacks(
//         base::BindOnce(&CounterStorage::AcquireFileLock, weak_ptr),
//         base::BindOnce(&CounterStorage::StashFileLock, weak_ptr),
//         base::BindOnce(&CounterStorage::ReadCounter, weak_ptr),
//         base::BindOnce(&CounterStorage::Add, weak_ptr, value),
//         base::BindOnce(&CounterStorage::WriteCounter, weak_ptr),
//         base::BindOnce(&CounterStorage::ReleaseFileLock, weak_ptr),
//         std::move(callback));
//   }
//
//  private:
//   void AcquireFileLock(
//       base::OnceCallback<void(FileLock)> next_step_callback);
//   void StashFileLock(base::OnceClosure next_step_callback, FileLock lock);
//   void ReadCounter(base::OnceCallback<void(size_t)> next_step_callback);
//   void Add(size_t value,
//            base::OnceCallback<void(size_t)> next_step_callback,
//            size_t count);
//   void WriteCounter(base::OnceClosure next_step_callback, size_t count);
//   void ReleaseFileLock(base::OnceClosure next_step_callback);
//
//   std::optional<FileLock> lock_;
//   base::WeakPtrFactory<CounterStorage> weak_ptr_factory_{this};
// };
//
// The alternate way to write AddLog() without RunChainedCallbacks() would be:
// base::BindOnce(
//     &Logger::AcquireFileLock,
//     weak_ptr,
//     base::BindOnce(
//         &Logger::StashFileLock,
//         weak_ptr,
//         base::BindOnce(
//             &Logger::ReadCounter,
//             weak_ptr,
//             base::BindOnce(
//                 &Logger::Add,
//                 weak_ptr,
//                 value,
//                 base::BindOnce(&Logger::WriteCounter,
//                                weak_ptr,
//                                base::BindOnce(&Logger::ReleaseFileLock,
//                                               weak_ptr,
//                                               std::move(callback)))))));
//
// RunChainedCallbacks() allows writing single action async methods that don't
// need to know about the high level procedure while avoiding messy indented
// nesting of multiple base::BindOnce() calls.

template <typename Callback>
Callback ChainCallbacks(Callback&& callback) {
  return std::forward<Callback>(callback);
}

template <typename FirstCallback, typename... NextCallbacks>
decltype(auto) ChainCallbacks(FirstCallback&& first_callback,
                              NextCallbacks&&... next_callbacks) {
  return base::BindOnce(std::forward<FirstCallback>(first_callback),
                        ChainCallbacks<NextCallbacks...>(
                            std::forward<NextCallbacks>(next_callbacks)...));
}

template <typename... Callbacks>
decltype(auto) RunChainedCallbacks(Callbacks&&... callbacks) {
  return ChainCallbacks(std::forward<Callbacks>(callbacks)...).Run();
}

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CALLBACK_UTILS_H_
