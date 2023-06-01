// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/proto_time_conversion.h"

#include "base/time/time.h"

namespace trusted_vault {

int64_t TimeToProtoTime(base::Time time) {
  return (time - base::Time::UnixEpoch()).InMilliseconds();
}

base::Time ProtoTimeToTime(int64_t proto_time) {
  return base::Time::UnixEpoch() + base::Milliseconds(proto_time);
}

}  // namespace trusted_vault
