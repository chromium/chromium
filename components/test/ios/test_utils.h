// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_IOS_TEST_UTILS_H_
#define COMPONENTS_TEST_IOS_TEST_UTILS_H_

#import "third_party/ocmock/OCMock/OCMock.h"

// Expects that the i-th parameter of the invocation is equal to `expected`
#define andCompareObjectAtIndex(expected, index)                      \
  andDo(^(NSInvocation * invocation) {                                \
    /* Introducing an extra variable in case `expected` is "param".*/ \
    std::add_const_t<decltype(expected)> expected_ = (expected);      \
    /* Not __unsafe_unretained because type is a c++ class.*/         \
    std::remove_reference_t<decltype(expected)>* param = nullptr;     \
    [invocation getArgument:&param atIndex:index + 2];                \
    EXPECT_EQ(*param, expected_);                                     \
  })

// Sets `variable` to the `index`-th parameter.
// The type of variable must be a non-objective C value.
#define andAssignStructParameterToVariable(variable, index)   \
  andDo(^(NSInvocation * invocation) {                        \
    /* Not __unsafe_unretained because type is a c++ class.*/ \
    [invocation getArgument:&variable atIndex:index + 2];     \
  })

// Sets `variable` to the `index`-th parameter.
// The type of variable must be a non-objective C value.
#define andAssignStructParameterToVariable(variable, index)   \
  andDo(^(NSInvocation * invocation) {                        \
    /* Not __unsafe_unretained because type is a c++ class.*/ \
    [invocation getArgument:&variable atIndex:index + 2];     \
  })

// Sets `variable` to the value whose address is `index`-th parameter.
// The type of variable must be a non-objective C value.
#define andAssignStructParameterAtAddressToVariable(variable, index) \
  andDo(^(NSInvocation * invocation) {                               \
    /* Not __unsafe_unretained because type is a c++ class.*/        \
    const decltype(variable)* param = nullptr;                       \
    [invocation getArgument:&param atIndex:index + 2];               \
    variable = *param;                                               \
  })

#define AssignValueToVariable(variable)                    \
  [OCMArg checkWithBlock:^BOOL(decltype(variable) param) { \
    variable = param;                                      \
    return YES;                                            \
  }]

#define CopyValueToVariable(variable)                      \
  [OCMArg checkWithBlock:^BOOL(decltype(variable) param) { \
    variable = [param copy];                               \
    return YES;                                            \
  }]

namespace ios::OCM {

// Returns a OCMArg that accepts any pointer, and can be used as argument of
// pointer of type T*.
template <typename T>
T* AnyPointer() {
  return static_cast<T*>([OCMArg anyPointer]);
}

}  // namespace ios::OCM

#endif  // COMPONENTS_TEST_IOS_TEST_UTILS_H_
