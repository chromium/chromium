// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/nacl_helper.h"

#include "content/public/renderer/renderer_ppapi_host.h"

namespace nacl {

NaClHelper::NaClHelper(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

NaClHelper::~NaClHelper() = default;

void NaClHelper::DidCreatePepperPlugin(content::RendererPpapiHost* host) {
  // The Native Client plugin is a host for external plugins.
  if (host->GetPluginName() == "Native Client")
    host->SetToExternalPluginHost();
}

void NaClHelper::OnDestruct() {
  delete this;
}

}  // namespace nacl
