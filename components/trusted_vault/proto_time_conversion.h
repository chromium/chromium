// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_PROTO_TIME_CONVERSION_H_
#define COMPONENTS_TRUSTED_VAULT_PROTO_TIME_CONVERSION_H_

#include <cstdint>

namespace base {
class Time;
}

namespace trusted_vault {

// Converts a time object to the format used in trusted vault protobufs (ms
// since the Unix epoch).
int64_t TimeToProtoTime(base::Time time);

// Converts a time field from trusted vault protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_time);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_PROTO_TIME_CONVERSION_H_
