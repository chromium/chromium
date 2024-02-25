// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_BUNDLE_ID_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_BUNDLE_ID_H_

#include <stdint.h>

#include <compare>

#include "base/hash/hash.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// A FrameSinkBundleId uniquely identifies a FrameSinkBundle and the client that
// uses it within the Viz compositing system.
class VIZ_COMMON_EXPORT FrameSinkBundleId {
 public:
  constexpr FrameSinkBundleId() = default;
  constexpr FrameSinkBundleId(const FrameSinkBundleId& other) = default;
  constexpr FrameSinkBundleId& operator=(const FrameSinkBundleId& other) =
      default;
  constexpr FrameSinkBundleId(uint32_t client_id, uint32_t bundle_id)
      : client_id_(client_id), bundle_id_(bundle_id) {}

  constexpr bool is_valid() const { return client_id_ != 0 || bundle_id_ != 0; }
  constexpr uint32_t client_id() const { return client_id_; }
  constexpr uint32_t bundle_id() const { return bundle_id_; }

  friend std::strong_ordering operator<=>(const FrameSinkBundleId&,
                                          const FrameSinkBundleId&) = default;

  size_t hash() const { return base::HashInts(client_id_, bundle_id_); }

 private:
  uint32_t client_id_{0};
  uint32_t bundle_id_{0};
};

struct FrameSinkBundleIdHash {
  size_t operator()(const FrameSinkBundleId& key) const { return key.hash(); }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_BUNDLE_ID_H_
