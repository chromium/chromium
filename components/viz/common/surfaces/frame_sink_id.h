// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_ID_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_ID_H_

#include <stdint.h>

#include <compare>
#include <iosfwd>
#include <string>
#include <string_view>

#include "base/hash/hash.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"

namespace viz {

// A FrameSinkId uniquely identifies a CompositorFrameSink and the client that
// uses it within the Viz compositing system. FrameSinkIds are used the first
// component of a SurfaceId, which is a FrameSinkId + LocalSurfaceId, to ensure
// SurfaceIds are unique across all CompositorFrameSinks.
//
// FrameSinkId consists of:
//
// - client_id: This part uniquely identifies a client namespace, typically one
//              per process.
//
// - sink_id: This part uniquely identifies a FrameSink within the client
//            namespace. This component may be allocated by the client specified
//            by the client_id.
//
// The FrameSinkId for a given client_id may be allocated using a
// FrameSinkIdAllocator.
class VIZ_COMMON_EXPORT FrameSinkId {
 public:
  constexpr FrameSinkId() : client_id_(0), sink_id_(0) {}

  constexpr FrameSinkId(const FrameSinkId& other) = default;
  constexpr FrameSinkId& operator=(const FrameSinkId& other) = default;

  constexpr FrameSinkId(uint32_t client_id, uint32_t sink_id)
      : client_id_(client_id), sink_id_(sink_id) {}

  constexpr bool is_valid() const { return client_id_ != 0 || sink_id_ != 0; }

  constexpr uint32_t client_id() const { return client_id_; }

  constexpr uint32_t sink_id() const { return sink_id_; }

  friend std::strong_ordering operator<=>(const FrameSinkId&,
                                          const FrameSinkId&) = default;

  size_t hash() const { return base::HashInts(client_id_, sink_id_); }

  std::string ToString() const;

  std::string ToString(std::string_view debug_label) const;

  using TraceProto = perfetto::protos::pbzero::FrameSinkId;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const;

 private:
  uint32_t client_id_;
  uint32_t sink_id_;
};

VIZ_COMMON_EXPORT std::ostream& operator<<(std::ostream& out,
                                           const FrameSinkId& frame_sink_id);

struct FrameSinkIdHash {
  size_t operator()(const FrameSinkId& key) const { return key.hash(); }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_ID_H_
