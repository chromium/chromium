// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/skia_surface_provider_factory.h"

#include "chrome/browser/vr/cmd_buffer_surface_provider.h"

namespace vr {

std::unique_ptr<SkiaSurfaceProvider> SkiaSurfaceProviderFactory::Create() {
  return std::make_unique<CmdBufferSurfaceProvider>();
}

}  // namespace vr
