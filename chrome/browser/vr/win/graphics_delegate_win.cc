// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/graphics_delegate_win.h"

#include "base/numerics/math_constants.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/gpu_stream_constants.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

namespace vr {

namespace {
constexpr float kZNear = 0.1f;
constexpr float kZFar = 10000.0f;
}  // namespace

GraphicsDelegateWin::GraphicsDelegateWin() = default;

GraphicsDelegateWin::~GraphicsDelegateWin() = default;

bool GraphicsDelegateWin::InitializeOnMainThread() {
  gpu::GpuChannelEstablishFactory* factory =
      content::GetGpuChannelEstablishFactory();
  scoped_refptr<gpu::GpuChannelHost> host = factory->EstablishGpuChannelSync();

  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.red_size = 8;
  attributes.green_size = 8;
  attributes.blue_size = 8;
  attributes.stencil_size = 0;
  attributes.depth_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;

  context_provider_ = base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      host, factory->GetGpuMemoryBufferManager(), content::kGpuStreamIdDefault,
      content::kGpuStreamPriorityUI, gpu::kNullSurfaceHandle,
      GURL(std::string("chrome://gpu/VrUiWin")), false /* automatic flushes */,
      false /* support locking */, false /* support grcontext */,
      gpu::SharedMemoryLimits::ForMailboxContext(), attributes,
      viz::command_buffer_metrics::ContextType::XR_COMPOSITING);
  gpu_memory_buffer_manager_ = factory->GetGpuMemoryBufferManager();
  return true;
}

void GraphicsDelegateWin::InitializeOnGLThread() {
  DCHECK(context_provider_);
  if (context_provider_->BindToCurrentSequence() ==
      gpu::ContextResult::kSuccess) {
    gl_ = context_provider_->ContextGL();
    sii_ = context_provider_->SharedImageInterface();
  }
}

bool GraphicsDelegateWin::BindContext() {
  if (!gl_)
    return false;

  gles2::SetGLContext(gl_);
  return true;
}

void GraphicsDelegateWin::ClearContext() {
  gles2::SetGLContext(nullptr);
}

gfx::Rect GraphicsDelegateWin::GetTextureSize() {
  int width = left_->viewport.width() + right_->viewport.width();
  int height = std::max(left_->viewport.height(), right_->viewport.height());

  return gfx::Rect(width, height);
}

bool GraphicsDelegateWin::PreRender() {
  if (!gl_)
    return false;

  BindContext();
  gfx::Rect size = GetTextureSize();

  // Create a memory buffer and a shared image referencing that memory buffer.
  if (!EnsureMemoryBuffer(size.width(), size.height()))
    return false;

  // Create a texture id and associate it with shared image.
  dest_texture_id_ =
      gl_->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox_.name);
  gl_->BeginSharedImageAccessDirectCHROMIUM(
      dest_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  gl_->BindTexture(GL_TEXTURE_2D, dest_texture_id_);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Bind our image/texture/memory buffer as the draw framebuffer.
  gl_->GenFramebuffers(1, &draw_frame_buffer_);
  gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer_);
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, dest_texture_id_, 0);

  if (gl_->GetError() != GL_NO_ERROR) {
    // Clear any remaining GL errors.
    while (gl_->GetError() != GL_NO_ERROR) {
    }
    return false;
  }

  return true;
}

void GraphicsDelegateWin::PostRender() {
  // Unbind the drawing buffer.
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl_->DeleteFramebuffers(1, &draw_frame_buffer_);

  gl_->EndSharedImageAccessDirectCHROMIUM(dest_texture_id_);
  gl_->DeleteTextures(1, &dest_texture_id_);
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  dest_texture_id_ = 0;
  draw_frame_buffer_ = 0;

  // Generate a SyncToken after GPU is done accessing the texture.
  gl_->GenSyncTokenCHROMIUM(access_done_sync_token_.GetData());

  // Flush.
  gl_->ShallowFlushCHROMIUM();
  ClearContext();
}

mojo::PlatformHandle GraphicsDelegateWin::GetTexture() {
  if (!gpu_memory_buffer_)
    return {};

  gfx::GpuMemoryBufferHandle gpu_handle = gpu_memory_buffer_->CloneHandle();
  return mojo::PlatformHandle(std::move(gpu_handle.dxgi_handle));
}

const gpu::SyncToken& GraphicsDelegateWin::GetSyncToken() {
  return access_done_sync_token_;
}

gfx::RectF GraphicsDelegateWin::GetLeft() {
  gfx::Rect size = GetTextureSize();
  return gfx::RectF(
      0, 0, static_cast<float>(left_->viewport.width()) / size.width(),
      static_cast<float>(left_->viewport.height()) / size.height());
}

gfx::RectF GraphicsDelegateWin::GetRight() {
  gfx::Rect size = GetTextureSize();
  return gfx::RectF(
      static_cast<float>(left_->viewport.width()) / size.width(), 0,
      static_cast<float>(right_->viewport.width()) / size.width(),
      static_cast<float>(right_->viewport.height()) / size.height());
}

bool GraphicsDelegateWin::EnsureMemoryBuffer(int width, int height) {
  if (last_width_ != width || last_height_ != height || !gpu_memory_buffer_) {
    if (!gpu_memory_buffer_manager_)
      return false;

    if (!mailbox_.IsZero()) {
      sii_->DestroySharedImage(access_done_sync_token_, mailbox_);
      mailbox_.SetZero();
      access_done_sync_token_.Clear();
    }

    gpu_memory_buffer_ = gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
        gfx::Size(width, height), gfx::BufferFormat::RGBA_8888,
        gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle, nullptr);
    if (!gpu_memory_buffer_)
      return false;

    last_width_ = width;
    last_height_ = height;

    mailbox_ = sii_->CreateSharedImage(
        gpu_memory_buffer_.get(), gpu_memory_buffer_manager_, gfx::ColorSpace(),
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
        gpu::SHARED_IMAGE_USAGE_GLES2 |
            gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT,
        "VRGraphicsDelegate");

    gl_->WaitSyncTokenCHROMIUM(sii_->GenUnverifiedSyncToken().GetConstData());
  }
  return true;
}

void GraphicsDelegateWin::ResetMemoryBuffer() {
  // Stop using a memory buffer if we had an error submitting with it.
  gpu_memory_buffer_ = nullptr;
}

void GraphicsDelegateWin::SetXrViews(
    const std::vector<device::mojom::XRViewPtr>& views) {
  // Store the first left and right views.
  for (auto& view : views) {
    if (view->eye == device::mojom::XREye::kLeft) {
      left_ = view.Clone();
    } else if (view->eye == device::mojom::XREye::kRight) {
      right_ = view.Clone();
    }
  }

  DCHECK(left_);
  DCHECK(right_);
}

FovRectangles GraphicsDelegateWin::GetRecommendedFovs() {
  DCHECK(left_);
  DCHECK(right_);
  FovRectangle left = {
      left_->field_of_view->left_degrees,
      left_->field_of_view->right_degrees,
      left_->field_of_view->down_degrees,
      left_->field_of_view->up_degrees,
  };

  FovRectangle right = {
      right_->field_of_view->left_degrees,
      right_->field_of_view->right_degrees,
      right_->field_of_view->down_degrees,
      right_->field_of_view->up_degrees,
  };

  return std::pair<FovRectangle, FovRectangle>(left, right);
}

float GraphicsDelegateWin::GetZNear() {
  return kZNear;
}

namespace {

CameraModel CameraModelViewProjFromXRView(
    const device::mojom::XRViewPtr& view) {
  CameraModel model = {};

  // TODO(https://crbug.com/1070380): mojo space is currently equivalent to
  // world space, so the view matrix is world_from_view.
  model.view_matrix = view->mojo_from_view;

  bool is_invertible = model.view_matrix.GetInverse(&model.view_matrix);
  DCHECK(is_invertible);

  float up_tan = tanf(view->field_of_view->up_degrees * base::kPiFloat / 180.0);
  float left_tan =
      tanf(view->field_of_view->left_degrees * base::kPiFloat / 180.0);
  float right_tan =
      tanf(view->field_of_view->right_degrees * base::kPiFloat / 180.0);
  float down_tan =
      tanf(view->field_of_view->down_degrees * base::kPiFloat / 180.0);
  float x_scale = 2.0f / (left_tan + right_tan);
  float y_scale = 2.0f / (up_tan + down_tan);
  // clang-format off
  model.proj_matrix = gfx::Transform::RowMajor(
      x_scale, 0, -((left_tan - right_tan) * x_scale * 0.5), 0,
      0, y_scale, ((up_tan - down_tan) * y_scale * 0.5), 0,
      0, 0, (kZFar + kZNear) / (kZNear - kZFar),
          2 * kZFar * kZNear / (kZNear - kZFar),
      0, 0, -1, 0);
  // clang-format on
  model.view_proj_matrix = model.proj_matrix * model.view_matrix;
  return model;
}

}  // namespace

RenderInfo GraphicsDelegateWin::GetRenderInfo(FrameType frame_type,
                                              const gfx::Transform& head_pose) {
  RenderInfo info;
  info.head_pose = head_pose;

  CameraModel left = CameraModelViewProjFromXRView(left_);
  left.eye_type = kLeftEye;
  left.viewport =
      gfx::Rect(0, 0, left_->viewport.width(), left_->viewport.height());
  info.left_eye_model = left;

  CameraModel right = CameraModelViewProjFromXRView(right_);
  right.eye_type = kRightEye;
  right.viewport =
      gfx::Rect(left_->viewport.width(), 0, right_->viewport.width(),
                right_->viewport.height());
  info.right_eye_model = right;
  cached_info_ = info;
  return info;
}

RenderInfo GraphicsDelegateWin::GetOptimizedRenderInfoForFovs(
    const FovRectangles& fovs) {
  RenderInfo info = cached_info_;
  // TODO(billorr): consider optimizing overlays to save texture size.
  // For now, we use a full-size texture when we could get by with less.
  return info;
}

void GraphicsDelegateWin::InitializeBuffers() {
  // No-op since we intiailize buffers elsewhere.
}

void GraphicsDelegateWin::PrepareBufferForWebXr() {
  // Desktop doesn't render WebXR through the browser renderer.
  DCHECK(prepared_drawing_buffer_ == DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kWebXr;
}

void GraphicsDelegateWin::PrepareBufferForWebXrOverlayElements() {
  // No-op.  We reuse the same buffer for overlays and other content, which is
  // intialized in PreRender.
  DCHECK(prepared_drawing_buffer_ == DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kWebXrOverlayElements;
}

void GraphicsDelegateWin::PrepareBufferForBrowserUi() {
  gl_->ClearColor(0, 0, 0, 0);
  gl_->Clear(GL_COLOR_BUFFER_BIT);

  DCHECK(prepared_drawing_buffer_ == DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kBrowserUi;
}

void GraphicsDelegateWin::OnFinishedDrawingBuffer() {
  DCHECK(prepared_drawing_buffer_ != DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kNone;
}

void GraphicsDelegateWin::GetWebXrDrawParams(int* texture_id,
                                             Transform* uv_transform) {
  // Reporting a texture_id of 0 will skip texture copies.
  *texture_id = 0;
}

// These methods return true when succeeded.
bool GraphicsDelegateWin::Initialize(
    const scoped_refptr<gl::GLSurface>& surface) {
  // Commandbuffer intialization is split between the main thread and the render
  // thread.  Additionally, it can be async, so we can't really do intialization
  // here - instead, we are initalized earlier.
  NOTREACHED();
  return false;
}

bool GraphicsDelegateWin::RunInSkiaContext(base::OnceClosure callback) {
  // TODO(billorr): Support multiple contexts in a share group.  For now just
  // share one context.
  std::move(callback).Run();
  return true;
}

}  // namespace vr
