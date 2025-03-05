// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/date_info.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Tests setting the date incrementally.
TEST(DateInfo, SetDateIncrementally) {
  DateInfo info;
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"");

  info.SetDate(u"12/2022", u"MM/YYYY");
  EXPECT_EQ(info.GetDate(u"YYYY-MM"), u"2022-12");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"");  // Because `info` has no day.

  info.SetDate(u"16", u"DD");
  EXPECT_EQ(info.GetDate(u""), u"");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"2022-12-16");
}

// Tests setting the date incrementally.
TEST(DateInfo, SetDateOrReset) {
  DateInfo info;
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"");

  info.SetDate(u"16/12/2022", u"DD/MM/YYYY");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"2022-12-16");

  info.SetDate(u"foobar", u"DD/MM/YYYY");
  EXPECT_EQ(info.GetDate(u""), u"");
}

}  // namespace
}  // namespace autofill
