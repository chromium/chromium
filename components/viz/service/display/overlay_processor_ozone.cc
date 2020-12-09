// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_ozone.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

namespace {

// TODO(weiliangc): When difference between primary plane and non-primary plane
// can be internalized, merge these two helper functions.
void ConvertToOzoneOverlaySurface(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& primary_plane,
    ui::OverlaySurfaceCandidate* ozone_candidate) {
  ozone_candidate->transform = primary_plane.transform;
  ozone_candidate->format = primary_plane.format;
  ozone_candidate->display_rect = primary_plane.display_rect;
  ozone_candidate->crop_rect = gfx::RectF(0.f, 0.f, 1.f, 1.f);
  ozone_candidate->clip_rect = gfx::ToEnclosingRect(primary_plane.display_rect);
  ozone_candidate->is_clipped = false;
  ozone_candidate->is_opaque = !primary_plane.enable_blending;
  ozone_candidate->plane_z_order = 0;
  ozone_candidate->buffer_size = primary_plane.resource_size;
}

void ConvertToOzoneOverlaySurface(
    const OverlayCandidate& overlay_candidate,
    ui::OverlaySurfaceCandidate* ozone_candidate) {
  ozone_candidate->transform = overlay_candidate.transform;
  ozone_candidate->format = overlay_candidate.format;
  ozone_candidate->display_rect = overlay_candidate.display_rect;
  ozone_candidate->crop_rect = overlay_candidate.uv_rect;
  ozone_candidate->clip_rect = overlay_candidate.clip_rect;
  ozone_candidate->is_clipped = overlay_candidate.is_clipped;
  ozone_candidate->is_opaque = overlay_candidate.is_opaque;
  ozone_candidate->plane_z_order = overlay_candidate.plane_z_order;
  ozone_candidate->buffer_size = overlay_candidate.resource_size_in_pixels;
  ozone_candidate->requires_overlay = overlay_candidate.requires_overlay;
}

uint32_t MailboxToUInt32(const gpu::Mailbox& mailbox) {
  return (mailbox.name[0] << 24) + (mailbox.name[1] << 16) +
         (mailbox.name[2] << 8) + mailbox.name[3];
}

void ReportSharedImageExists(bool exists) {
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.Display.OverlayProcessorOzone."
      "SharedImageExists",
      exists);
}

}  // namespace

// |overlay_candidates| is an object used to answer questions about possible
// overlays configurations.
// |available_strategies| is a list of overlay strategies that should be
// initialized by InitializeStrategies.
OverlayProcessorOzone::OverlayProcessorOzone(
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
    std::vector<OverlayStrategy> available_strategies,
    gpu::SharedImageInterface* shared_image_interface)
    : OverlayProcessorUsingStrategy(),
      overlay_candidates_(std::move(overlay_candidates)),
      available_strategies_(std::move(available_strategies)),
      shared_image_interface_(shared_image_interface) {
  for (OverlayStrategy strategy : available_strategies_) {
    switch (strategy) {
      case OverlayStrategy::kFullscreen:
        strategies_.push_back(
            std::make_unique<OverlayStrategyFullscreen>(this));
        break;
      case OverlayStrategy::kSingleOnTop:
        strategies_.push_back(
            std::make_unique<OverlayStrategySingleOnTop>(this));
        break;
      case OverlayStrategy::kUnderlay:
        strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
        break;
      case OverlayStrategy::kUnderlayCast:
        strategies_.push_back(
            std::make_unique<OverlayStrategyUnderlayCast>(this));
        break;
      default:
        NOTREACHED();
    }
  }
}

OverlayProcessorOzone::~OverlayProcessorOzone() = default;

bool OverlayProcessorOzone::IsOverlaySupported() const {
  return true;
}

bool OverlayProcessorOzone::NeedsSurfaceDamageRectList() const {
  return true;
}

void OverlayProcessorOzone::CheckOverlaySupport(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* surfaces) {
  // This number is depended on what type of strategies we have. Currently we
  // only overlay one video.
  DCHECK_EQ(1U, surfaces->size());
  auto full_size = surfaces->size();
  if (primary_plane)
    full_size += 1;

  ui::OverlayCandidatesOzone::OverlaySurfaceCandidateList ozone_surface_list(
      full_size);

  // Convert OverlayCandidateList to OzoneSurfaceCandidateList.
  {
    auto ozone_surface_iterator = ozone_surface_list.begin();

    // For ozone-cast, there will not be a primary_plane.
    if (primary_plane) {
      ConvertToOzoneOverlaySurface(*primary_plane, &(*ozone_surface_iterator));
      // TODO(crbug.com/1138568): Fuchsia claims support for presenting primary
      // plane as overlay, but does not provide a mailbox. Handle this case.
#if !defined(OS_FUCHSIA)
      if (shared_image_interface_) {
        bool result = SetNativePixmapForCandidate(&(*ozone_surface_iterator),
                                                  primary_plane->mailbox,
                                                  /*is_primary=*/true);
        // We cannot validate an overlay configuration without the buffer for
        // primary plane present.
        if (!result) {
          for (auto& candidate : *surfaces) {
            candidate.overlay_handled = false;
          }
          return;
        }
      }
#endif
      ozone_surface_iterator++;
    }

    auto surface_iterator = surfaces->cbegin();
    for (; ozone_surface_iterator < ozone_surface_list.end() &&
           surface_iterator < surfaces->cend();
         ozone_surface_iterator++, surface_iterator++) {
      ConvertToOzoneOverlaySurface(*surface_iterator,
                                   &(*ozone_surface_iterator));
      if (shared_image_interface_) {
        bool result = SetNativePixmapForCandidate(&(*ozone_surface_iterator),
                                                  surface_iterator->mailbox,
                                                  /*is_primary=*/false);
        // Skip the candidate if the corresponding NativePixmap is not found.
        if (!result) {
          *ozone_surface_iterator = ui::OverlaySurfaceCandidate();
          ozone_surface_iterator->plane_z_order =
              surface_iterator->plane_z_order;
        }
      }
    }
  }
  overlay_candidates_->CheckOverlaySupport(&ozone_surface_list);

  // Copy information from OzoneSurfaceCandidatelist back to
  // OverlayCandidateList.
  {
    DCHECK_EQ(full_size, ozone_surface_list.size());
    auto ozone_surface_iterator = ozone_surface_list.cbegin();
    // The primary plane is always handled, and don't need to copy information.
    if (primary_plane)
      ozone_surface_iterator++;

    auto surface_iterator = surfaces->begin();
    for (; surface_iterator < surfaces->end() &&
           ozone_surface_iterator < ozone_surface_list.cend();
         surface_iterator++, ozone_surface_iterator++) {
      surface_iterator->overlay_handled =
          ozone_surface_iterator->overlay_handled;
      surface_iterator->display_rect = ozone_surface_iterator->display_rect;
    }
  }
}

gfx::Rect OverlayProcessorOzone::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

bool OverlayProcessorOzone::SetNativePixmapForCandidate(
    ui::OverlaySurfaceCandidate* candidate,
    const gpu::Mailbox& mailbox,
    bool is_primary) {
  DCHECK(shared_image_interface_);

  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.Display.OverlayProcessorOzone."
      "IsCandidateSharedImage",
      mailbox.IsSharedImage());

  if (!mailbox.IsSharedImage())
    return false;

  scoped_refptr<gfx::NativePixmap> native_pixmap =
      shared_image_interface_->GetNativePixmap(mailbox);

  if (!native_pixmap) {
    // SharedImage creation and destruction happens on a different
    // thread so there is no guarantee that we can always look them up
    // successfully. If a SharedImage doesn't exist, ignore the
    // candidate. We will try again next frame.
    DLOG(ERROR) << "Unable to find the NativePixmap corresponding to the "
                   "overlay candidate";
    ReportSharedImageExists(false);
    return false;
  }
  ReportSharedImageExists(true);

  if (is_primary && (candidate->buffer_size != native_pixmap->GetBufferSize() ||
                     candidate->format != native_pixmap->GetBufferFormat())) {
    // If |mailbox| corresponds to the last submitted primary plane, its
    // parameters may not match those of the current candidate due to a
    // reshape. If the size and format don't match, skip this candidate for
    // now, and try again next frame.
    return false;
  }

  candidate->native_pixmap = std::move(native_pixmap);
  candidate->native_pixmap_unique_id = MailboxToUInt32(mailbox);
  return true;
}

}  // namespace viz
