// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_types.h"

#include "build/build_config.h"
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
    case RendererType::kSkiaGraphiteDawn:
      return "SkiaGraphiteDawn";
    case RendererType::kSkiaGraphiteMetal:
      return "SkiaGraphiteMetal";
    case RendererType::kSoftware:
      return "Software";
  }
}

std::vector<RendererType> GetRendererTypes(bool include_software) {
  std::vector<RendererType> types;
  if (include_software) {
    types.push_back(RendererType::kSoftware);
  }
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
  types.push_back(RendererType::kSkiaGL);
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
  types.push_back(RendererType::kSkiaVk);
#endif  // BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
  types.push_back(RendererType::kSkiaGraphiteDawn);
#if BUILDFLAG(IS_IOS)
  types.push_back(RendererType::kSkiaGraphiteMetal);
#endif  // BUILDFLAG(IS_IOS)
#endif  // BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
  return types;
}

}  // namespace

void PrintTo(RendererType type, std::ostream* os) {
  *os << RendererTypeTestSuffix(type);
}

std::vector<RendererType> GetRendererTypes() {
  return GetRendererTypes(true);
}

std::vector<RendererType> GetGpuRendererTypes() {
  return GetRendererTypes(false);
}

}  // namespace viz
