// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/tracing_support.h"

#include <optional>

#include "components/performance_manager/graph/process_node_impl.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

bool HasProcessTracingTrack(const ProcessNode* process_node) {
  return ProcessNodeImpl::FromNode(process_node)->tracing_track().has_value();
}

perfetto::NamedTrack CreateProcessTracingTrack(const ProcessNode* process_node,
                                               perfetto::DynamicString name,
                                               uint64_t id) {
  return perfetto::NamedTrack(
      name, id,
      ProcessNodeImpl::FromNode(process_node)->tracing_track().value());
}

perfetto::NamedTrack CreateProcessTracingTrack(const ProcessNode* process_node,
                                               perfetto::StaticString name,
                                               uint64_t id) {
  return perfetto::NamedTrack(
      name, id,
      ProcessNodeImpl::FromNode(process_node)->tracing_track().value());
}

}  // namespace performance_manager
