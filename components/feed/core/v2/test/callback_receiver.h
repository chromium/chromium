// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_
#define COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_

#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"

namespace feed {
namespace internal {

template <typename T>
std::optional<T> Nullopt() {
  return std::nullopt;
}

class CallbackReceiverBase {
 public:
  explicit CallbackReceiverBase(base::RunLoop* run_loop = nullptr)
      : run_loop_(run_loop) {}

  void Clear() { called_ = false; }
  bool called() const { return called_; }
  void RunUntilCalled();
  void Done();

 private:
  bool called_ = false;
  raw_ptr<base::RunLoop> run_loop_;
};

}  // namespace internal

template <typename... T>
class CallbackReceiver : public internal::CallbackReceiverBase {
 public:
  explicit CallbackReceiver(base::RunLoop* run_loop = nullptr)
      : CallbackReceiverBase(run_loop) {}

  void Done(T... results) {
    results_ = std::make_tuple(std::move(results)...);
    CallbackReceiverBase::Done();
  }
  base::OnceCallback<void(T...)> Bind() {
    return base::BindOnce(&CallbackReceiver::Done, GetWeakPtr());
  }
  base::RepeatingCallback<void(T...)> BindRepeating() {
    return base::BindRepeating(&CallbackReceiver::Done, GetWeakPtr());
  }

  void Clear() {
    CallbackReceiverBase::Clear();
    results_ = std::make_tuple(internal::Nullopt<T>()...);
  }

  // Get a result by its position in the arguments to Done().
  // Call GetResult() for the first argument or GetResult<I>().
  template <size_t I = 0>
  typename std::tuple_element<I, std::tuple<std::optional<T>...>>::type&
  GetResult() {
    return std::get<I>(results_);
  }

  template <size_t I = 0>
  typename std::tuple_element<I, std::tuple<T...>>::type& RunAndGetResult() {
    RunUntilCalled();
    return std::get<I>(results_).value();
  }

  // Get a result by its type. Won't compile if there is more than one matching
  // type.
  template <class C>
  std::optional<C>& GetResult() {
    return std::get<std::optional<C>>(results_);
  }

 private:
  base::WeakPtr<CallbackReceiver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::tuple<std::optional<T>...> results_;
  base::WeakPtrFactory<CallbackReceiver> weak_ptr_factory_{this};
};

template <>
class CallbackReceiver<> : public internal::CallbackReceiverBase {
 public:
  explicit CallbackReceiver(base::RunLoop* run_loop = nullptr)
      : CallbackReceiverBase(run_loop) {}

  base::OnceClosure Bind() {
    return base::BindOnce(&CallbackReceiverBase::Done, base::Unretained(this));
  }
  base::RepeatingClosure BindRepeating() {
    return base::BindRepeating(&CallbackReceiverBase::Done,
                               base::Unretained(this));
  }
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_
