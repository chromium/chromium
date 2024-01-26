// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <vector>

#include "base/command_line.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "components/viz/service/hit_test/hit_test_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_latest_local_surface_id_lookup_delegate.h"
#include "ui/gfx/geometry/test/fuzzer_util.h"

namespace {

constexpr uint32_t kMaxDepthAllowed = 255;

uint32_t GetNextUInt32NonZero(FuzzedDataProvider* fuzz) {
  return fuzz->ConsumeIntegralInRange<uint32_t>(
      1, std::numeric_limits<uint32_t>::max());
}

void SubmitHitTestRegionList(
    FuzzedDataProvider* fuzz,
    viz::TestLatestLocalSurfaceIdLookupDelegate* delegate,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    const viz::SurfaceId& surface_id,
    bool support_is_root,
    const uint32_t depth);

void AddHitTestRegion(FuzzedDataProvider* fuzz,
                      std::vector<viz::HitTestRegion>* regions,
                      uint32_t child_count,
                      viz::TestLatestLocalSurfaceIdLookupDelegate* delegate,
                      viz::FrameSinkManagerImpl* frame_sink_manager,
                      const viz::SurfaceId& surface_id,
                      const uint32_t depth) {
  if (!child_count || depth > kMaxDepthAllowed)
    return;

  // If there's not enough space left for a HitTestRegion, then skip.
  if (fuzz->remaining_bytes() < sizeof(viz::HitTestRegion))
    return;

  viz::HitTestRegion hit_test_region;
  hit_test_region.flags = fuzz->ConsumeIntegral<uint32_t>();
  hit_test_region.async_hit_test_reasons = fuzz->ConsumeIntegral<uint32_t>();
  if (fuzz->ConsumeBool())
    hit_test_region.flags |= viz::HitTestRegionFlags::kHitTestChildSurface;
  hit_test_region.frame_sink_id = viz::FrameSinkId(
      fuzz->ConsumeIntegral<uint32_t>(), fuzz->ConsumeIntegral<uint32_t>());
  hit_test_region.rect =
      gfx::Rect(fuzz->ConsumeIntegral<int>(), fuzz->ConsumeIntegral<int>(),
                fuzz->ConsumeIntegral<int>(), fuzz->ConsumeIntegral<int>());
  hit_test_region.transform = gfx::ConsumeTransform(*fuzz);

  if (fuzz->ConsumeBool() &&
      (hit_test_region.flags & viz::HitTestRegionFlags::kHitTestChildSurface)) {
    // If there's not enough space left for a LocalSurfaceId, then skip.
    if (fuzz->remaining_bytes() < sizeof(viz::LocalSurfaceId))
      return;

    uint32_t last_frame_sink_id_client_id =
        surface_id.frame_sink_id().client_id();
    uint32_t last_frame_sink_id_sink_id = surface_id.frame_sink_id().sink_id();
    viz::FrameSinkId frame_sink_id(last_frame_sink_id_client_id + 1,
                                   last_frame_sink_id_sink_id + 1);
    viz::LocalSurfaceId local_surface_id(GetNextUInt32NonZero(fuzz),
                                         GetNextUInt32NonZero(fuzz),
                                         base::UnguessableToken::Create());
    SubmitHitTestRegionList(fuzz, delegate, frame_sink_manager,
                            viz::SurfaceId(frame_sink_id, local_surface_id),
                            false /* support_is_root */, depth + 1);
  }

  regions->push_back(std::move(hit_test_region));
  AddHitTestRegion(fuzz, regions, child_count - 1, delegate, frame_sink_manager,
                   surface_id, depth + 1);
}

void SubmitHitTestRegionList(
    FuzzedDataProvider* fuzz,
    viz::TestLatestLocalSurfaceIdLookupDelegate* delegate,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    const viz::SurfaceId& surface_id,
    bool support_is_root,
    const uint32_t depth) {
  // If there's not enough space left for a HitTestRegionList, then skip.
  if (fuzz->remaining_bytes() < sizeof(viz::HitTestRegionList) + sizeof(bool) ||
      depth > kMaxDepthAllowed) {
    return;
  }

  std::optional<viz::HitTestRegionList> hit_test_region_list;
  if (fuzz->ConsumeBool()) {
    hit_test_region_list.emplace();
    hit_test_region_list->flags = fuzz->ConsumeIntegral<uint32_t>();
    hit_test_region_list->async_hit_test_reasons =
        fuzz->ConsumeIntegral<uint32_t>();
    if (fuzz->ConsumeBool())
      hit_test_region_list->flags |=
          viz::HitTestRegionFlags::kHitTestChildSurface;
    hit_test_region_list->bounds =
        gfx::Rect(fuzz->ConsumeIntegral<int>(), fuzz->ConsumeIntegral<int>(),
                  fuzz->ConsumeIntegral<int>(), fuzz->ConsumeIntegral<int>());
    hit_test_region_list->transform = gfx::ConsumeTransform(*fuzz);

    uint32_t child_count = fuzz->ConsumeIntegral<uint32_t>();
    AddHitTestRegion(fuzz, &hit_test_region_list->regions, child_count,
                     delegate, frame_sink_manager, surface_id, depth + 1);
  }

  delegate->SetSurfaceIdMap(surface_id);
  viz::CompositorFrameSinkSupport support(
      nullptr /* client */, frame_sink_manager, surface_id.frame_sink_id(),
      support_is_root);
  support.SubmitCompositorFrame(surface_id.local_surface_id(),
                                viz::MakeDefaultCompositorFrame(),
                                std::move(hit_test_region_list));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t num_bytes) {
  FuzzedDataProvider fuzz(data, num_bytes);
  viz::ServerSharedBitmapManager shared_bitmap_manager;
  viz::FrameSinkManagerImpl frame_sink_manager{
      viz::FrameSinkManagerImpl::InitParams(&shared_bitmap_manager)};
  viz::TestLatestLocalSurfaceIdLookupDelegate delegate;
  viz::TestLatestLocalSurfaceIdLookupDelegate* lsi_delegate =
      fuzz.ConsumeBool() ? &delegate : nullptr;

  // If there's not enough space left for a LocalSurfaceId, then skip.
  if (fuzz.remaining_bytes() < sizeof(viz::LocalSurfaceId))
    return 0;

  constexpr uint32_t root_client_id = 1;
  constexpr uint32_t root_sink_id = 1;
  viz::FrameSinkId frame_sink_id(root_client_id, root_sink_id);
  viz::LocalSurfaceId local_surface_id(GetNextUInt32NonZero(&fuzz),
                                       GetNextUInt32NonZero(&fuzz),
                                       base::UnguessableToken::Create());
  viz::SurfaceId surface_id(frame_sink_id, local_surface_id);
  viz::HitTestAggregator aggregator(
      frame_sink_manager.hit_test_manager(), &frame_sink_manager, lsi_delegate,
      frame_sink_id, 10 /* initial_region_size */, 100 /* max_region_size */);

  SubmitHitTestRegionList(&fuzz, &delegate, &frame_sink_manager, surface_id,
                          true /* support_is_root */, 0 /* depth */);

  viz::SurfaceId aggregate_surface_id = surface_id;
  if (fuzz.ConsumeBool() && fuzz.remaining_bytes() >= sizeof(viz::SurfaceId)) {
    aggregate_surface_id =
        viz::SurfaceId(viz::FrameSinkId(GetNextUInt32NonZero(&fuzz),
                                        GetNextUInt32NonZero(&fuzz)),
                       viz::LocalSurfaceId(GetNextUInt32NonZero(&fuzz),
                                           GetNextUInt32NonZero(&fuzz),
                                           base::UnguessableToken::Create()));
  }
  aggregator.Aggregate(aggregate_surface_id);
  viz::Surface* surface = frame_sink_manager.surface_manager()->GetSurfaceForId(
      aggregate_surface_id);
  if (surface)
    frame_sink_manager.surface_manager()->SurfaceDestroyed(surface);

  return 0;
}
