// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_TEST_UTIL_H_
#define COMPONENTS_FEED_CORE_V2_TEST_TEST_UTIL_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// Trims whitespace from begin and end of all lines of text.
std::string TrimLines(std::string_view text);

// Does the protobuf argument's ToTextProto() output match message? Allows some
// whitespace differences.
MATCHER_P(EqualsTextProto, message, message) {
  std::string actual_string = ToTextProto(arg);
  if (TrimLines(actual_string) != TrimLines(message)) {
    return testing::ExplainMatchResult(testing::Eq(message), ToTextProto(arg),
                                       result_listener);
  } else {
    return true;
  }
}

// Does the protobuf argument match message?
MATCHER_P(EqualsProto, message, ToTextProto(message)) {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
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
