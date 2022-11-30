// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/time_serialization.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

TEST(TimeSerializationTest, TimeSerialization) {
  std::vector<base::Time> values_to_test = {base::Time::Min(), base::Time(),
                                            base::Time() + base::Hours(1),
                                            base::Time::Max()};
  for (const base::Time& value : values_to_test) {
    EXPECT_EQ(SerializeTime(value), value.ToInternalValue());
    EXPECT_EQ(base::Time::FromInternalValue(SerializeTime(value)), value);
    EXPECT_EQ(DeserializeTime(SerializeTime(value)), value);
  }
}

}  // namespace ntp_snippets
