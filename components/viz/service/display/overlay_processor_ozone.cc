// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_ozone.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
  ozone_candidate->crop_rect = primary_plane.uv_rect;
  ozone_candidate->clip_rect.reset();
  ozone_candidate->is_opaque = !primary_plane.enable_blending;
  ozone_candidate->opacity = primary_plane.opacity;
  ozone_candidate->plane_z_order = 0;
  ozone_candidate->buffer_size = primary_plane.resource_size;
  ozone_candidate->priority_hint = primary_plane.priority_hint;
  ozone_candidate->rounded_corners = primary_plane.rounded_corners;
}

void ConvertToOzoneOverlaySurface(
    const OverlayCandidate& overlay_candidate,
    ui::OverlaySurfaceCandidate* ozone_candidate) {
  ozone_candidate->transform = overlay_candidate.transform;
  ozone_candidate->format = overlay_candidate.format;
  ozone_candidate->display_rect = overlay_candidate.display_rect;
  ozone_candidate->crop_rect = overlay_candidate.uv_rect;
  ozone_candidate->clip_rect = overlay_candidate.clip_rect;
  ozone_candidate->is_opaque = overlay_candidate.is_opaque;
  ozone_candidate->opacity = overlay_candidate.opacity;
  ozone_candidate->plane_z_order = overlay_candidate.plane_z_order;
  ozone_candidate->buffer_size = overlay_candidate.resource_size_in_pixels;
  ozone_candidate->requires_overlay = overlay_candidate.requires_overlay;
  ozone_candidate->priority_hint = overlay_candidate.priority_hint;
  ozone_candidate->rounded_corners = overlay_candidate.rounded_corners;
  // That can be a solid color quad.
  if (!overlay_candidate.is_solid_color)
    ozone_candidate->background_color = overlay_candidate.color;
}

uint32_t MailboxToUInt32(const gpu::Mailbox& mailbox) {
  return (mailbox.name[0] << 24) + (mailbox.name[1] << 16) +
         (mailbox.name[2] << 8) + mailbox.name[3];
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool AllowColorSpaceCombination(
    const gfx::ColorSpace& source_color_space,
    const gfx::ColorSpace& destination_color_space) {
  // Allow invalid source color spaces because the assumption is that the
  // compositor won't do a color space conversion in this case anyway, so it
  // should be consistent with the overlay path.
  if (!source_color_space.IsValid())
    return true;

  // Allow color space mismatches as long as either a) the source color space is
  // SRGB; or b) both the source and destination color spaces have the same
  // color usage. It is possible that case (a) still allows for visible color
  // inconsistency between overlays and composition, but we'll address that case
  // if it comes up.
  return source_color_space.GetContentColorUsage() ==
             gfx::ContentColorUsage::kSRGB ||
         source_color_space.GetContentColorUsage() ==
             destination_color_space.GetContentColorUsage();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

void OverlayProcessorOzone::CheckOverlaySupportImpl(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* surfaces) {
  MaybeObserveHardwareCapabilities();

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
#if !BUILDFLAG(IS_FUCHSIA)
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // On Chrome OS, skip the candidate if we think a color space combination
      // might cause visible color differences between compositing and overlays.
      // The reason is that on Chrome OS, we don't yet have an API to set up
      // color space conversion per plane. Note however that we should only do
      // this if the candidate does not require an overlay (e.g., for protected
      // content, it's better to display it with an incorrect color space than
      // to not display it at all).
      // TODO(b/181974042): plumb the color space all the way to the ozone DRM
      // backend when we get an API for per-plane color management.
      if (!surface_iterator->requires_overlay &&
          !AllowColorSpaceCombination(
              /*source_color_space=*/surface_iterator->color_space,
              /*destination_color_space=*/primary_plane_color_space_)) {
        *ozone_surface_iterator = ui::OverlaySurfaceCandidate();
        ozone_surface_iterator->plane_z_order = surface_iterator->plane_z_order;
        continue;
      }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      if (shared_image_interface_) {
        bool result = SetNativePixmapForCandidate(&(*ozone_surface_iterator),
                                                  surface_iterator->mailbox,
                                                  /*is_primary=*/false);
#if BUILDFLAG(IS_CHROMEOS_ASH)
        if (!result && surface_iterator->requires_overlay) {
          // For ChromeOS HW protected content, there's a race condition that
          // can occur here where the mailbox for the native pixmap isn't
          // registered yet so we will fail to promote to overlay due to this
          // check. Allow us to proceed even w/out the native pixmap in that
          // case as it will still succeed and would otherwise cause black
          // flashing between frames while the race condition is completing.
          result = true;
          DLOG(WARNING) << "Allowing required overlay with missing pixmap";
        }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

void OverlayProcessorOzone::MaybeObserveHardwareCapabilities() {
  if (tried_observing_hardware_capabilities_) {
    return;
  }
  tried_observing_hardware_capabilities_ = true;

  // HardwareCapabilities isn't necessary unless attempting multiple overlays.
  if (max_overlays_config_ <= 1) {
    return;
  }
  if (overlay_candidates_) {
    overlay_candidates_->ObserveHardwareCapabilities(
        base::BindRepeating(&OverlayProcessorOzone::ReceiveHardwareCapabilities,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void OverlayProcessorOzone::ReceiveHardwareCapabilities(
    ui::HardwareCapabilities hardware_capabilities) {
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.Display.OverlayProcessorOzone.HardwareCapabilitiesIsValid",
      hardware_capabilities.is_valid);
  if (hardware_capabilities.is_valid) {
    // Subtract 1 because one of these overlay capable planes will be needed for
    // the primary plane.
    int max_overlays_supported =
        hardware_capabilities.num_overlay_capable_planes - 1;
    max_overlays_considered_ =
        std::min(max_overlays_supported, max_overlays_config_);

    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.Display.OverlayProcessorOzone.MaxPlanesSupported",
        hardware_capabilities.num_overlay_capable_planes);
  } else {
    // Default to attempting 1 overlay if we get an invalid response.
    max_overlays_considered_ = 1;
  }

  // Different hardware capabilities may mean a different result for a specific
  // combination of overlays, so clear this cache.
  ClearOverlayCombinationCache();
}

gfx::Rect OverlayProcessorOzone::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

void OverlayProcessorOzone::RegisterOverlayRequirement(bool requires_overlay) {
  // This can be null in unit tests.
  if (overlay_candidates_)
    overlay_candidates_->RegisterOverlayRequirement(requires_overlay);
}

bool OverlayProcessorOzone::SetNativePixmapForCandidate(
    ui::OverlaySurfaceCandidate* candidate,
    const gpu::Mailbox& mailbox,
    bool is_primary) {
  DCHECK(shared_image_interface_);

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
    return false;
  }

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
