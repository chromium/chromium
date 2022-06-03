// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_CAST_STREAMING_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_CAST_STREAMING_RENDER_FRAME_OBSERVER_H_

#include "base/callback.h"
#include "components/cast_streaming/renderer/cast_streaming_receiver.h"
#include "content/public/renderer/render_frame_observer.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace cast_streaming {

// This class owns the CastStreamingReceiver and ties its lifespan to a
// RenderFrame. Owned by CastStreamingContentRendererClient, this object will be
// destroyed on RenderFrame destruction, triggering the destruction of of the
// CastStreamingReceiver.
class CastStreamingRenderFrameObserver : public content::RenderFrameObserver {
 public:
  // |on_render_frame_deleted_callback| must delete |this|.
  CastStreamingRenderFrameObserver(
      content::RenderFrame* render_frame,
      base::OnceCallback<void(int)> on_render_frame_deleted_callback);
  ~CastStreamingRenderFrameObserver() override;

  CastStreamingRenderFrameObserver(const CastStreamingRenderFrameObserver&) =
      delete;
  CastStreamingRenderFrameObserver& operator=(
      const CastStreamingRenderFrameObserver&) = delete;

  CastStreamingReceiver* cast_streaming_receiver() {
    return &cast_streaming_receiver_;
  }

 private:
  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  CastStreamingReceiver cast_streaming_receiver_;

  base::OnceCallback<void(int)> on_render_frame_deleted_callback_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_CAST_STREAMING_RENDER_FRAME_OBSERVER_H_
