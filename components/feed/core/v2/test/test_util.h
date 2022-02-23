// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_TEST_UTIL_H_
#define COMPONENTS_FEED_CORE_V2_TEST_TEST_UTIL_H_

#include <string>

#include "base/callback.h"
#include "base/time/time.h"

// Some functionality shared among feed tests.
namespace feed {

// Although time is mocked through TaskEnvironment, it does drift by small
// amounts.
const base::TimeDelta kEpsilon = base::Milliseconds(5);
#define EXPECT_TIME_EQ(WANT, GOT)                    \
  {                                                  \
    base::Time want___ = (WANT), got___ = (GOT);     \
    if (got___ != want___) {                         \
      EXPECT_LT(want___ - ::feed::kEpsilon, got___); \
      EXPECT_GT(want___ + ::feed::kEpsilon, got___); \
    }                                                \
  }

// This is EXPECT_EQ, but also dumps the string values for ease of reading.
#define EXPECT_STRINGS_EQUAL(WANT, GOT)                     \
  {                                                         \
    std::string want___ = (WANT), got___ = (GOT);           \
    EXPECT_EQ(want___, got___) << "Wanted:\n"               \
                               << want___ << "\nBut got:\n" \
                               << got___;                   \
  }

// Execute a runloop until `criteria` is true. If the criteria are not true
// after 1000 iterations, ASSERT with the content of
// `failure_message_callback.Run()`.
void RunLoopUntil(base::RepeatingCallback<bool()> criteria,
                  base::OnceCallback<std::string()> failure_message_callback);

void RunLoopUntil(base::RepeatingCallback<bool()> criteria,
                  const std::string& failure_message);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_TEST_UTIL_H_
