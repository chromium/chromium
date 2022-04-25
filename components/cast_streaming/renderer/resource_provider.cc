// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/public/resource_provider.h"

namespace cast_streaming {
namespace {

ResourceProvider* g_instance = nullptr;

}  // namespace

// static
mojo::PendingReceiver<media::mojom::Renderer> ResourceProvider::GetReceiver(
    content::RenderFrame* render_frame) {
  DCHECK(g_instance);
  return g_instance->GetReceiverImpl(render_frame);
}

ResourceProvider::ResourceProvider() {
  DCHECK(!g_instance);
  g_instance = this;
}

ResourceProvider::~ResourceProvider() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

}  // namespace cast_streaming
