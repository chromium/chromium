// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/graphics_delegate_win.h"

#include "base/numerics/math_constants.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace vr {

namespace {
constexpr float kZNear = 0.1f;
constexpr float kZFar = 10000.0f;
}  // namespace

GraphicsDelegateWin::GraphicsDelegateWin() {}

GraphicsDelegateWin::~GraphicsDelegateWin() {}

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
  if (context_provider_->BindToCurrentThread() == gpu::ContextResult::kSuccess)
    gl_ = context_provider_->ContextGL();
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
  int width = info_->left_eye->render_width + info_->right_eye->render_width;
  int height =
      std::max(info_->left_eye->render_height, info_->right_eye->render_width);
  return gfx::Rect(width, height);
}

void GraphicsDelegateWin::PreRender() {
  if (!gl_)
    return;

  BindContext();
  gfx::Rect size = GetTextureSize();

  // Create a memory buffer, and an image referencing that memory buffer.
  if (!EnsureMemoryBuffer(size.width(), size.height()))
    return;

  // Create a texture id, and associate it with our image.
  gl_->GenTextures(1, &dest_texture_id_);
  gl_->BindTexture(GL_TEXTURE_2D, dest_texture_id_);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Bind our image/texture/memory buffer as the draw framebuffer.
  gl_->GenFramebuffers(1, &draw_frame_buffer_);
  gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer_);
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, dest_texture_id_, 0);
}

void GraphicsDelegateWin::PostRender() {
  // Unbind the drawing buffer.
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl_->DeleteFramebuffers(1, &draw_frame_buffer_);
  gl_->BindTexture(GL_TEXTURE_2D, dest_texture_id_);
  gl_->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);
  gl_->DeleteTextures(1, &dest_texture_id_);
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  dest_texture_id_ = 0;
  draw_frame_buffer_ = 0;

  // Flush.
  gl_->ShallowFlushCHROMIUM();
  ClearContext();
}

mojo::ScopedHandle GraphicsDelegateWin::GetTexture() {
  // Hand out the gpu memory buffer.
  mojo::ScopedHandle handle;
  if (!gpu_memory_buffer_) {
    return handle;
  }

  gfx::GpuMemoryBufferHandle gpu_handle = gpu_memory_buffer_->CloneHandle();
  return mojo::WrapPlatformFile(gpu_handle.dxgi_handle.GetHandle());
}

gfx::RectF GraphicsDelegateWin::GetLeft() {
  gfx::Rect size = GetTextureSize();
  return gfx::RectF(
      0, 0, static_cast<float>(info_->left_eye->render_width) / size.width(),
      static_cast<float>(info_->left_eye->render_height) / size.height());
}

gfx::RectF GraphicsDelegateWin::GetRight() {
  gfx::Rect size = GetTextureSize();
  return gfx::RectF(
      static_cast<float>(info_->left_eye->render_width) / size.width(), 0,
      static_cast<float>(info_->right_eye->render_width) / size.width(),
      static_cast<float>(info_->right_eye->render_height) / size.height());
}

void GraphicsDelegateWin::Cleanup() {
  context_provider_ = nullptr;
}

bool GraphicsDelegateWin::EnsureMemoryBuffer(int width, int height) {
  if (last_width_ != width || last_height_ != height || !gpu_memory_buffer_) {
    if (!gpu_memory_buffer_manager_)
      return false;

    if (image_id_) {
      gl_->DestroyImageCHROMIUM(image_id_);
      image_id_ = 0;
    }

    gpu_memory_buffer_ = gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
        gfx::Size(width, height), gfx::BufferFormat::RGBA_8888,
        gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle);
    if (!gpu_memory_buffer_)
      return false;

    last_width_ = width;
    last_height_ = height;

    image_id_ = gl_->CreateImageCHROMIUM(gpu_memory_buffer_->AsClientBuffer(),
                                         width, height, GL_RGBA);
    if (!image_id_) {
      gpu_memory_buffer_ = nullptr;
      return false;
    }
  }
  return true;
}

void GraphicsDelegateWin::ResetMemoryBuffer() {
  // Stop using a memory buffer if we had an error submitting with it.
  gpu_memory_buffer_ = nullptr;
}

void GraphicsDelegateWin::SetVRDisplayInfo(
    device::mojom::VRDisplayInfoPtr info) {
  info_ = std::move(info);
}

FovRectangles GraphicsDelegateWin::GetRecommendedFovs() {
  DCHECK(info_);
  FovRectangle left = {
      info_->left_eye->field_of_view->left_degrees,
      info_->left_eye->field_of_view->right_degrees,
      info_->left_eye->field_of_view->down_degrees,
      info_->left_eye->field_of_view->up_degrees,
  };

  FovRectangle right = {
      info_->right_eye->field_of_view->left_degrees,
      info_->right_eye->field_of_view->right_degrees,
      info_->right_eye->field_of_view->down_degrees,
      info_->right_eye->field_of_view->up_degrees,
  };

  return std::pair<FovRectangle, FovRectangle>(left, right);
}

float GraphicsDelegateWin::GetZNear() {
  return kZNear;
}

namespace {

CameraModel CameraModelViewProjFromVREyeParameters(
    const device::mojom::VREyeParametersPtr& eye_params,
    gfx::Transform head_from_world) {
  CameraModel model = {};

  DCHECK(eye_params->head_from_eye.IsInvertible());
  gfx::Transform eye_from_head;
  if (eye_params->head_from_eye.GetInverse(&eye_from_head)) {
    model.view_matrix = eye_from_head * head_from_world;
  }

  float up_tan =
      tanf(eye_params->field_of_view->up_degrees * base::kPiFloat / 180.0);
  float left_tan =
      tanf(eye_params->field_of_view->left_degrees * base::kPiFloat / 180.0);
  float right_tan =
      tanf(eye_params->field_of_view->right_degrees * base::kPiFloat / 180.0);
  float down_tan =
      tanf(eye_params->field_of_view->down_degrees * base::kPiFloat / 180.0);
  float x_scale = 2.0f / (left_tan + right_tan);
  float y_scale = 2.0f / (up_tan + down_tan);
  // clang-format off
  model.proj_matrix =
      gfx::Transform(x_scale, 0, -((left_tan - right_tan) * x_scale * 0.5), 0,
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

  CameraModel left =
      CameraModelViewProjFromVREyeParameters(info_->left_eye, head_pose);
  left.eye_type = kLeftEye;
  left.viewport = gfx::Rect(0, 0, info_->left_eye->render_width,
                            info_->left_eye->render_height);
  info.left_eye_model = left;

  CameraModel right =
      CameraModelViewProjFromVREyeParameters(info_->right_eye, head_pose);
  right.eye_type = kRightEye;
  right.viewport = gfx::Rect(info_->left_eye->render_width, 0,
                             info_->right_eye->render_width,
                             info_->right_eye->render_height);
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

void GraphicsDelegateWin::PrepareBufferForContentQuadLayer(
    const gfx::Transform& quad_transform) {
  // Content quad is never ready - unused on desktop.
  NOTREACHED();
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

bool GraphicsDelegateWin::IsContentQuadReady() {
  // The content quad is not used.  If we integrate with the views framework at
  // some point, we may return true later.
  return false;
}

void GraphicsDelegateWin::ResumeContentRendering() {
  NOTREACHED();
}

void GraphicsDelegateWin::BufferBoundsChanged(
    const gfx::Size& content_buffer_size,
    const gfx::Size& overlay_buffer_size) {
  // Allows the browser to specify size, but we just use headset default always.
  NOTREACHED();
}

void GraphicsDelegateWin::GetContentQuadDrawParams(Transform* uv_transform,
                                                   float* border_x,
                                                   float* border_y) {
  NOTREACHED();
}

int GraphicsDelegateWin::GetContentBufferWidth() {
  // Called when rendering AlertDialogs, which we don't do.
  NOTREACHED();
  return 0;
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

void GraphicsDelegateWin::SetFrameDumpFilepathBase(std::string& filepath_base) {
  // We don't support saving filepaths currently.
  // TODO(billorr): Support this if/when needed for tests.
}

}  // namespace vr
