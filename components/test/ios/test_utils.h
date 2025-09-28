// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_IOS_TEST_UTILS_H_
#define COMPONENTS_TEST_IOS_TEST_UTILS_H_

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

#endif  // COMPONENTS_TEST_IOS_TEST_UTILS_H_
