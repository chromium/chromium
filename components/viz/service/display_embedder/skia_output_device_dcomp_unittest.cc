// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/dc_layer_overlay_image.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/presenter.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_surface_stub.h"

using ::testing::_;
using ::testing::Invoke;

namespace viz {
namespace {

class MockPresenter : public gl::Presenter {
 public:
  MockPresenter() = default;

  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
    std::move(presentation_callback).Run({});
  }

  bool ScheduleOverlayPlane(
      gl::OverlayImage image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override {
    return true;
  }

  MOCK_METHOD1(ScheduleDCLayers, void(std::vector<gl::DCLayerOverlayParams>));

  bool SupportsDelegatedInk() override { return false; }
  HWND GetWindow() const override { return nullptr; }
  bool DestroyDCLayerTree() override { return true; }

 protected:
  ~MockPresenter() override = default;
};

class TestSkiaOutputDeviceDComp : public SkiaOutputDeviceDComp {
 public:
  TestSkiaOutputDeviceDComp(scoped_refptr<gl::Presenter> presenter,
                            gpu::SharedContextState* context_state)
      : SkiaOutputDeviceDComp(
            /*shared_image_representation_factory=*/nullptr,
            context_state,
            std::move(presenter),
            /*workarounds=*/gpu::GpuDriverBugWorkarounds(),
            /*memory_tracker=*/nullptr,
            /*did_swap_buffer_complete_callback=*/base::DoNothing()) {}

  void SetOverlayImageSize(const gpu::Mailbox& mailbox, const gfx::Size& size) {
    overlay_sizes_[mailbox] = size;
  }

 protected:
  std::optional<gl::DCLayerOverlayImage> BeginOverlayAccess(
      const gpu::Mailbox& mailbox) override {
    auto it = overlay_sizes_.find(mailbox);
    if (it != overlay_sizes_.end()) {
      return gl::DCLayerOverlayImage(it->second,
                                     Microsoft::WRL::ComPtr<IUnknown>());
    }
    return std::nullopt;
  }

 private:
  base::flat_map<gpu::Mailbox, gfx::Size> overlay_sizes_;
};

class SkiaOutputDeviceDCompTest : public testing::Test {
 public:
  void SetUp() override {
    auto presenter = base::MakeRefCounted<MockPresenter>();
    presenter_ = presenter.get();

    auto surface = base::MakeRefCounted<gl::GLSurfaceStub>();
    auto context = base::MakeRefCounted<gl::GLContextStub>();
    context->Initialize(surface.get(), gl::GLContextAttribs());
    context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
        /*share_group=*/nullptr,
        surface,
        context,
        /*use_virtualized_gl_contexts=*/false,
        /*context_lost_callback=*/base::DoNothing(),
        gpu::GrContextType::kGraphiteDawn);

    output_device_ = std::make_unique<TestSkiaOutputDeviceDComp>(
        std::move(presenter), context_state_.get());
  }

  void TearDown() override {
    presenter_ = nullptr;
    output_device_.reset();
    context_state_.reset();
  }

 protected:
  scoped_refptr<gpu::SharedContextState> context_state_;
  std::unique_ptr<TestSkiaOutputDeviceDComp> output_device_;
  raw_ptr<MockPresenter> presenter_ = nullptr;
};

TEST_F(SkiaOutputDeviceDCompTest, ClampsOutOfBoundsUVRect) {
  gfx::Size image_size(100, 100);
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  output_device_->SetOverlayImageSize(mailbox, image_size);

  OverlayCandidate candidate;
  candidate.mailbox = mailbox;
  candidate.display_rect = gfx::RectF(0, 0, 100, 100);
  candidate.uv_rect = gfx::RectF(-0.1f, -0.1f, 1.2f, 1.2f);  // OOB
  candidate.resource_size_in_pixels = image_size;
  candidate.transform = gfx::Transform();

  SkiaOutputSurface::OverlayList overlays;
  overlays.push_back(candidate);

  EXPECT_CALL(*presenter_, ScheduleDCLayers(_))
      .WillOnce([](std::vector<gl::DCLayerOverlayParams> params) {
        ASSERT_EQ(params.size(), 1u);
        // The uv_rect is [-0.1, -0.1, 1.2, 1.2].
        // Content rect in pixels would be [-10, -10, 120, 120].
        // Clamped content rect should be [0, 0, 100, 100].
        EXPECT_EQ(params[0].content_rect, gfx::RectF(0, 0, 100, 100));
      });

  output_device_->ScheduleOverlays(std::move(overlays));
}

TEST_F(SkiaOutputDeviceDCompTest, ClampsOutOfBoundsResourceSize) {
  gfx::Size image_size(100, 100);
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  output_device_->SetOverlayImageSize(mailbox, image_size);

  OverlayCandidate candidate;
  candidate.mailbox = mailbox;
  candidate.display_rect = gfx::RectF(0, 0, 100, 100);
  candidate.uv_rect = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);
  candidate.resource_size_in_pixels = gfx::Size(150, 150);
  candidate.transform = gfx::Transform();

  SkiaOutputSurface::OverlayList overlays;
  overlays.push_back(candidate);

  EXPECT_CALL(*presenter_, ScheduleDCLayers(_))
      .WillOnce([](std::vector<gl::DCLayerOverlayParams> params) {
        ASSERT_EQ(params.size(), 1u);
        // ScaleRect gives [0, 0, 150, 150].
        // Clamped to image size [100, 100] gives [0, 0, 100, 100].
        EXPECT_EQ(params[0].content_rect, gfx::RectF(0, 0, 100, 100));
      });

  output_device_->ScheduleOverlays(std::move(overlays));
}

}  // namespace
}  // namespace viz
