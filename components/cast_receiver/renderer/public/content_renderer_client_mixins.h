// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_RENDERER_PUBLIC_CONTENT_RENDERER_CLIENT_MIXINS_H_
#define COMPONENTS_CAST_RECEIVER_RENDERER_PUBLIC_CONTENT_RENDERER_CLIENT_MIXINS_H_

namespace content {
class RenderFrame;
}

namespace cast_receiver {

// Functions to provide additional ContentRendererClient functionality as
// required for a functioning Cast receiver.
class ContentRendererClientMixins {
  // To be called by the ContentRendererClient method of the same name.
  void RenderFrameCreated(content::RenderFrame* render_frame);
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_RENDERER_PUBLIC_CONTENT_RENDERER_CLIENT_MIXINS_H_
