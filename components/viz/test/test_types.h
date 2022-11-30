// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_TYPES_H_
#define COMPONENTS_VIZ_TEST_TEST_TYPES_H_

#include <ostream>
#include <vector>

namespace viz {

enum class RendererType {
  kSkiaGL,
  kSkiaVk,
  // SkiaRenderer with the Dawn backend will be used; on Linux this will
  // initialize Vulkan, and on Windows this will initialize D3D12.
  kSkiaDawn,
  kSoftware,
};

void PrintTo(RendererType type, std::ostream* os);

// Returns a list containing all RendererTypes applicable to the platform.
std::vector<RendererType> GetRendererTypes();
std::vector<RendererType> GetRendererTypesNoDawn();

// Returns a list containing all RendererTypes, except SoftwareRenderer,
// applicable to the platform.
std::vector<RendererType> GetGpuRendererTypes();
std::vector<RendererType> GetGpuRendererTypesNoDawn();

// Returns a list containing all Skia RendererTypes applicable to the platform.
std::vector<RendererType> GetRendererTypesSkiaOnly();

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_TYPES_H_
