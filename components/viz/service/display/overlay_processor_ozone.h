// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OZONE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OZONE_H_

#include <memory>
#include <vector>

#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace viz {

class VIZ_SERVICE_EXPORT OverlayProcessorOzone
    : public OverlayProcessorUsingStrategy {
 public:
  OverlayProcessorOzone(
      std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
      std::vector<OverlayStrategy> available_strategies,
      gpu::SharedImageInterface* shared_image_interface);
  ~OverlayProcessorOzone() override;

  bool IsOverlaySupported() const override;

  bool NeedsSurfaceDamageRectList() const override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetViewportSize(const gfx::Size& size) override {}

  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* surfaces) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& candidate) const override;

 private:
  // Populates |native_pixmap| and |native_pixmap_unique_id| in |candidate|
  // based on |mailbox|. |is_primary| should be true if this is the primary
  // surface. Return false if the corresponding NativePixmap cannot be found.
  bool SetNativePixmapForCandidate(ui::OverlaySurfaceCandidate* candidate,
                                   const gpu::Mailbox& mailbox,
                                   bool is_primary);

  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates_;
  const std::vector<OverlayStrategy> available_strategies_;
  gpu::SharedImageInterface* const shared_image_interface_;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OZONE_H_
