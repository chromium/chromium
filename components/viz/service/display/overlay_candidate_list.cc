// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_list.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/video_types.h"

namespace viz {

OverlayCandidateList::OverlayCandidateList() = default;

OverlayCandidateList::OverlayCandidateList(const OverlayCandidateList& other) =
    default;

OverlayCandidateList::OverlayCandidateList(OverlayCandidateList&& other) =
    default;

OverlayCandidateList::~OverlayCandidateList() = default;

OverlayCandidateList& OverlayCandidateList::operator=(
    const OverlayCandidateList& other) = default;

OverlayCandidateList& OverlayCandidateList::operator=(
    OverlayCandidateList&& other) = default;

void OverlayCandidateList::AddPromotionHint(const OverlayCandidate& candidate) {
  promotion_hint_info_map_[candidate.resource_id] = candidate.display_rect;
}

void OverlayCandidateList::AddToPromotionHintRequestorSetIfNeeded(
    const DisplayResourceProvider* resource_provider,
    const DrawQuad* quad) {
  if (quad->material != DrawQuad::Material::kStreamVideoContent)
    return;
  ResourceId id = StreamVideoDrawQuad::MaterialCast(quad)->resource_id();
  if (!resource_provider->DoesResourceWantPromotionHint(id))
    return;
  promotion_hint_requestor_set_.insert(id);
}

std::vector<DisplayResourceProvider::ScopedReadLockSharedImage>
OverlayCandidateList::ConvertLocalPromotionToMailboxKeyed(
    DisplayResourceProvider* resource_provider,
    base::flat_set<gpu::Mailbox>* promotion_denied,
    base::flat_map<gpu::Mailbox, gfx::Rect>* possible_promotions) {
  DCHECK(empty() || size() == 1u);
  std::vector<DisplayResourceProvider::ScopedReadLockSharedImage> locks;
  for (auto& request : promotion_hint_requestor_set_) {
    // If we successfully promote one candidate, then that promotion hint should
    // be sent later when we schedule the overlay.
    if (!empty() && front().resource_id == request)
      continue;

    locks.emplace_back(resource_provider, request);
    auto iter = promotion_hint_info_map_.find(request);
    if (iter != promotion_hint_info_map_.end()) {
      // This is a possible promotion.
      possible_promotions->emplace(locks.back().mailbox(),
                                   gfx::ToEnclosedRect(iter->second));
    } else {
      promotion_denied->insert(locks.back().mailbox());
    }
  }
  return locks;
}
}  // namespace viz
