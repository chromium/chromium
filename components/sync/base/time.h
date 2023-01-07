// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_TIME_H_
#define COMPONENTS_SYNC_BASE_TIME_H_

#include <stdint.h>

#include <string>

#include "base/time/time.h"

namespace syncer {

// Converts a time object to the format used in sync protobufs (ms
// since the Unix epoch).
int64_t TimeToProtoTime(const base::Time& t);

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t);

std::string GetTimeDebugString(const base::Time& t);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_TIME_H_
