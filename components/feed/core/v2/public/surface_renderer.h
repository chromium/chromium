// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_SURFACE_RENDERER_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_SURFACE_RENDERER_H_

#include <string_view>

#include "base/observer_list_types.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"

namespace feedui {
class StreamUpdate;
}

namespace feed {

// Consumes stream data for a single `StreamType` and displays it to the user.
// A feed surface may be attached and detached multiple times across its
// lifetime.
class SurfaceRenderer {
 public:
  virtual ~SurfaceRenderer() = default;

  // Called after registering the observer to provide the full stream state.
  // Also called whenever the stream changes.
  virtual void StreamUpdate(const feedui::StreamUpdate&) = 0;

  // Access to the xsurface data store.
  virtual void ReplaceDataStoreEntry(std::string_view key,
                                     std::string_view data) = 0;
  virtual void RemoveDataStoreEntry(std::string_view key) = 0;
  // Returns the ReliabilityLogger associated with this surface.
  virtual ReliabilityLoggingBridge& GetReliabilityLoggingBridge() = 0;
};

}  // namespace feed
#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_SURFACE_RENDERER_H_
