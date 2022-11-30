// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace elevation_service {

class ElevatorTest : public ::testing::Test {};

TEST_F(ElevatorTest, StringHandlingTest) {
  std::string str;
  const std::string kFirstString("hello again");
  Elevator::AppendStringWithLength(kFirstString, str);
  EXPECT_EQ(sizeof(uint32_t) + kFirstString.length(), str.length());

  const std::string kSecondString("another string");
  Elevator::AppendStringWithLength(kSecondString, str);
  EXPECT_EQ(sizeof(uint32_t) + kFirstString.length() + sizeof(uint32_t) +
                kSecondString.length(),
            str.length());

  const std::string first_str = Elevator::PopFromStringFront(str);
  EXPECT_EQ(kFirstString, first_str);
  EXPECT_EQ(sizeof(uint32_t) + kSecondString.length(), str.length());

  const std::string second_str = Elevator::PopFromStringFront(str);
  EXPECT_EQ(kSecondString, second_str);

  const std::string error_str = Elevator::PopFromStringFront(str);
  EXPECT_TRUE(error_str.empty());

  std::string str2;

  Elevator::AppendStringWithLength("", str2);
  EXPECT_EQ(sizeof(uint32_t), str2.length());

  std::string empty_str = Elevator::PopFromStringFront(str2);
  EXPECT_TRUE(empty_str.empty());
}

}  // namespace elevation_service
