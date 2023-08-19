// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CMD_BUFFER_SURFACE_PROVIDER_H_
#define CHROME_BROWSER_VR_CMD_BUFFER_SURFACE_PROVIDER_H_

#include "chrome/browser/vr/skia_surface_provider.h"
#include "chrome/browser/vr/vr_ui_export.h"

class GrDirectContext;

namespace vr {

class VR_UI_EXPORT CmdBufferSurfaceProvider : public SkiaSurfaceProvider {
 public:
  CmdBufferSurfaceProvider();
  ~CmdBufferSurfaceProvider() override;

  std::unique_ptr<Texture> CreateTextureWithSkia(
      const gfx::Size& size,
      base::FunctionRef<void(SkCanvas*)> paint) override;

 private:
  sk_sp<GrDirectContext> gr_context_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CMD_BUFFER_SURFACE_PROVIDER_H_
