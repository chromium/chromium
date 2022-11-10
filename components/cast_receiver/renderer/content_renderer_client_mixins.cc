// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/renderer/public/content_renderer_client_mixins.h"

#include "components/media_control/renderer/media_playback_options.h"
#include "components/on_load_script_injector/renderer/on_load_script_injector.h"

namespace cast_receiver {

void ContentRendererClientMixins::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Add script injection support to the RenderFrame, used for bindings support
  // APIs. The injector's lifetime is bound to the RenderFrame's lifetime.
  new on_load_script_injector::OnLoadScriptInjector(render_frame);

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new media_control::MediaPlaybackOptions(render_frame);
}

bool ContentRendererClientMixins::DeferMediaLoad(
    content::RenderFrame* render_frame,
    base::OnceClosure closure) {
  auto* playback_options =
      media_control::MediaPlaybackOptions::Get(render_frame);
  DCHECK(playback_options);
  return playback_options->RunWhenInForeground(std::move(closure));
}

}  // namespace cast_receiver
