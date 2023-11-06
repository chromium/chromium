// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/time.h"

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"

namespace syncer {

int64_t TimeToProtoTime(const base::Time& t) {
  return t.InMillisecondsSinceUnixEpoch();
}

base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromMillisecondsSinceUnixEpoch(proto_t);
}

std::string GetTimeDebugString(const base::Time& t) {
  return base::UnlocalizedTimeFormatWithPattern(t, "yyyy-MM-dd HH:mm:ss X");
}

}  // namespace syncer
