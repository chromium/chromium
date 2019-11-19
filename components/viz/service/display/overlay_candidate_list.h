// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_LIST_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_LIST_H_

#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/transform.h"

namespace viz {
// This class defines a list of overlay candidates with extra information about
// Android Classic video overlay information.
// TODO(weiliangc): Do not inherit std::vector.
class VIZ_SERVICE_EXPORT OverlayCandidateList
    : public std::vector<OverlayCandidate> {
 public:
  OverlayCandidateList();
  OverlayCandidateList(const OverlayCandidateList&);
  OverlayCandidateList(OverlayCandidateList&&);
  ~OverlayCandidateList();

  OverlayCandidateList& operator=(const OverlayCandidateList&);
  OverlayCandidateList& operator=(OverlayCandidateList&&);

  // [id] == candidate's |display_rect| for all promotable resources.
  using PromotionHintInfoMap = std::map<ResourceId, gfx::RectF>;

  // For android, this provides a set of resources that could be promoted to
  // overlay, if one backs them with a SurfaceView.
  PromotionHintInfoMap promotion_hint_info_map_;

  // Set of resources that have requested a promotion hint that also have quads
  // that use them.
  ResourceIdSet promotion_hint_requestor_set_;
  // base::flat_set<DisplayResourceProvider::ScopedReadLockSharedImage>
  //     promotion_requestors_shared_image_;

  // Helper to insert |candidate| into |promotion_hint_info_|.
  void AddPromotionHint(const OverlayCandidate& candidate);

  // Add |quad| to |promotion_hint_requestors_| if it is requesting a hint.
  void AddToPromotionHintRequestorSetIfNeeded(
      const DisplayResourceProvider* resource_provider,
      const DrawQuad* quad);

  std::vector<DisplayResourceProvider::ScopedReadLockSharedImage>
  ConvertLocalPromotionToMailboxKeyed(
      DisplayResourceProvider* resource_provider,
      base::flat_set<gpu::Mailbox>* promotion_denied,
      base::flat_map<gpu::Mailbox, gfx::Rect>* possible_promotions);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_LIST_H_
