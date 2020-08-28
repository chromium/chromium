// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_FUTURE_VALUE_H_
#define CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_FUTURE_VALUE_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/component_export.h"

namespace chromeos {
namespace bloom {

// A future object that will invoke the given callback when the value becomes
// available.
//
// This is especially useful if your code needs to wait for 2 values to be
// fetched, as you can then simply:
//   * Fetch the first value asynchronously, and pass it to the first future.
//   * Fetch the second value asynchronously, and pass it to the second future.
//   * Call WaitForBothValues() and pass it a callback that will be called with
//     both values.
//
// Example:
//
// // Waits for 2 values to be fetched asynchronously.
// // Calls |MyWaiter::OnValuesReady()| when both values are available.
// class MyWaiter {
//  public:
//   void Wait() {
//     // Fetch first value and pass it to the first future. Note you must use a
//     // weak_ptr if |first_fetcher_| is not owned by this class.
//     first_fetcher_.FetchValue(
//         base::BindOnce([](FutureValue<FirstType> * future, FirstType &&
//         value)) {
//           future->SetValue(std::move(value));
//         },
//         &first_future_);
//
//     // Fetch second value and pass it to the second future. Note you must use
//     // a weak_ptr if |second_fetcher_| is not owned by this class.
//     second_fetcher_.FetchValue(
//         base::BindOnce([](FutureValue<SecondType> * future, SecondType &&
//         value))
//         {
//           future->SetValue(std::move(value));
//         },
//         &second_future_);
//
//     // Now wait for both. OnValuesReady() will be called when both values are
//     // available. Note that we can always use |base::Unretained| here, as
//     // both |FutureValue| objects are owned by this class.
//     WhenBothAreReady(
//         &first_future_, &second_future_,
//         base::BindOnce(&MyWaiter::OnValuesReady, base::Unretained(this)));
//   }
//
//  private:
//   void OnValuesReady(FirstType&& first_value, SecondType&& second_value) {
//     // Both values are available now.
//   }
//
//   FirstFetcher first_fetcher_;
//   FutureValue<FirstType> first_future_;
//
//   SecondFetcher second_fetcher_;
//   FutureValue<SecondType> second_future_;
// };
//
template <typename _Type>
class COMPONENT_EXPORT(BLOOM) FutureValue {
 public:
  using Callback = base::OnceCallback<void(_Type&& value)>;

  void WhenReady(Callback callback) {
    DCHECK(!callback_);
    callback_ = std::move(callback);
    RunCallbackIfReady();
  }

  void SetValue(_Type&& value) {
    DCHECK(!value_);
    value_ = std::move(value);
    RunCallbackIfReady();
  }

 private:
  void RunCallbackIfReady() {
    if (value_ && callback_)
      std::move(callback_).Run(std::move(value_).value());
  }

  base::Optional<_Type> value_;
  Callback callback_;
};

template <typename _First, typename _Second>
using DoubleCallback =
    base::OnceCallback<void(_First&& first_result, _Second&& second_result)>;

// Waits for both futures to be ready, and then passes both results to
// |callback|.
template <typename _First, typename _Second>
void COMPONENT_EXPORT(BLOOM)
    WhenBothAreReady(FutureValue<_First>* first_future,
                     FutureValue<_Second>* second_future,
                     DoubleCallback<_First, _Second> callback) {
  using Callback = DoubleCallback<_First, _Second>;

  first_future->WhenReady(base::BindOnce(
      [](Callback callback, FutureValue<_Second>* second_future,
         _First&& first_value) {
        // We've got the first value, now wait for the second.
        second_future->WhenReady(base::BindOnce(
            [](Callback callback, _First&& first_value,
               _Second&& second_value) {
              // We've got both values, so call the callback.
              std::move(callback).Run(std::move(first_value),
                                      std::move(second_value));
            },
            std::move(callback), std::move(first_value)));
      },
      std::move(callback), second_future));
}

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_FUTURE_VALUE_H_
