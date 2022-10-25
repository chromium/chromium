// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/renderer/public/content_renderer_client_mixins.h"

#include "components/on_load_script_injector/renderer/on_load_script_injector.h"

namespace cast_receiver {

void ContentRendererClientMixins::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Add script injection support to the RenderFrame, used for bindings support
  // APIs. The injector's lifetime is bound to the RenderFrame's lifetime.
  new on_load_script_injector::OnLoadScriptInjector(render_frame);
}

}  // namespace cast_receiver
