// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_BROWSER_SCOPED_CG_WINDOW_ID_H_
#define COMPONENTS_REMOTE_COCOA_BROWSER_SCOPED_CG_WINDOW_ID_H_

#include <stdint.h>

#include "components/remote_cocoa/browser/remote_cocoa_browser_export.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace remote_cocoa {

// A registry of the CGWindowIDs for the remote cocoa windows created in any
// process (the browser and also the renderer). This can be used to look up
// the compositing frame sink id for efficient video capture.
class REMOTE_COCOA_BROWSER_EXPORT ScopedCGWindowID {
 public:
  ScopedCGWindowID(uint32_t cg_window_id,
                   const viz::FrameSinkId& frame_sink_id);
  ~ScopedCGWindowID();
  ScopedCGWindowID(const ScopedCGWindowID&) = delete;
  ScopedCGWindowID& operator=(const ScopedCGWindowID&) = delete;

  // Query the frame sink id for this window, which can be used for optimized
  // video capture.
  const viz::FrameSinkId& GetFrameSinkId() const { return frame_sink_id_; }

  // Look up this structure corresponding to a given CGWindowID.
  static ScopedCGWindowID* Get(uint32_t cg_window_id);

 private:
  const uint32_t cg_window_id_;
  const viz::FrameSinkId frame_sink_id_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_BROWSER_SCOPED_CG_WINDOW_ID_H_
