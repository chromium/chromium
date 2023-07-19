// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/frame_evictor.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "build/buildflag.h"
#include "components/viz/common/features.h"

namespace viz {

FrameEvictor::FrameEvictor(FrameEvictorClient* client) : client_(client) {}

FrameEvictor::~FrameEvictor() {
  OnSurfaceDiscarded();
}

void FrameEvictor::OnNewSurfaceEmbedded() {
  has_surface_ = true;
  FrameEvictionManager::GetInstance()->AddFrame(this, visible_);
}

void FrameEvictor::OnSurfaceDiscarded() {
  FrameEvictionManager::GetInstance()->RemoveFrame(this);
  has_surface_ = false;
}

void FrameEvictor::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  if (has_surface_) {
    if (visible)
      FrameEvictionManager::GetInstance()->LockFrame(this);
    else
      FrameEvictionManager::GetInstance()->UnlockFrame(this);
  }
}

std::vector<SurfaceId> FrameEvictor::CollectSurfaceIdsForEviction() const {
  std::vector<SurfaceId> surface_ids = {
      client_->CollectSurfaceIdsForEviction()};
  auto current = client_->GetCurrentSurfaceId();
  DCHECK(surface_ids.empty() || !current.is_valid() ||
         base::Contains(surface_ids, current));

  // TODO(crbug.com/1416884): Once we've confirmed the memory impact, remove
  // this feature and always add the current SurfaceId. Android was always only
  // adding the current one before the feature was added.
  bool add_current = base::FeatureList::IsEnabled(features::kEvictSubtree);
#if BUILDFLAG(IS_ANDROID)
  add_current = true;
#endif
  if (surface_ids.empty() && add_current && current.is_valid()) {
    surface_ids.push_back(current);
  }

  auto pre_nav_surface_id = client_->GetPreNavigationSurfaceId();
  if (pre_nav_surface_id.is_valid()) {
    surface_ids.push_back(pre_nav_surface_id);
  }

  return surface_ids;
}

void FrameEvictor::EvictCurrentFrame() {
  client_->EvictDelegatedFrame(CollectSurfaceIdsForEviction());
}

}  // namespace viz
