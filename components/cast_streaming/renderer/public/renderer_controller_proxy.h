// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RENDERER_CONTROLLER_PROXY_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RENDERER_CONTROLLER_PROXY_H_

#include "base/callback.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace cast_streaming {

// This class acts as a hub through which mojom::RendererController calls may be
// received from the browser process and then be dispatched to elsewhere in the
// renderer process. This eliminates any timing concerns associated with
// initialization of the intended receiver for this mojo pipe and instantiation
// of the source in the browser thread. Without this class, there is no
// deterministic way to bind the PendingAssociatedReceiver received from the
// Browser thread to the caller of the GetReceiver() method, if this call were
// to happen after the browser interface is bound.
//
// This class is intended to be a singleton, owned by the ContentRendererClient.
// For this reason, this class is assumed to outlive all RenderFrames with which
// it interacts.
class RendererControllerProxy {
 public:
  static RendererControllerProxy* GetInstance();

  virtual ~RendererControllerProxy();

  // Captures the receiving end of a mojom::RendererController sent to a
  // specific |frame|.
  virtual base::RepeatingCallback<
      void(mojo::PendingAssociatedReceiver<mojom::RendererController>)>
  GetBinder(content::RenderFrame* frame) = 0;

  // Gets the receiver associated with a given |frame|.
  virtual mojo::PendingReceiver<media::mojom::Renderer> GetReceiver(
      content::RenderFrame* frame) = 0;

 protected:
  RendererControllerProxy();

 private:
  static RendererControllerProxy* singleton_instance_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RENDERER_CONTROLLER_PROXY_H_
