// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_SYSTEM_PROFILE_METADATA_RECORDER_H_
#define COMPONENTS_TRACING_COMMON_SYSTEM_PROFILE_METADATA_RECORDER_H_

#include <string_view>

#include "components/tracing/tracing_export.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

void TRACING_EXPORT RecordSystemProfileMetadata(
    perfetto::protos::pbzero::ChromeEventBundle* bundle);

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_SYSTEM_PROFILE_METADATA_RECORDER_H_
