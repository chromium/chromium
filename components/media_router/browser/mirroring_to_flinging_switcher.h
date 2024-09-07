// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_TO_FLINGING_SWITCHER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_TO_FLINGING_SWITCHER_H_

#include "content/public/browser/frame_tree_node_id.h"

namespace media_router {

// Switch mirroring session to a flinging session if the site (e.g. WebContents
// associated with the given `frame_tree_node_id`) has a
// DefaultPresentationRequest (e.g. uses the Cast Web Sender).
// Must be called on the UI thread.
void SwitchToFlingingIfPossible(content::FrameTreeNodeId frame_tree_node_id);

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MIRRORING_TO_FLINGING_SWITCHER_H_
