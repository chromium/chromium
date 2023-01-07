// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_DELEGATED_INK_POINT_RENDERER_SKIA_FOR_TEST_H_
#define COMPONENTS_VIZ_TEST_DELEGATED_INK_POINT_RENDERER_SKIA_FOR_TEST_H_

#include <memory>

#include "components/viz/service/display/delegated_ink_point_renderer_skia.h"

namespace viz {

// Test class that just holds on to the most recently received metadata,
// specifically for testing purposes.
class DelegatedInkPointRendererSkiaForTest
    : public DelegatedInkPointRendererSkia {
 public:
  DelegatedInkPointRendererSkiaForTest();
  ~DelegatedInkPointRendererSkiaForTest() override;

  void SetDelegatedInkMetadata(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) override;

  const gfx::DelegatedInkMetadata* last_metadata() const {
    return last_metadata_.get();
  }

 private:
  std::unique_ptr<gfx::DelegatedInkMetadata> last_metadata_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_DELEGATED_INK_POINT_RENDERER_SKIA_FOR_TEST_H_
