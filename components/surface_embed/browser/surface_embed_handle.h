// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HANDLE_H_
#define COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HANDLE_H_

#include "components/guest_contents/browser/guest_contents_handle.h"

namespace surface_embed {

// TODO(crbug.com/565065586): Move the implementation of GuestContentsHandle
// here and delete GuestContentsHandle.
using SurfaceEmbedHandle = guest_contents::GuestContentsHandle;

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HANDLE_H_
