// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_
#define COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"

namespace feed {
namespace internal {

template <typename T>
base::Optional<T> Nullopt() {
  return base::nullopt;
}

}  // namespace internal

template <typename... T>
class CallbackReceiver {
 public:
  explicit CallbackReceiver(base::RunLoop* run_loop = nullptr)
      : run_loop_(run_loop) {}
  void Done(T... results) {
    results_ = std::make_tuple(std::move(results)...);
    if (run_loop_)
      run_loop_->Quit();
  }
  base::OnceCallback<void(T...)> Bind() {
    return base::BindOnce(&CallbackReceiver::Done, base::Unretained(this));
  }

  void Clear() { results_ = std::make_tuple(internal::Nullopt<T>()...); }

  // Get a result by its position in the arguments to Done().
  // Call GetResult() for the first argument or GetResult<I>().
  template <size_t I = 0>
  typename std::tuple_element<I, std::tuple<base::Optional<T>...>>::type&
  GetResult() {
    return std::get<I>(results_);
  }

  // Get a result by its type. Won't compile if there is more than one matching
  // type.
  template <class C>
  base::Optional<C>& GetResult() {
    return std::get<base::Optional<C>>(results_);
  }

 private:
  std::tuple<base::Optional<T>...> results_;
  base::RunLoop* run_loop_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_
