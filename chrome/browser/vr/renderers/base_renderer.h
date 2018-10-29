// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_BASE_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_BASE_RENDERER_H_

#include "base/macros.h"
#include "chrome/browser/vr/gl_bindings.h"

namespace vr {

class BaseRenderer {
 public:
  virtual ~BaseRenderer();

  virtual void Flush();

 protected:
  BaseRenderer() = default;
  BaseRenderer(const char* vertex_src, const char* fragment_src);

  GLuint program_handle_ = 0;
  GLuint position_handle_ = 0;

  GLuint clip_rect_handle_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BaseRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_BASE_RENDERER_H_
