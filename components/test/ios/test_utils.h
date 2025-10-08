// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_IOS_TEST_UTILS_H_
#define COMPONENTS_TEST_IOS_TEST_UTILS_H_

#import "third_party/ocmock/OCMock/OCMock.h"

// This file contains helpers, in order to easily uses OCMStub and OCMExpect
// with methods whose arguments are pointers to c++ object.
//
// The method whose name start with `and` can be used as method defined on the
// OCMExpect/OCMStub. For example, let’s consider a method
// `-fooWithString:(std::string*)string`. Then OCMStub([mock_
// fooWithString:ios::OCM::AnyPointer<std::string]).
//    andCompareObjectAtIndex("foo", 0);
// checks that the parameter in the string foo. And
// std::string s;
// OCMStub([mock_ fooWithString:ios::OCM::AnyPointer<std::string]).
//   andAssignStructParameterAtAddressToVariable(s, 0);
// will set `s` to the value of the argument to `fooWithString`.
//
// Given how usual it is to save the value in a variable, a similar helper was
// introduced for NSObject. Let’s consider a method
// `-fooWithNSString:(NSString*)string`. You can assign the value sent to the
// argument with:
// __block NSString* s;
// OCMStub([mock_ fooWithNSString:AssignValueToVariable(s)]);
// or with `CopyValueToVariable` if the value is copyable.

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

// Sets `variable` to the value whose address is `index`-th parameter.
// The type of variable must be a non-objective C value.
#define andAssignStructParameterAtAddressToVariable(variable, index) \
  andDo(^(NSInvocation * invocation) {                               \
    /* Not __unsafe_unretained because type is a c++ class.*/        \
    const decltype(variable)* param = nullptr;                       \
    [invocation getArgument:&param atIndex:index + 2];               \
    variable = *param;                                               \
  })

// Sets `variable` to the NSObject value received by the API. This function
// should be called is a OCMExpect or OCMStub, as an argument of the
// mocked/stubbed function call.
#define AssignValueToVariable(variable)                    \
  [OCMArg checkWithBlock:^BOOL(decltype(variable) param) { \
    variable = param;                                      \
    return YES;                                            \
  }]

// Sets `variable` to a copy of the NSObject value received by the API. This
// objects must be copyable. This function should be called is a OCMExpect or
// OCMStub, as an argument of the mocked/stubbed function call.
#define CopyValueToVariable(variable)                      \
  [OCMArg checkWithBlock:^BOOL(decltype(variable) param) { \
    variable = [param copy];                               \
    return YES;                                            \
  }]

// Calls `block` with the `index`-th parameter.
// The type of the parameter must be a non-objective-C value.
#define andCallBlockWithParameterAtIndex(type, index, block)  \
  andDo(^(NSInvocation * invocation) {                        \
    /* Not __unsafe_unretained because type is a c++ class.*/ \
    std::remove_reference_t<type>* param = nullptr;           \
    [invocation getArgument:&param atIndex:index + 2];        \
    block(param);                                             \
  })

namespace ios::OCM {
// Returns a OCMArg that accepts any pointer, and can be used as argument of
// pointer of type T*.
template <typename T>
T* AnyPointer() {
  return static_cast<T*>([OCMArg anyPointer]);
}

}  // namespace ios::OCM

#endif  // COMPONENTS_TEST_IOS_TEST_UTILS_H_
