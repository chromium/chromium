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
#include "base/test/bind.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/gl_scaler_test_util.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
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
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), layer_rect, visible_layer_rect,
                       gfx::MaskFilterInfo(), /*clip_rect=*/absl::nullopt,
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

// TODO(kylechar): Create an OOP-R style GPU resource with no GL dependencies.
ResourceId CreateGpuResource(
    scoped_refptr<ContextProvider> child_context_provider,
    ClientResourceProvider* child_resource_provider,
    const gfx::Size& size,
    ResourceFormat format,
    base::span<const uint8_t> pixels) {
  gpu::SharedImageInterface* sii =
      child_context_provider->SharedImageInterface();
  DCHECK(sii);
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      format, size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, gpu::SHARED_IMAGE_USAGE_DISPLAY, pixels);
  gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

  TransferableResource gl_resource = TransferableResource::MakeGL(
      mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token, size,
      /*is_overlay_candidate=*/false);
  gl_resource.format = format;
  auto release_callback =
      base::BindOnce(&DeleteSharedImage, child_context_provider, mailbox);
  return child_resource_provider->ImportResource(gl_resource,
                                                 std::move(release_callback));
}

// Creates a RenderPass that embeds a single quad containing |bitmap|.
std::unique_ptr<AggregatedRenderPass> GenerateRootRenderPass(
    DisplayResourceProvider* resource_provider,
    scoped_refptr<ContextProvider> child_context_provider,
    ClientResourceProvider* child_resource_provider,
    const SkBitmap& bitmap) {
  ResourceFormat format = (bitmap.info().colorType() == kBGRA_8888_SkColorType)
                              ? ResourceFormat::BGRA_8888
                              : ResourceFormat::RGBA_8888;
  ResourceId resource_id =
      CreateGpuResource(child_context_provider, child_resource_provider,
                        kSourceSize, format, MakePixelSpan(bitmap));

  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap({resource_id}, resource_provider,
                                             child_resource_provider,
                                             child_context_provider.get());
  ResourceId mapped_resource_id = resource_map[resource_id];

  const gfx::Rect output_rect(kSourceSize);
  auto pass = std::make_unique<AggregatedRenderPass>();
  pass->SetNew(AggregatedRenderPassId{1}, output_rect, output_rect,
               gfx::Transform());

  SharedQuadState* sqs = CreateSharedQuadState(pass.get(), output_rect);

  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(sqs, output_rect, output_rect, /*needs_blending=*/false,
               mapped_resource_id, gfx::RectF(output_rect), kSourceSize,
               /*is_premultiplied=*/true, /*nearest_neighbor=*/true,
               /*force_anti_aliasing_off=*/false);
  return pass;
}

void ReadbackTextureOnGpuThread(gpu::SharedImageManager* shared_image_manager,
                                gpu::SharedContextState* context_state,
                                const gpu::Mailbox& mailbox,
                                const gfx::Size& texture_size,
                                SkColorType color_type,
                                SkBitmap& out_bitmap) {
  DCHECK(color_type == kAlpha_8_SkColorType ||
         color_type == kR8G8_unorm_SkColorType);

  if (!context_state->MakeCurrent(nullptr))
    return;

  auto representation = shared_image_manager->ProduceSkia(
      mailbox, context_state->memory_type_tracker(), context_state);

  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto scoped_write = representation->BeginScopedWriteAccess(
      /*final_msaa_count=*/0, surface_props, &begin_semaphores, &end_semaphores,
      gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

  auto* surface = scoped_write->surface();

  surface->wait(begin_semaphores.size(), begin_semaphores.data());

  size_t row_bytes =
      texture_size.width() * (color_type == kAlpha_8_SkColorType ? 1 : 2);
  size_t num_bytes = row_bytes * texture_size.height();

  DCHECK_EQ(num_bytes, out_bitmap.computeByteSize());
  DCHECK_EQ(row_bytes, out_bitmap.rowBytes());
  DCHECK_EQ(
      static_cast<size_t>(out_bitmap.width() * out_bitmap.bytesPerPixel()),
      out_bitmap.rowBytes());

  SkPixmap pixmap(SkImageInfo::Make(texture_size.width(), texture_size.height(),
                                    color_type, kUnpremul_SkAlphaType),
                  out_bitmap.getAddr(0, 0), row_bytes);

  bool success = surface->readPixels(pixmap, 0, 0);
  DCHECK(success);

  GrFlushInfo flush_info;
  flush_info.fNumSemaphores = end_semaphores.size();
  flush_info.fSignalSemaphores = end_semaphores.data();

  gpu::AddVulkanCleanupTaskForSkiaFlush(context_state->vk_context_provider(),
                                        &flush_info);

  surface->flush(SkSurface::BackendSurfaceAccess::kNoAccess, flush_info);
}

// Reads back NV12 planes from textures returned in the result.
// Will issue a task to the GPU thread and block on its completion.
// The |texture_size| needs to be passed in explicitly, because if the request
// was issued with an appended BlitRequest, the |result->size()| only describes
// the size of the region that was populated in the caller-provided textures,
// *not* the entire texture.
void ReadbackNV12Planes(TestGpuServiceHolder* gpu_service_holder,
                        CopyOutputResult* result,
                        const gfx::Size& texture_size,
                        SkBitmap& out_luma_plane,
                        SkBitmap& out_chroma_planes) {
  base::WaitableEvent wait;

  gpu_service_holder->ScheduleGpuTask(base::BindLambdaForTesting(
      [&out_luma_plane, &out_chroma_planes, result, &wait, &texture_size]() {
        auto* shared_image_manager = TestGpuServiceHolder::GetInstance()
                                         ->gpu_service()
                                         ->shared_image_manager();
        auto* context_state = TestGpuServiceHolder::GetInstance()
                                  ->gpu_service()
                                  ->GetContextState()
                                  .get();

        ReadbackTextureOnGpuThread(
            shared_image_manager, context_state,
            result->GetTextureResult()->planes[0].mailbox, texture_size,
            kAlpha_8_SkColorType, out_luma_plane);

        ReadbackTextureOnGpuThread(
            shared_image_manager, context_state,
            result->GetTextureResult()->planes[1].mailbox,
            gfx::Size(texture_size.width() / 2, texture_size.height() / 2),
            kR8G8_unorm_SkColorType, out_chroma_planes);

        wait.Signal();
      }));

  wait.Wait();
}

// Generates a sequence of bytes of specified length, using the given pattern.
// The pattern will be repeated in the generated sequence, and does not have to
// fit in the |num_bytes| evenly. For example, for byte pattern A B C and length
// 7, the result will be: A B C A B C A.
std::vector<uint8_t> GeneratePixels(size_t num_bytes,
                                    base::span<const uint8_t> pattern) {
  std::vector<uint8_t> result;
  result.reserve(num_bytes);

  while (result.size() < num_bytes) {
    result.push_back(pattern[result.size() % pattern.size()]);
  }

  return result;
}

}  // namespace

class SkiaReadbackPixelTest : public cc::PixelTest,
                              public testing::WithParamInterface<bool> {
 public:
  CopyOutputResult::Format RequestFormat() const {
    return CopyOutputResult::Format::RGBA;
  }

  // TODO(kylechar): Add parameter to also test RGBA_TEXTURE when it's
  // supported with the Skia readback API.
  CopyOutputResult::Destination RequestDestination() const {
    return CopyOutputResult::Destination::kSystemMemory;
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

  // In order to test coordinate calculations the tests will issue copy
  // requests for a small region just to the right and below the center of the
  // entire source texture/framebuffer.
  gfx::Rect result_selection(kSourceSize.width() / 2, kSourceSize.height() / 2,
                             kSourceSize.width() / 4, kSourceSize.height() / 4);

  if (ScaleByHalf()) {
    result_selection =
        gfx::ScaleToEnclosingRect(gfx::Rect(result_selection), 0.5f);
  }

  std::unique_ptr<CopyOutputResult> result;
  {
    auto pass = GenerateRootRenderPass(
        resource_provider_.get(), child_context_provider_.get(),
        child_resource_provider_.get(), source_bitmap_);
    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        RequestFormat(), RequestDestination(),
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result_out,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result_out = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));

    // Build CopyOutputRequest based on test parameters.
    if (ScaleByHalf()) {
      request->SetUniformScaleRatio(2, 1);
    }
    request->set_result_selection(result_selection);

    pass->copy_requests.push_back(std::move(request));

    AggregatedRenderPassList pass_list;
    SurfaceDamageRectList surface_damage_rect_list;
    pass_list.push_back(std::move(pass));

    renderer_->DecideRenderPassAllocationsForFrame(pass_list);
    renderer_->DrawFrame(&pass_list, 1.0f, kSourceSize,
                         gfx::DisplayColorSpaces(),
                         std::move(surface_damage_rect_list));
    // Call SwapBuffersSkipped(), so the renderer can have a chance to release
    // resources.
    renderer_->SwapBuffersSkipped();

    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result_selection, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  auto scoped_bitmap = result->ScopedAccessSkBitmap();
  auto actual = scoped_bitmap.bitmap();

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

class SkiaReadbackPixelTestNV12
    : public cc::PixelTest,
      public testing::WithParamInterface<
          std::tuple<bool, CopyOutputResult::Destination>> {
 public:
  bool ScaleByHalf() const { return std::get<0>(GetParam()); }

  CopyOutputResult::Destination RequestDestination() const {
    return std::get<1>(GetParam());
  }

  CopyOutputResult::Format RequestFormat() const {
    return CopyOutputResult::Format::NV12_PLANES;
  }

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

  gpu::gles2::GLES2Interface* gl() {
    return child_context_provider_->ContextGL();
  }

 protected:
  SkBitmap source_bitmap_;
};

// Test that SkiaRenderer readback works correctly. This test will use the
// default readback implementation for the platform, which is either the legacy
// GLRendererCopier or the new Skia readback API.
TEST_P(SkiaReadbackPixelTestNV12, ExecutesCopyRequest) {
  // Generates a RenderPass which contains one quad that spans the full output.
  // The quad has our source image, so the framebuffer should contain the source
  // image after DrawFrame().

  // In order to test coordinate calculations the tests will issue copy requests
  // for a small region just to the right and below the center of the entire
  // source texture/framebuffer.
  gfx::Rect result_selection(kSourceSize.width() / 2, kSourceSize.height() / 2,
                             kSourceSize.width() / 4, kSourceSize.height() / 4);

  if (ScaleByHalf()) {
    result_selection =
        gfx::ScaleToEnclosingRect(gfx::Rect(result_selection), 0.5f);
  }

  // Check if width/2 and height/2 are even.
  if (result_selection.width() % 2 != 0 || result_selection.height() % 2 != 0) {
    // TODO(https://crbug.com/1256483): Fail the test case after adjusting asset
    // sizes, if we got odd dimensions it means that the assets have been
    // accidentally changed to no longer be even, even after scaling by half.
    GTEST_SKIP()
        << " The test case expects the result size to match the "
           "request size exactly, which is not possible with NV12 "
           "when the request size dimensions aren't even. result_selection="
        << result_selection.ToString() << ", ScaleByHalf()=" << ScaleByHalf()
        << ", RequestDestination()="
        << static_cast<int>(base::to_underlying(RequestDestination()));
  }

  std::unique_ptr<CopyOutputResult> result;
  {
    auto pass = GenerateRootRenderPass(
        resource_provider_.get(), child_context_provider_.get(),
        child_resource_provider_.get(), source_bitmap_);

    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        RequestFormat(), RequestDestination(),
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result_out,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result_out = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));

    // Build CopyOutputRequest based on test parameters.
    if (ScaleByHalf()) {
      request->SetUniformScaleRatio(2, 1);
    }
    request->set_result_selection(result_selection);

    pass->copy_requests.push_back(std::move(request));

    AggregatedRenderPassList pass_list;
    SurfaceDamageRectList surface_damage_rect_list;
    pass_list.push_back(std::move(pass));

    renderer_->DecideRenderPassAllocationsForFrame(pass_list);
    renderer_->DrawFrame(&pass_list, 1.0f, kSourceSize,
                         gfx::DisplayColorSpaces(),
                         std::move(surface_damage_rect_list));
    // Call SwapBuffersSkipped(), so the renderer can have a chance to release
    // resources.
    renderer_->SwapBuffersSkipped();

    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(result_selection, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  // Approach is the same as the one in GLNV12ConverterPixelTest.

  SkBitmap luma_plane;
  SkBitmap chroma_planes;

  // Packed plane sizes:
  const gfx::Size luma_plane_size =
      gfx::Size(result->size().width() / 4, result->size().height());
  const gfx::Size chroma_planes_size =
      gfx::Size(luma_plane_size.width(), luma_plane_size.height() / 2);

  if (RequestDestination() == CopyOutputResult::Destination::kSystemMemory) {
    // Create a bitmap with packed Y values:
    luma_plane = GLScalerTestUtil::AllocateRGBABitmap(luma_plane_size);
    chroma_planes = GLScalerTestUtil::AllocateRGBABitmap(chroma_planes_size);

    result->ReadNV12Planes(static_cast<uint8_t*>(luma_plane.getAddr(0, 0)),
                           result->size().width(),
                           static_cast<uint8_t*>(chroma_planes.getAddr(0, 0)),
                           result->size().width());
  } else {
    luma_plane = GLScalerTestUtil::AllocateRGBABitmap(luma_plane_size);
    chroma_planes = GLScalerTestUtil::AllocateRGBABitmap(chroma_planes_size);

    ReadbackNV12Planes(gpu_service_holder_, result.get(), result->size(),
                       luma_plane, chroma_planes);
  }

  // Allocate new bitmap & populate it with Y & UV data.
  SkBitmap actual = GLScalerTestUtil::AllocateRGBABitmap(result->size());
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x00, 0x00));

  GLScalerTestUtil::UnpackPlanarBitmap(luma_plane, 0, &actual);
  GLScalerTestUtil::UnpackUVBitmap(chroma_planes, &actual);

  SkBitmap expected;
  if (!cc::ReadPNGFile(GetExpectedPath(), &expected)) {
    LOG(ERROR) << "Cannot read reference image: " << GetExpectedPath().value();
    ADD_FAILURE();
    return;
  }

  expected = GLScalerTestUtil::CopyAndConvertToRGBA(expected);
  GLScalerTestUtil::ConvertRGBABitmapToYUV(&expected);

  constexpr float kAvgAbsoluteErrorLimit = 16.f;
  constexpr int kMaxAbsoluteErrorLimit = 128;
  if (!cc::MatchesBitmap(
          actual, expected,
          cc::FuzzyPixelComparator(false, 100.f, 0.f, kAvgAbsoluteErrorLimit,
                                   kMaxAbsoluteErrorLimit, 0))) {
    ADD_FAILURE();
    return;
  }
}

#if !defined(OS_ANDROID) || !defined(ARCH_CPU_X86_FAMILY)
INSTANTIATE_TEST_SUITE_P(
    ,
    SkiaReadbackPixelTestNV12,
    // Result scaling: Scale by half?
    testing::Combine(
        testing::Values(true, false),
        testing::Values(CopyOutputResult::Destination::kSystemMemory,
                        CopyOutputResult::Destination::kNativeTextures)));
#else
// Don't instantiate the NV12 tests when run on Android emulator, they won't
// work since the SkiaRenderer currently does not support CopyOutputRequests
// with NV12 format if the platform does not support GL_EXT_texture_rg extension
// in GL ES 2.0 (which is the case on Android emulator).
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SkiaReadbackPixelTestNV12);
#endif

class SkiaReadbackPixelTestNV12WithBlit
    : public cc::PixelTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ScaleByHalf() const { return GetParam(); }

  CopyOutputResult::Destination RequestDestination() const {
    return CopyOutputResult::Destination::kNativeTextures;
  }

  CopyOutputResult::Format RequestFormat() const {
    return CopyOutputResult::Format::NV12_PLANES;
  }

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

  gpu::gles2::GLES2Interface* gl() {
    return child_context_provider_->ContextGL();
  }

 protected:
  SkBitmap source_bitmap_;
};

// Test that SkiaRenderer readback works correctly. This test will use the
// default readback implementation for the platform, which is either the legacy
// GLRendererCopier or the new Skia readback API.
TEST_P(SkiaReadbackPixelTestNV12WithBlit, ExecutesCopyRequestWithBlit) {
  // In order to test coordinate calculations the tests will issue copy requests
  // for a small region just to the right and below the center of the entire
  // source texture/framebuffer.
  gfx::Rect result_selection(kSourceSize.width() / 2, kSourceSize.height() / 2,
                             kSourceSize.width() / 4, kSourceSize.height() / 4);

  if (ScaleByHalf()) {
    result_selection =
        gfx::ScaleToEnclosingRect(gfx::Rect(result_selection), 0.5f);
  }

  // Check if width/2 and height/2 are even.
  if (result_selection.width() % 2 != 0 || result_selection.height() % 2 != 0) {
    // TODO(https://crbug.com/1256483): Fail the test case after adjusting asset
    // sizes, if we got odd dimensions it means that the assets have been
    // accidentally changed to no longer be even, even after scaling by half.
    GTEST_SKIP()
        << " The test case expects the result size to match the "
           "request size exactly, which is not possible with NV12 "
           "when the request size dimensions aren't even. result_selection="
        << result_selection.ToString() << ", ScaleByHalf()=" << ScaleByHalf()
        << ", RequestDestination()="
        << static_cast<int>(base::to_underlying(RequestDestination()));
  }

  // Generate 2 shared images that will be owned by us. They will be used as the
  // destination for the issued BlitRequest. The logical size of the image will
  // be the same as kSourceSize. The destination region will be the same size of
  // |result_selection| rectangle, with the same center as the center of
  // kSourceSize rectangle. As a consequence, the CopyOutputResult should
  // contain the pixels from the source image in the middle, and the rest should
  // remain unchanged.

  gfx::Rect destination_subregion = gfx::Rect(kSourceSize);
  destination_subregion.ClampToCenteredSize(result_selection.size());

  if (destination_subregion.x() % 2 != 0 ||
      destination_subregion.y() % 2 != 0) {
    // TODO(https://crbug.com/1256483): Fail the test case after adjusting asset
    // sizes, if we got odd origin it means that the assets have been
    // accidentally changed to no longer be even.
    GTEST_SKIP() << " The test case expects the blit region's origin to be "
                    "even for NV12 "
                    " blit requests";
  }

  const SkColor rgba_red = SkColorSetARGB(0xff, 0xff, 0, 0);
  const SkColor yuv_red = GLScalerTestUtil::ConvertRGBAColorToYUV(rgba_red);

  const std::vector<uint8_t> luma_pattern = {
      static_cast<uint8_t>(SkColorGetR(yuv_red))};
  const std::vector<uint8_t> chromas_pattern = {
      static_cast<uint8_t>(SkColorGetG(yuv_red)),
      static_cast<uint8_t>(SkColorGetB(yuv_red))};

  std::array<gpu::MailboxHolder, CopyOutputResult::kMaxPlanes> mailboxes;
  for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
    const auto resource_format =
        i == 0 ? ResourceFormat::RED_8 : ResourceFormat::RG_88;
    const gfx::Size plane_size =
        i == 0 ? kSourceSize
               : gfx::Size(kSourceSize.width() / 2, kSourceSize.height() / 2);
    const size_t plane_size_in_bytes =
        plane_size.GetArea() *
        (resource_format == ResourceFormat::RED_8 ? 1 : 2);

    std::vector<uint8_t> pixels =
        (i == 0) ? GeneratePixels(plane_size_in_bytes, luma_pattern)
                 : GeneratePixels(plane_size_in_bytes, chromas_pattern);

    mailboxes[i].mailbox =
        child_context_provider_->SharedImageInterface()->CreateSharedImage(
            resource_format, plane_size, gfx::ColorSpace::CreateREC709(),
            kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
            gpu::SHARED_IMAGE_USAGE_DISPLAY, pixels);
    DCHECK(!mailboxes[i].mailbox.IsZero());
  }

  std::unique_ptr<CopyOutputResult> result;
  {
    auto pass = GenerateRootRenderPass(
        resource_provider_.get(), child_context_provider_.get(),
        child_resource_provider_.get(), source_bitmap_);

    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        RequestFormat(), RequestDestination(),
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result_out,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result_out = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));

    // Build CopyOutputRequest based on test parameters.
    if (ScaleByHalf()) {
      request->SetUniformScaleRatio(2, 1);
    }
    request->set_result_selection(result_selection);

    request->set_blit_request(
        BlitRequest(destination_subregion.origin(), mailboxes));

    pass->copy_requests.push_back(std::move(request));

    AggregatedRenderPassList pass_list;
    SurfaceDamageRectList surface_damage_rect_list;
    pass_list.push_back(std::move(pass));

    renderer_->DecideRenderPassAllocationsForFrame(pass_list);
    renderer_->DrawFrame(&pass_list, 1.0f, kSourceSize,
                         gfx::DisplayColorSpaces(),
                         std::move(surface_damage_rect_list));
    // Call SwapBuffersSkipped(), so the renderer can have a chance to release
    // resources.
    renderer_->SwapBuffersSkipped();

    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(result_selection, result->rect());
  ASSERT_EQ(result->destination(),
            CopyOutputResult::Destination::kNativeTextures);

  // Packed plane sizes. Note that for blit request, the size of the returned
  // textures is caller-controlled, and we have issued a COR w/ blit request
  // that is supposed to write to an image of |kSourceSize| size.
  const gfx::Size luma_plane_size =
      gfx::Size(kSourceSize.width() / 4, kSourceSize.height());
  const gfx::Size chroma_planes_size =
      gfx::Size(luma_plane_size.width(), luma_plane_size.height() / 2);

  SkBitmap luma_plane = GLScalerTestUtil::AllocateRGBABitmap(luma_plane_size);
  SkBitmap chroma_planes =
      GLScalerTestUtil::AllocateRGBABitmap(chroma_planes_size);

  ReadbackNV12Planes(gpu_service_holder_, result.get(), kSourceSize, luma_plane,
                     chroma_planes);

  for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
    child_context_provider_->SharedImageInterface()->DestroySharedImage(
        result->GetTextureResult()->planes[i].sync_token,
        result->GetTextureResult()->planes[i].mailbox);
  }

  // Allocate new bitmap & populate it with Y & UV data.
  SkBitmap actual = GLScalerTestUtil::AllocateRGBABitmap(kSourceSize);
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x00, 0x00));

  GLScalerTestUtil::UnpackPlanarBitmap(luma_plane, 0, &actual);
  GLScalerTestUtil::UnpackUVBitmap(chroma_planes, &actual);

  // Load the expected subregion from a file - we will then write it on top of
  // a new, all-red bitmap:
  SkBitmap expected_subregion;
  if (!cc::ReadPNGFile(GetExpectedPath(), &expected_subregion)) {
    LOG(ERROR) << "Cannot read reference image: " << GetExpectedPath().value();
    ADD_FAILURE();
    return;
  }
  expected_subregion =
      GLScalerTestUtil::CopyAndConvertToRGBA(expected_subregion);

  // The textures that we passed in to BlitRequest contained NV12 plane data for
  // an all-red image, let's re-create such a bitmap:
  SkBitmap expected = GLScalerTestUtil::AllocateRGBABitmap(kSourceSize);
  expected.eraseColor(rgba_red);

  // Blit request should "stitch" the pixels from the source image into a
  // sub-region of caller-provided texture - let's write our expected pixels
  // loaded from a file into the same subregion of an all-red texture:
  expected.writePixels(expected_subregion.pixmap(), destination_subregion.x(),
                       destination_subregion.y());

  // Now let's convert it to YUV so we can compare with the result:
  GLScalerTestUtil::ConvertRGBABitmapToYUV(&expected);

  constexpr float kAvgAbsoluteErrorLimit = 16.f;
  constexpr int kMaxAbsoluteErrorLimit = 128;
  if (!cc::MatchesBitmap(
          actual, expected,
          cc::FuzzyPixelComparator(false, 100.f, 0.f, kAvgAbsoluteErrorLimit,
                                   kMaxAbsoluteErrorLimit, 0))) {
    ADD_FAILURE();
    return;
  }
}

#if !defined(OS_ANDROID) || !defined(ARCH_CPU_X86_FAMILY)
INSTANTIATE_TEST_SUITE_P(,
                         SkiaReadbackPixelTestNV12WithBlit,
                         // Result scaling: Scale by half?
                         testing::Values(true, false));
#else
// Don't instantiate the NV12 tests when run on Android emulator, they won't
// work since the SkiaRenderer currently does not support CopyOutputRequests
// with NV12 format if the platform does not support GL_EXT_texture_rg extension
// in GL ES 2.0 (which is the case on Android emulator).
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    SkiaReadbackPixelTestNV12WithBlit);
#endif

}  // namespace viz
