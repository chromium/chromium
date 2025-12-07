// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OZONE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OZONE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/ozone/public/hardware_capabilities.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace viz {

class VIZ_SERVICE_EXPORT OverlayProcessorOzone
    : public OverlayProcessorUsingStrategy {
 public:
  class PixmapProvider {
   public:
    virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap(
        const gpu::Mailbox& mailbox) = 0;
    virtual ~PixmapProvider();
  };

  OverlayProcessorOzone(
      std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
      std::vector<OverlayStrategy> available_strategies,
      std::unique_ptr<PixmapProvider> pixmap_provider);
  ~OverlayProcessorOzone() override;

  bool IsOverlaySupported() const override;

  bool NeedsSurfaceDamageRectList() const override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetViewportSize(const gfx::Size& size) override {}
  bool SupportsFlipRotateTransform() const override;
  void NotifyOverlayPromotion(
      DisplayResourceProvider* display_resource_provider,
      const OverlayCandidateList& candidate_list,
      const QuadList& quad_list) override;

  void CheckOverlaySupportImpl(
      const std::optional<OverlayCandidate>& primary_plane,
      OverlayCandidateList* surfaces) override;
  // If UseMultipleOverlays is enabled, set `ReceiveHardwareCapabilities` as a
  // callback on `overlay_candidates_` to be called with a
  // `HardwareCapabilities` once, and then each time displays are configured.
  void MaybeObserveHardwareCapabilities();
  // Updates `max_overlays_considered_` based on the overlay plane support in
  // the received `HardwareCapabilities`.
  void ReceiveHardwareCapabilities(
      ui::HardwareCapabilities hardware_capabilities);

  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& candidate) const override;
  void RegisterOverlayRequirement(bool requires_overlay) override;

  // Forwards this message to the OverlayCandidates, which can react to swap
  // result accordingly.
  void OnSwapBuffersComplete(gfx::SwapResult swap_result) override;

 protected:
  void InsertPrimaryPlane(OverlayCandidate primary_plane,
                          OverlayCandidateList& candidates) override;

 private:
  // Populates |native_pixmap| and |native_pixmap_unique_id| in |candidate|
  // based on |mailbox|. |is_primary| should be true if this is the primary
  // surface. Return false if the corresponding NativePixmap cannot be found.
  bool SetNativePixmapForCandidate(ui::OverlaySurfaceCandidate* candidate,
                                   const gpu::Mailbox& mailbox,
                                   bool is_primary);

  bool tried_observing_hardware_capabilities_ = false;
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates_;
  const std::vector<OverlayStrategy> available_strategies_;
  bool has_independent_cursor_plane_ = true;
  std::unique_ptr<PixmapProvider> pixmap_provider_;

  base::WeakPtrFactory<OverlayProcessorOzone> weak_ptr_factory_{this};
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_OZONE_H_
