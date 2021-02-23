// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
namespace {

// The size of the source texture or framebuffer.
constexpr gfx::Size kSourceSize = gfx::Size(240, 120);

base::FilePath GetTestFilePath(const base::FilePath::CharType* basename) {
  base::FilePath test_dir;
  base::PathService::Get(Paths::DIR_TEST_DATA, &test_dir);
  return test_dir.Append(base::FilePath(basename));
}

SharedQuadState* CreateSharedQuadState(AggregatedRenderPass* render_pass,
                                       const gfx::Rect& rect) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = rect;
  const gfx::Rect clip_rect = rect;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), layer_rect, visible_layer_rect,
                       gfx::MaskFilterInfo(), clip_rect, /*is_clipped=*/false,
                       /*are_contents_opaque=*/false, /*opacity=*/1.0f,
                       SkBlendMode::kSrcOver,
                       /*sorting_context_id=*/0);
  return shared_state;
}

base::span<const uint8_t> MakePixelSpan(const SkBitmap& bitmap) {
  return base::make_span(static_cast<const uint8_t*>(bitmap.getPixels()),
                         bitmap.computeByteSize());
}

void DeleteSharedImage(scoped_refptr<ContextProvider> context_provider,
                       gpu::Mailbox mailbox,
                       const gpu::SyncToken& sync_token,
                       bool is_lost) {
  DCHECK(context_provider);
  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(sync_token, mailbox);
}

}  // namespace

class SkiaReadbackPixelTest : public cc::PixelTest,
                              public testing::WithParamInterface<bool> {
 public:
  // TODO(kylechar): Add parameter to also test RGBA_TEXTURE when it's
  // supported with the Skia readback API.
  CopyOutputResult::Format RequestFormat() const {
    return CopyOutputResult::Format::RGBA_BITMAP;
  }
  bool ScaleByHalf() const { return GetParam(); }

  void SetUp() override {
    SetUpSkiaRenderer(gfx::SurfaceOrigin::kBottomLeft);

    ASSERT_TRUE(cc::ReadPNGFile(
        GetTestFilePath(FILE_PATH_LITERAL("16_color_rects.png")),
        &source_bitmap_));
    source_bitmap_.setImmutable();
  }

  // Returns filepath for expected output PNG.
  base::FilePath GetExpectedPath() const {
    return GetTestFilePath(
        ScaleByHalf() ? FILE_PATH_LITERAL("half_of_one_of_16_color_rects.png")
                      : FILE_PATH_LITERAL("one_of_16_color_rects.png"));
  }

  // TODO(kylechar): Create an OOP-R style GPU resource with no GL dependencies.
  ResourceId CreateGpuResource(const gfx::Size& size,
                               ResourceFormat format,
                               base::span<const uint8_t> pixels) {
    gpu::SharedImageInterface* sii =
        child_context_provider_->SharedImageInterface();
    DCHECK(sii);
    gpu::Mailbox mailbox = sii->CreateSharedImage(
        format, size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, gpu::SHARED_IMAGE_USAGE_DISPLAY, pixels);
    gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

    TransferableResource gl_resource = TransferableResource::MakeGL(
        mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token, size,
        /*is_overlay_candidate=*/false);
    gl_resource.format = format;
    auto release_callback = SingleReleaseCallback::Create(
        base::BindOnce(&DeleteSharedImage, child_context_provider_, mailbox));
    return child_resource_provider_->ImportResource(
        gl_resource, std::move(release_callback));
  }

  // Creates a RenderPass that embeds a single quad containing |source_bitmap_|.
  std::unique_ptr<AggregatedRenderPass> GenerateRootRenderPass() {
    ResourceFormat format =
        (source_bitmap_.info().colorType() == kBGRA_8888_SkColorType)
            ? ResourceFormat::BGRA_8888
            : ResourceFormat::RGBA_8888;
    ResourceId resource_id =
        CreateGpuResource(kSourceSize, format, MakePixelSpan(source_bitmap_));

    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        cc::SendResourceAndGetChildToParentMap(
            {resource_id}, this->resource_provider_.get(),
            this->child_resource_provider_.get(),
            this->child_context_provider_.get());
    ResourceId mapped_resource_id = resource_map[resource_id];

    const gfx::Rect output_rect(kSourceSize);
    auto pass = std::make_unique<AggregatedRenderPass>();
    pass->SetNew(AggregatedRenderPassId{1}, output_rect, output_rect,
                 gfx::Transform());

    SharedQuadState* sqs = CreateSharedQuadState(pass.get(), output_rect);

    auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
    quad->SetNew(sqs, output_rect, output_rect, /*needs_blending=*/false,
                 mapped_resource_id, gfx::RectF(output_rect), kSourceSize,
                 /*contents_premultiplied=*/true, /*nearest_neighbor=*/true,
                 /*force_anti_aliasing_off=*/false);
    return pass;
  }

 protected:
  SkBitmap source_bitmap_;
};

// Test that SkiaRenderer readback works correctly. This test will use the
// default readback implementation for the platform, which is either the legacy
// GLRendererCopier or the new Skia readback API.
TEST_P(SkiaReadbackPixelTest, ExecutesCopyRequest) {
  // Generates a RenderPass which contains one quad that spans the full output.
  // The quad has our source image, so the framebuffer should contain the source
  // image after DrawFrame().
  auto pass = GenerateRootRenderPass();

  std::unique_ptr<CopyOutputResult> result;
  base::RunLoop loop;
  auto request = std::make_unique<CopyOutputRequest>(
      RequestFormat(),
      base::BindOnce(
          [](std::unique_ptr<CopyOutputResult>* result_out,
             base::OnceClosure quit_closure,
             std::unique_ptr<CopyOutputResult> result_from_copier) {
            *result_out = std::move(result_from_copier);
            std::move(quit_closure).Run();
          },
          &result, loop.QuitClosure()));

  // In order to test coordinate calculations the tests will issue copy requests
  // for a small region just to the right and below the center of the entire
  // source texture/framebuffer.
  gfx::Rect result_selection(kSourceSize.width() / 2, kSourceSize.height() / 2,
                             kSourceSize.width() / 4, kSourceSize.height() / 4);

  // Build CopyOutputRequest based on test parameters.
  if (ScaleByHalf()) {
    result_selection =
        gfx::ScaleToEnclosingRect(gfx::Rect(result_selection), 0.5f);
    request->SetUniformScaleRatio(2, 1);
  }
  request->set_result_selection(result_selection);
  pass->copy_requests.push_back(std::move(request));

  AggregatedRenderPassList pass_list;
  SurfaceDamageRectList surface_damage_rect_list;
  pass_list.push_back(std::move(pass));

  renderer_->DecideRenderPassAllocationsForFrame(pass_list);
  renderer_->DrawFrame(&pass_list, 1.0f, kSourceSize, gfx::DisplayColorSpaces(),
                       std::move(surface_damage_rect_list));
  // Call SwapBuffersSkipped(), so the renderer can have a chance to release
  // resources.
  renderer_->SwapBuffersSkipped();

  loop.Run();

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result_selection, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  const SkBitmap actual = result->AsSkBitmap();

  base::FilePath expected_path = GetExpectedPath();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRebaselinePixelTests)) {
    EXPECT_TRUE(cc::WritePNGFile(actual, expected_path, false));
  }

  if (!cc::MatchesPNGFile(actual, expected_path,
                          cc::ExactPixelComparator(false))) {
    LOG(ERROR) << "Entire source: " << cc::GetPNGDataUrl(source_bitmap_);
    ADD_FAILURE();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SkiaReadbackPixelTest,
                         // Result scaling: Scale by half?
                         testing::Values(true, false));

}  // namespace viz
