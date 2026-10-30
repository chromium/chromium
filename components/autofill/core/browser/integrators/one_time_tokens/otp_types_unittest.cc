// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

TEST(OtpTypesTest, IsStrictlyNewerThan_EmailReceivedTimestampTakesPrecedence) {
  const base::Time t0 = base::Time::FromSecondsSinceUnixEpoch(1000);
  const base::Time t1 = t0 + base::Seconds(10);
  const base::TimeTicks arrival0 = base::TimeTicks() + base::Seconds(100);
  const base::TimeTicks arrival1 = arrival0 + base::Seconds(5);

  OtpTokenIdentity older("111111", "a@example.test", t0, arrival1);
  OtpTokenIdentity newer("222222", "a@example.test", t1, arrival0);

  EXPECT_TRUE(newer.IsStrictlyNewerThan(older));
  EXPECT_FALSE(older.IsStrictlyNewerThan(newer));
}

TEST(OtpTypesTest, IsStrictlyNewerThan_FallsBackToOnDeviceArrival) {
  const base::Time t0 = base::Time::FromSecondsSinceUnixEpoch(1000);
  const base::TimeTicks arrival0 = base::TimeTicks() + base::Seconds(100);
  const base::TimeTicks arrival1 = arrival0 + base::Seconds(5);

  OtpTokenIdentity sms_older("111111", std::nullopt, std::nullopt, arrival0);
  OtpTokenIdentity sms_newer("111111", std::nullopt, std::nullopt, arrival1);
  EXPECT_TRUE(sms_newer.IsStrictlyNewerThan(sms_older));
  EXPECT_FALSE(sms_older.IsStrictlyNewerThan(sms_newer));

  // SMS vs Gmail compares on-device arrival time.
  OtpTokenIdentity gmail_older("222222", "a@example.test", t0, arrival0);
  EXPECT_TRUE(sms_newer.IsStrictlyNewerThan(gmail_older));
  EXPECT_FALSE(gmail_older.IsStrictlyNewerThan(sms_newer));

  // An exact tie in `on_device_arrival` is not strictly newer.
  OtpTokenIdentity same_time_same_val("111111", std::nullopt, std::nullopt,
                                      arrival0);
  OtpTokenIdentity same_time_diff_val("222222", std::nullopt, std::nullopt,
                                      arrival0);
  EXPECT_FALSE(same_time_same_val.IsStrictlyNewerThan(sms_older));
  EXPECT_FALSE(same_time_diff_val.IsStrictlyNewerThan(sms_older));
}

}  // namespace
}  // namespace autofill
