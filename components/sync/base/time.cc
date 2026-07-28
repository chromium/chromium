// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/time.h"

#include <cmath>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace syncer {

int64_t TimeToProtoTime(const base::Time& t) {
  return t.InMillisecondsSinceUnixEpoch();
}

base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromMillisecondsSinceUnixEpoch(proto_t);
}

std::string GetTimeDebugString(const base::Time& t) {
  base::Time::Exploded local_exploded;
  t.LocalExplode(&local_exploded);

  base::Time local_as_utc;
  std::string offset_str = "Z";
  if (base::Time::FromUTCExploded(local_exploded, &local_as_utc)) {
    base::TimeDelta offset = local_as_utc - t;
    if (!offset.is_zero()) {
      int64_t total_minutes = offset.InMinutes();
      int64_t hours = total_minutes / 60;
      int64_t minutes = std::abs(total_minutes % 60);
      if (minutes == 0) {
        offset_str = base::StringPrintf("%+03d", static_cast<int>(hours));
      } else {
        offset_str = base::StringPrintf("%+03d%02d", static_cast<int>(hours),
                                        static_cast<int>(minutes));
      }
    }
  }

  return base::StringPrintf(
      "%04d-%02d-%02d %02d:%02d:%02d %s", local_exploded.year,
      local_exploded.month, local_exploded.day_of_month, local_exploded.hour,
      local_exploded.minute, local_exploded.second, offset_str.c_str());
}

}  // namespace syncer
