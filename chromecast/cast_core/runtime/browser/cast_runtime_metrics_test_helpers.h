// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_TEST_HELPERS_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_TEST_HELPERS_H_

#include <cstdint>
#include <memory>
#include <string>

#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.pb.h"
#include "third_party/metrics_proto/cast_logs.pb.h"

namespace chromecast {

// Gets the number of samples in the bucket in |histogram| that |value| would go
// into.
int64_t GetCount(int64_t value, const cast::metrics::Histogram* histogram);

// Returns nullptr if parsing failed.
std::unique_ptr<::metrics::CastLogsProto_CastEventProto> ParseCastEventProto(
    const std::string& serialized);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_TEST_HELPERS_H_
