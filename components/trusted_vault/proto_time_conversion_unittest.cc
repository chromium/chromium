// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/proto_time_conversion.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::Eq;

TEST(ProtoTimeConversionTest, ShouldConvertProtoTimeToTimeAndBack) {
  const int64_t proto_time_ms_since_unix_epoch = 100;
  const base::Time time = base::Time::UnixEpoch() +
                          base::Milliseconds(proto_time_ms_since_unix_epoch);
  EXPECT_THAT(ProtoTimeToTime(proto_time_ms_since_unix_epoch), Eq(time));
  EXPECT_THAT(TimeToProtoTime(time), Eq(proto_time_ms_since_unix_epoch));
}

}  // namespace

}  // namespace trusted_vault
