// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/test/test_shared_image_interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

constexpr CompositorRenderPassId kRenderPassId{1};
constexpr gfx::Size kSize(100, 100);

gfx::DisplayColorSpaces CreateWideGamutDisplayColorSpaces() {
  gfx::DisplayColorSpaces display_color_spaces(gfx::ColorSpace::CreateSRGB());
  for (bool needs_alpha : {false, true}) {
    display_color_spaces.SetOutputColorSpaceAndFormat(
        gfx::ContentColorUsage::kWideColorGamut, needs_alpha,
        gfx::ColorSpace::CreateDisplayP3D65(), SinglePlaneFormat::kRGBA_F16);
  }
  return display_color_spaces;
}

CompositorFrameTransitionDirective CreateDirective() {
  CompositorFrameTransitionDirective::SharedElement element;
  element.render_pass_id = kRenderPassId;
  element.view_transition_element_resource_id =
      ViewTransitionElementResourceId(blink::ViewTransitionToken(), 1,
                                      /*for_scope_snapshot=*/false);
  return CompositorFrameTransitionDirective::CreateSave(
      blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/false,
      /*sequence_id=*/1, {element}, CreateWideGamutDisplayColorSpaces(),
      /*delay_layer_tree_view_deletion=*/false);
}

std::unique_ptr<CompositorRenderPass> CreateRenderPass() {
  auto render_pass = CompositorRenderPass::Create();
  render_pass->SetNew(kRenderPassId, gfx::Rect(kSize), gfx::Rect(kSize),
                      gfx::Transform());
  return render_pass;
}

}  // namespace

class SurfaceSavedFrameTest : public testing::Test {
 public:
  std::unique_ptr<SurfaceSavedFrame> CreateSavedFrame() {
    return SurfaceSavedFrame::CreateForTesting(
        CreateDirective(),
        shared_image_interface_provider_.GetSharedImageInterface(),
        base::DoNothing());
  }

 protected:
  TestSharedImageInterfaceProvider shared_image_interface_provider_;
};

TEST_F(SurfaceSavedFrameTest, CopyRequestColorSpaceMatchesCompositingMode) {
  const auto gpu_saved_frame = CreateSavedFrame();
  const auto gpu_request = gpu_saved_frame->CreateCopyRequestForTesting(
      *CreateRenderPass(), /*is_software=*/false,
      gfx::ContentColorUsage::kWideColorGamut);
  ASSERT_TRUE(gpu_request);
  ASSERT_TRUE(gpu_request->has_blit_request());

  const auto& gpu_shared_image = gpu_request->blit_request().shared_image();
  EXPECT_FALSE(gpu_shared_image->is_software());
  EXPECT_EQ(SinglePlaneFormat::kRGBA_F16, gpu_shared_image->format());
  EXPECT_EQ(gfx::ColorSpace::CreateDisplayP3D65(),
            gpu_shared_image->color_space());

  const auto software_saved_frame = CreateSavedFrame();
  const auto software_request =
      software_saved_frame->CreateCopyRequestForTesting(
          *CreateRenderPass(), /*is_software=*/true,
          gfx::ContentColorUsage::kWideColorGamut);
  ASSERT_TRUE(software_request);
  ASSERT_TRUE(software_request->has_blit_request());

  const auto& software_shared_image =
      software_request->blit_request().shared_image();
  EXPECT_TRUE(software_shared_image->is_software());
  EXPECT_EQ(CreateWideGamutDisplayColorSpaces().GetOutputFormat(
                gfx::ContentColorUsage::kSRGB,
                /*needs_alpha=*/true),
            software_shared_image->format());
  EXPECT_EQ(gfx::ColorSpace::CreateSRGB(),
            software_shared_image->color_space());
}

}  // namespace viz
