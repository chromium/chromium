// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/gl_renderer.h"

#include <memory>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/testapp/vr_test_context.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

namespace {

bool ClearGlErrors() {
  bool errors = false;
  while (glGetError() != GL_NO_ERROR)
    errors = true;
  return errors;
}

}  // namespace

GlRenderer::GlRenderer() : weak_ptr_factory_(this) {}

GlRenderer::~GlRenderer() {}

bool GlRenderer::Initialize(const scoped_refptr<gl::GLSurface>& surface) {
  if (!BaseGraphicsDelegate::Initialize(surface))
    return false;
  PostRenderFrameTask();
  return true;
}

// TODO(crbug/895313): Provide actual implementation for the methods.
void GlRenderer::OnResume() {}
FovRectangles GlRenderer::GetRecommendedFovs() {
  return {{}, {}};
}
float GlRenderer::GetZNear() {
  return 0;
}
RenderInfo GlRenderer::GetRenderInfo(FrameType frame_type,
                                     const gfx::Transform& head_pose) {
  return {};
}
RenderInfo GlRenderer::GetOptimizedRenderInfoForFovs(
    const FovRectangles& fovs) {
  return {};
}
void GlRenderer::InitializeBuffers() {}
void GlRenderer::PrepareBufferForWebXr() {}
void GlRenderer::PrepareBufferForWebXrOverlayElements() {}
void GlRenderer::PrepareBufferForContentQuadLayer(
    const gfx::Transform& quad_transform) {}
void GlRenderer::PrepareBufferForBrowserUi() {}
void GlRenderer::OnFinishedDrawingBuffer() {}
void GlRenderer::GetWebXrDrawParams(int* texture_id, Transform* uv_transform) {}
bool GlRenderer::IsContentQuadReady() {
  return true;
}
void GlRenderer::ResumeContentRendering() {}
void GlRenderer::BufferBoundsChanged(const gfx::Size& content_buffer_size,
                                     const gfx::Size& overlay_buffer_size) {}
void GlRenderer::GetContentQuadDrawParams(Transform* uv_transform,
                                          float* border_x,
                                          float* border_y) {}
int GlRenderer::GetContentBufferWidth() {
  return 0;
}
void GlRenderer::SetFrameDumpFilepathBase(std::string& filepath_base) {}

void GlRenderer::RenderFrame() {
  // Checking and clearing GL errors can be expensive, but we can afford to do
  // this in the testapp as a sanity check.  Clear errors before drawing UI,
  // then assert no new errors after drawing.  See https://crbug.com/768905.
  ClearGlErrors();

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  vr_context_->DrawFrame();

  DCHECK(!ClearGlErrors());

  SwapSurfaceBuffers();
  PostRenderFrameTask();
}

void GlRenderer::PostRenderFrameTask() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GlRenderer::RenderFrame, weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSecondsD(1.0 / 60));
}

}  // namespace vr
