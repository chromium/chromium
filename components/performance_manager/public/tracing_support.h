// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_TRACING_SUPPORT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_TRACING_SUPPORT_H_

#include <optional>

#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

class ProcessNode;

// Returns a Perfetto NamedTrack with the given `name` (and optionally `id`),
// which will be nested under the ProcessTrack for the process hosted in
// `process_node`. The browser process can write trace events to this track,
// which will be grouped with the child process in traces.
perfetto::NamedTrack CreateProcessTracingTrack(const ProcessNode* process_node,
                                               perfetto::StaticString name,
                                               uint64_t id = 0);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_TRACING_SUPPORT_H_
