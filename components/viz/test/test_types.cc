// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_types.h"

#include "components/viz/test/buildflags.h"

namespace viz {

namespace {

// Provides a test renderer suffix appropriate for |type|.
const char* RendererTypeTestSuffix(RendererType type) {
  switch (type) {
    case RendererType::kSkiaGL:
      return "SkiaGL";
    case RendererType::kSkiaVk:
      return "SkiaVulkan";
    case RendererType::kSkiaGraphite:
      return "SkiaGraphite";
    case RendererType::kSoftware:
      return "Software";
  }
}

std::vector<RendererType> GetRendererTypes(bool include_software,
                                           bool skia_only) {
  std::vector<RendererType> types;
  if (include_software && !skia_only)
    types.push_back(RendererType::kSoftware);
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
  types.push_back(RendererType::kSkiaGL);
#endif
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
  types.push_back(RendererType::kSkiaVk);
#endif
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
  types.push_back(RendererType::kSkiaGraphite);
#endif
  return types;
}

}  // namespace

void PrintTo(RendererType type, std::ostream* os) {
  *os << RendererTypeTestSuffix(type);
}

std::vector<RendererType> GetRendererTypes() {
  return GetRendererTypes(true, false);
}

std::vector<RendererType> GetGpuRendererTypes() {
  return GetRendererTypes(false, false);
}

std::vector<RendererType> GetRendererTypesSkiaOnly() {
  return GetRendererTypes(false, true);
}

}  // namespace viz
