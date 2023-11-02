// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"
#include "components/viz/service/transitions/surface_animation_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

#define EXPECT_BETWEEN(lower, value, upper) \
  EXPECT_LT(value, upper);                  \
  EXPECT_GT(value, lower);

constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

std::vector<CompositorFrameTransitionDirective> CreateSaveDirectiveAsVector(
    uint32_t sequence_id) {
  std::vector<CompositorFrameTransitionDirective> result;
  result.emplace_back(sequence_id,
                      CompositorFrameTransitionDirective::Type::kSave);
  return result;
}

std::vector<CompositorFrameTransitionDirective>
CreateAnimateRendererDirectiveAsVector(uint32_t sequence_id) {
  std::vector<CompositorFrameTransitionDirective> result;
  result.emplace_back(
      sequence_id, CompositorFrameTransitionDirective::Type::kAnimateRenderer);
  return result;
}

}  // namespace

class SurfaceAnimationManagerTest : public testing::Test {
 public:
  void SetUp() override {
    surface_manager_ = frame_sink_manager_.surface_manager();
    support_ = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &frame_sink_manager_, kArbitraryFrameSinkId, /*is_root=*/true);

    LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
    surface_id_ = SurfaceId(kArbitraryFrameSinkId, local_surface_id);
    CompositorFrame frame = MakeDefaultCompositorFrame();
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

    manager_.emplace(&shared_bitmap_manager_);
    manager_->SetDirectiveFinishedCallback(base::DoNothing());
  }

  void TearDown() override {
    storage()->ExpireForTesting();
    manager_.reset();
  }

  Surface* surface() {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id_);
    // Can't ASSERT in a non-void function, so just CHECK instead.
    CHECK(surface);
    return surface;
  }

  SurfaceSavedFrameStorage* storage() {
    return manager().GetSurfaceSavedFrameStorageForTesting();
  }

  SurfaceAnimationManager& manager() { return *manager_; }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl frame_sink_manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)};
  raw_ptr<SurfaceManager> surface_manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  SurfaceId surface_id_;

  absl::optional<SurfaceAnimationManager> manager_;
};

TEST_F(SurfaceAnimationManagerTest, SaveTimesOut) {
  manager().ProcessTransitionDirectives(CreateSaveDirectiveAsVector(1),
                                        surface());

  storage()->ExpireForTesting();

  manager().ProcessTransitionDirectives(
      CreateAnimateRendererDirectiveAsVector(2), surface());
}

TEST_F(SurfaceAnimationManagerTest, RepeatedSavesAreOk) {
  uint32_t sequence_id = 1;
  for (int i = 0; i < 200; ++i) {
    manager().ProcessTransitionDirectives(
        CreateSaveDirectiveAsVector(sequence_id), surface());
  }

  storage()->CompleteForTesting();

  manager().ProcessTransitionDirectives(
      CreateAnimateRendererDirectiveAsVector(sequence_id), surface());
}

}  // namespace viz
