// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_TEST_MATCHERS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_TEST_MATCHERS_H_

#include <tuple>

#include "base/check.h"
#include "base/location.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill::test {

// Stores the address of the `I`th argument in `*ptr`.
// See LazyRef() for a usage example.
template <size_t I = 0, typename T>
auto SaveArgPtr(T** ptr) {
  return [ptr](auto&&... args) { *ptr = &std::get<I>(std::tie(args...)); };
}

// `LazyRef(x)` behaves like `::testing::Ref(x)` but dereferences `x` at match
// time.
//
// Usage:
//    MockFunction<void(Foo&)> fun;
//    {
//      Foo* foo_ptr = nullptr;
//      EXPECT_CALL(fun, Call).WillOnce(SaveArgPtr(&foo_ptr));
//      EXPECT_CALL(fun, Call(LazyRef(foo_ptr)));
//    }
//    Foo foo;
//    fun.Call(foo);
//    fun.Call(foo);
template <typename T>
auto LazyRef(T*& ptr, base::Location loc = FROM_HERE) {
  return ::testing::Truly([&ptr, loc](const T& actual) {
    CHECK(ptr) << "LazyRef is unbound in " << loc.ToString();
    return std::addressof(actual) == ptr;
  });
}

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_TEST_MATCHERS_H_
