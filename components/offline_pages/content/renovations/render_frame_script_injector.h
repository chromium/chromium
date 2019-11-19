// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CONTENT_RENOVATIONS_RENDER_FRAME_SCRIPT_INJECTOR_H_
#define COMPONENTS_OFFLINE_PAGES_CONTENT_RENOVATIONS_RENDER_FRAME_SCRIPT_INJECTOR_H_

#include "components/offline_pages/core/renovations/script_injector.h"

namespace content {
class RenderFrameHost;
}

namespace offline_pages {

// ScriptInjector for running scripts in a RenderFrame within a given isolated
// world.
class RenderFrameScriptInjector : public ScriptInjector {
 public:
  ~RenderFrameScriptInjector() override = default;

  // The |render_frame_host| is expected to outlive this
  // RenderFrameScriptInjector instance.
  RenderFrameScriptInjector(content::RenderFrameHost* render_frame_host,
                            int32_t isolated_world_id);

  // ScriptInjector implementation.
  void Inject(base::string16 script, ResultCallback callback) override;

 private:
  content::RenderFrameHost* render_frame_host_;
  int32_t isolated_world_id_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CONTENT_RENOVATIONS_RENDER_FRAME_SCRIPT_INJECTOR_H_
