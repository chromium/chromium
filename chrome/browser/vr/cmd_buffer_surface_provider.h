// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CMD_BUFFER_SURFACE_PROVIDER_H_
#define CHROME_BROWSER_VR_CMD_BUFFER_SURFACE_PROVIDER_H_

#include "chrome/browser/vr/skia_surface_provider.h"
#include "chrome/browser/vr/vr_ui_export.h"

class GrContext;

namespace vr {

class VR_UI_EXPORT CmdBufferSurfaceProvider : public SkiaSurfaceProvider {
 public:
  CmdBufferSurfaceProvider();
  ~CmdBufferSurfaceProvider() override;

  sk_sp<SkSurface> MakeSurface(const gfx::Size& size) override;
  GLuint FlushSurface(SkSurface* surface, GLuint reuse_texture_id) override;

 private:
  sk_sp<GrContext> gr_context_;
  GLint main_fbo_ = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CMD_BUFFER_SURFACE_PROVIDER_H_
