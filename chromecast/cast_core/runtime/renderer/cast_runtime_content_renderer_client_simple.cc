// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/renderer/cast_runtime_content_renderer_client.h"

#include <memory>

#include "chromecast/renderer/cast_content_renderer_client.h"

namespace chromecast {

std::unique_ptr<shell::CastContentRendererClient>
shell::CastContentRendererClient::Create() {
  return std::make_unique<CastRuntimeContentRendererClient>();
}

}  // namespace chromecast
