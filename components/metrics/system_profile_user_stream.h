// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SYSTEM_PROFILE_USER_STREAM_H_
#define COMPONENTS_METRICS_SYSTEM_PROFILE_USER_STREAM_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"

namespace metrics {

// Minidump user stream type that carries the serialized `SystemProfileProto`.
// Must match the stream ID expected by the crash server.
inline constexpr uint32_t kSystemProfileUserStreamType = 0x4B6B0003;

// Maximum payload capacity of the SystemProfile user stream. Stream owners must
// allocate at least this much; larger payloads are never passed to the writer
// (see `UMA.SystemProfile.UserStreamOverflow`).
inline constexpr uint32_t kSystemProfileUserStreamCapacityBytes = 32 * 1024;

// Callback that delivers `data` to the minidump user stream with
// `user_stream_type`. Returns `false` if no such stream exists.
using UserStreamWriter =
    base::FunctionRef<bool(uint32_t user_stream_type,
                           base::span<const uint8_t> data)>;

// Records `UMA.SystemProfile.UserStreamOverflow` and, if the serialized
// `SystemProfileProto` `payload` fits in
// `kSystemProfileUserStreamCapacityBytes`, writes it using `writer`. Larger
// payloads are not written, so the stream keeps its previous payload.
void WriteSystemProfileUserStream(std::string_view payload,
                                  UserStreamWriter writer);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SYSTEM_PROFILE_USER_STREAM_H_
