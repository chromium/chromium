// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_SYSTEM_PROFILE_METADATA_RECORDER_H_
#define COMPONENTS_TRACING_COMMON_SYSTEM_PROFILE_METADATA_RECORDER_H_

#include <string_view>

#include "base/version_info/channel.h"
#include "components/tracing/tracing_export.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

void TRACING_EXPORT RecordSystemProfileMetadata(
    perfetto::protos::pbzero::ChromeEventBundle* bundle);

void TRACING_EXPORT FillChromeMetadataPacket(
    version_info::Channel channel,
    perfetto::protos::pbzero::ChromeMetadataPacket* packet);

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_SYSTEM_PROFILE_METADATA_RECORDER_H_
