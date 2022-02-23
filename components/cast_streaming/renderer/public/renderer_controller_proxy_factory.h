// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RENDERER_CONTROLLER_PROXY_FACTORY_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RENDERER_CONTROLLER_PROXY_FACTORY_H_

#include <memory>

namespace cast_streaming {

class RendererControllerProxy;

// Creates a new RendererControllerProxy. May only be called once.
std::unique_ptr<RendererControllerProxy> CreateRendererControllerProxy();

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RENDERER_CONTROLLER_PROXY_FACTORY_H_
