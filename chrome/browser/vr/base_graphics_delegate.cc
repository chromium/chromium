// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/base_graphics_delegate.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

BaseGraphicsDelegate::BaseGraphicsDelegate() {}

BaseGraphicsDelegate::~BaseGraphicsDelegate() {
  if (curr_context_id_ != kNone)
    contexts_[curr_context_id_]->ReleaseCurrent(surface_.get());
}

bool BaseGraphicsDelegate::Initialize(
    const scoped_refptr<gl::GLSurface>& surface) {
  surface_ = surface;
  share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
  for (auto& context : contexts_) {
    context = gl::init::CreateGLContext(share_group_.get(), surface_.get(),
                                        gl::GLContextAttribs());
    if (!context.get()) {
      LOG(ERROR) << "gl::init::CreateGLContext failed";
      return false;
    }
  }
  return MakeContextCurrent(kMainContext);
}

bool BaseGraphicsDelegate::RunInSkiaContext(base::OnceClosure callback) {
  DCHECK_EQ(curr_context_id_, kMainContext);
  if (!MakeContextCurrent(kSkiaContext))
    return false;
  std::move(callback).Run();
  return MakeContextCurrent(kMainContext);
}

void BaseGraphicsDelegate::SwapSurfaceBuffers() {
  TRACE_EVENT0("gpu", __func__);
  DCHECK(surface_);
  surface_->SwapBuffers(base::DoNothing(), gfx::FrameData());
}

bool BaseGraphicsDelegate::MakeContextCurrent(ContextId context_id) {
  DCHECK(context_id > kNone && context_id < kNumContexts);
  if (curr_context_id_ == context_id)
    return true;
  auto& context = contexts_[context_id];
  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }
  curr_context_id_ = context_id;
  return true;
}

}  // namespace vr
