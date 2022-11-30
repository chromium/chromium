// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_FACTORY_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_FACTORY_H_

#include <memory>

namespace cast_streaming {

class ResourceProvider;

std::unique_ptr<ResourceProvider> CreateResourceProvider();

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_RESOURCE_PROVIDER_FACTORY_H_
