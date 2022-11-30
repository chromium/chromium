// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_stub.h"

namespace viz {
bool OverlayProcessorStub::IsOverlaySupported() const {
  return false;
}
gfx::Rect OverlayProcessorStub::GetAndResetOverlayDamage() {
  return gfx::Rect();
}
gfx::Rect OverlayProcessorStub::GetPreviousFrameOverlaysBoundingRect() const {
  return gfx::Rect();
}

bool OverlayProcessorStub::NeedsSurfaceDamageRectList() const {
  return false;
}

gfx::CALayerResult OverlayProcessorStub::GetCALayerErrorCode() const {
  return gfx::kCALayerFailedOverlayDisabled;
}

}  // namespace viz
