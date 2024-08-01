// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu_debug_capture.h"

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/output_presenter_gl.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"
#include "components/viz/service/display_embedder/skia_output_device_gl.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "components/viz/service/display_embedder/skia_output_device_webview.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_render_copy_results.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "ui/gfx/color_space.h"

// We only include the functionality for gpu buffer capture iif the visual
// debugger is enabled at a build level
#if VIZ_DEBUGGER_IS_ON()

namespace {

DBG_FLAG_FBOOL("skia_gpu.buffer_capture.low_res", buffer_capture_low_res)

struct DebuggerCaptureBufferSharedImageInfo {
  viz::SharedImageFormat si_format;
  gfx::Size size;
  gfx::ColorSpace color_space;
  GrSurfaceOrigin surface_origin;
  SkAlphaType alpha_type;
  gpu::SharedImageUsageSet usage;
  gpu::Mailbox mailbox;
};

void DebuggerCaptureBufferCallback(
    void* si_info_raw,
    std::unique_ptr<const SkSurface::AsyncReadResult> async_result) {
  DBG_LOG("skia_gpu.buffer_capture.debugging", "Readback request is %d",
          async_result != nullptr);
  if (!async_result) {
    return;
  }

  std::unique_ptr<DebuggerCaptureBufferSharedImageInfo> si_info(
      static_cast<DebuggerCaptureBufferSharedImageInfo*>(si_info_raw));
  int submit_id = 0;
  DBG_DRAW_RECTANGLE_OPT("skia_gpu.captured_buffer.black",
                         viz::VizDebugger::DrawOption({0, 0, 0, 255}),
                         gfx::Vector2dF(), gfx::SizeF(si_info->size));
  DBG_DRAW_RECTANGLE_OPT_BUFF_UV(
      "skia_gpu.captured_buffer.image", DBG_OPT_BLACK, gfx::Vector2dF(),
      gfx::SizeF(si_info->size), &submit_id, DBG_DEFAULT_UV);

  viz::VizDebugger::BufferInfo buffer_info;
  buffer_info.bitmap.setInfo(SkImageInfo::MakeN32(
      si_info->size.width(), si_info->size.height(), kPremul_SkAlphaType));
  buffer_info.bitmap.allocPixels();
  SkPixmap pixmap(buffer_info.bitmap.info(), async_result->data(0),
                  buffer_info.bitmap.info().minRowBytes());
  buffer_info.bitmap.writePixels(pixmap);

  DBG_COMPLETE_BUFFERS(submit_id, buffer_info);
}
}  // namespace

namespace viz {
void AttemptDebuggerBufferCapture(
    ImageContextImpl* context,
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory) {
  CHECK(context_state);
  if (!context_state->IsCurrent(nullptr)) {
    // There should already be a current context from the caller of this
    // function. We don't make a context current to avoid needing to reset it
    // afterwards.
    DLOG(ERROR) << "No current context state.";
    return;
  }

  auto representation = representation_factory->ProduceSkia(
      context->mailbox_holder().mailbox, context_state);

  if (!representation) {
    DLOG(ERROR) << "Failed to make produce skia representation.";
    return;
  }
  std::vector<GrBackendSemaphore> begin_semaphores_readback;
  std::vector<GrBackendSemaphore> end_semaphores_readback;
  const gfx::Size texture_size = representation->size();

  auto scoped_read = representation->BeginScopedReadAccess(
      &begin_semaphores_readback, &end_semaphores_readback);

  if (!scoped_read) {
    DLOG(ERROR) << "Failed to make begin read from representation";
    return;
  }
  // Perform ApplyBackendSurfaceEndState() on the ScopedReadAccess before
  // exiting.
  absl::Cleanup cleanup = [&]() { scoped_read->ApplyBackendSurfaceEndState(); };

  if (!begin_semaphores_readback.empty()) {
    bool result = context_state->gr_context()->wait(
        begin_semaphores_readback.size(), begin_semaphores_readback.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  auto skimage = scoped_read->CreateSkImage(context_state);

  if (!skimage) {
    DLOG(ERROR) << "Failed to create SkImage from scoped read";
    return;
  }
  DBG_LOG("skia_gpu.buffer_capture.debugging", "Image %s",
          texture_size.ToString().c_str());

  auto resolution_div = buffer_capture_low_res() ? 2 : 1;
  SkImageInfo dst_info = SkImageInfo::Make(
      texture_size.width() / resolution_div,
      texture_size.height() / resolution_div, kBGRA_8888_SkColorType,
      kPremul_SkAlphaType, sk_sp<SkColorSpace>());

  auto si_info = std::make_unique<DebuggerCaptureBufferSharedImageInfo>();
  si_info->si_format = context->format();
  si_info->size = gfx::Size(dst_info.width(), dst_info.height());
  si_info->color_space = representation->color_space();
  si_info->surface_origin = context->origin();
  si_info->alpha_type = context->alpha_type();
  si_info->usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  si_info->mailbox = context->mailbox_holder().mailbox;

  if (auto* graphite_context = context_state->graphite_context()) {
    // SkImage/SkSurface asyncRescaleAndReadPixels methods won't be implemented
    // for Graphite. Instead the equivalent methods will be on Graphite Context.
    graphite_context->asyncRescaleAndReadPixels(
        skimage.get(), dst_info,
        SkIRect::MakeWH(texture_size.width(), texture_size.height()),
        SkSurface::RescaleGamma::kSrc, SkSurface::RescaleMode::kRepeatedLinear,
        &DebuggerCaptureBufferCallback, si_info.release());
  } else {
    skimage->asyncRescaleAndReadPixels(
        dst_info, SkIRect::MakeWH(texture_size.width(), texture_size.height()),
        SkSurface::RescaleGamma::kSrc, SkSurface::RescaleMode::kRepeatedLinear,
        &DebuggerCaptureBufferCallback, si_info.release());
  }
}

}  // namespace viz
#else
namespace viz {
void AttemptDebuggerBufferCapture(
    ImageContextImpl* context,
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory);
}  // namespace viz
#endif  // VIZ_DEBUGGER_IS_ON()
