// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_RENDERER_PUBLIC_CONTENT_RENDERER_CLIENT_MIXINS_H_
#define COMPONENTS_CAST_RECEIVER_RENDERER_PUBLIC_CONTENT_RENDERER_CLIENT_MIXINS_H_

#include "base/functional/callback_forward.h"

namespace content {
class RenderFrame;
}

namespace cast_receiver {

// Functions to provide additional ContentRendererClient functionality as
// required for a functioning Cast receiver.
//
// TODO(crbug.com/1359580): Use this class in the
// CastRuntimeContentRendererClient.
class ContentRendererClientMixins {
  // To be called by the ContentRendererClient method of the same name.
  void RenderFrameCreated(content::RenderFrame* render_frame);
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      base::OnceClosure closure);
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_RENDERER_PUBLIC_CONTENT_RENDERER_CLIENT_MIXINS_H_
