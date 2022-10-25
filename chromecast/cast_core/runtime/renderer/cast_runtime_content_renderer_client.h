// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "chromecast/renderer/cast_content_renderer_client.h"

namespace cast_streaming {
class ResourceProvider;
}

namespace chromecast {

// TODO(crbug.com/1359580): Use code from //components/cast_receiver/renderer
// instead of relying on CastContentRendererClient.
class CastRuntimeContentRendererClient
    : public shell::CastContentRendererClient {
 public:
  CastRuntimeContentRendererClient();
  CastRuntimeContentRendererClient(const CastRuntimeContentRendererClient&) =
      delete;
  CastRuntimeContentRendererClient(CastRuntimeContentRendererClient&&) = delete;
  ~CastRuntimeContentRendererClient() override;

  CastRuntimeContentRendererClient& operator=(
      const CastRuntimeContentRendererClient&) = delete;
  CastRuntimeContentRendererClient& operator=(
      CastRuntimeContentRendererClient&&) = delete;

  // content::ContentRendererClient overrides.
  std::unique_ptr<cast_streaming::ResourceProvider>
  CreateCastStreamingResourceProvider() override;

 private:
  std::unique_ptr<cast_streaming::ResourceProvider>
      cast_streaming_resource_provider_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_CONTENT_RENDERER_CLIENT_H_
