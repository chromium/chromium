// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/experiment_labels.h"

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::StartsWith;
using ::testing::StrEq;

namespace installer {

TEST(ExperimentLabels, Value) {
  static constexpr wchar_t kDummyValue[] =
      L"name=value|Fri, 14 Aug 2015 16:13:03 GMT";
  ExperimentLabels labels(kDummyValue);

  EXPECT_THAT(labels.value(), StrEq(kDummyValue));
}

TEST(ExperimentLabels, GetValueForLabel) {
  static constexpr wchar_t kDummyValue[] =
      L"name=value|Fri, 14 Aug 2015 16:13:03 GMT;"
      L"name2=value2|Fri, 14 Aug 2015 16:13:03 GMT";
  ExperimentLabels labels(kDummyValue);

  base::WStringPiece value = labels.GetValueForLabel(L"name");
  EXPECT_EQ(value, L"value");

  value = labels.GetValueForLabel(L"name2");
  EXPECT_EQ(value, L"value2");

  value = labels.GetValueForLabel(L"name3");
  EXPECT_TRUE(value.empty());
}

TEST(ExperimentLabels, SetValueForLabel) {
  ExperimentLabels label(L"");

  label.SetValueForLabel(L"name", L"value", base::Seconds(1));
  base::WStringPiece value = label.GetValueForLabel(L"name");
  EXPECT_EQ(value, L"value");
  EXPECT_THAT(label.value(), StartsWith(L"name=value|"));

  label.SetValueForLabel(L"name", L"othervalue", base::Seconds(1));
  value = label.GetValueForLabel(L"name");
  EXPECT_EQ(value, L"othervalue");
  EXPECT_THAT(label.value(), StartsWith(L"name=othervalue|"));

  label.SetValueForLabel(L"othername", L"somevalue", base::Seconds(1));
  value = label.GetValueForLabel(L"othername");
  EXPECT_EQ(value, L"somevalue");
  EXPECT_THAT(label.value(), HasSubstr(L"name=othervalue|"));
  EXPECT_THAT(label.value(), HasSubstr(L"othername=somevalue|"));
}

TEST(ExperimentLabels, TimeFormatting) {
  base::Time::Exploded exploded = {};
  exploded.year = 2015;
  exploded.month = 8;
  exploded.day_of_week = 5;
  exploded.day_of_month = 14;
  exploded.hour = 16;
  exploded.minute = 13;
  exploded.second = 3;

  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCExploded(exploded, &time));

  ExperimentLabels label(L"");
  label.SetValueForLabel(L"name", L"value", time);
  EXPECT_THAT(label.value(),
              StrEq(L"name=value|Fri, 14 Aug 2015 16:13:03 GMT"));
}

}  // namespace installer
