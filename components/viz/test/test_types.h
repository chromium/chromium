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
  kSkiaGraphiteDawn,
  kSkiaGraphiteMetal,
  kSoftware,
};

void PrintTo(RendererType type, std::ostream* os);

// Returns a list containing all RendererTypes applicable to the platform.
std::vector<RendererType> GetRendererTypes();

// Returns a list containing all RendererTypes, except SoftwareRenderer,
// applicable to the platform.
std::vector<RendererType> GetGpuRendererTypes();

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_TYPES_H_
