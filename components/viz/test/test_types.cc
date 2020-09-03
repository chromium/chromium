// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_types.h"

namespace viz {

namespace {

// Provides a test renderer suffix appropriate for |type|.
const char* RendererTypeTestSuffix(RendererType type) {
  switch (type) {
    case RendererType::kGL:
      return "GL";
    case RendererType::kSkiaGL:
      return "SkiaGL";
    case RendererType::kSkiaVk:
      return "SkiaVulkan";
    case RendererType::kSkiaDawn:
      return "SkiaDawn";
    case RendererType::kSoftware:
      return "Software";
  }
}

}  // namespace

void PrintTo(RendererType type, std::ostream* os) {
  *os << RendererTypeTestSuffix(type);
}

}  // namespace viz
