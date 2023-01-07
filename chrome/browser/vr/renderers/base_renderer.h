// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_BASE_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_BASE_RENDERER_H_

#include "device/vr/gl_bindings.h"

namespace vr {

class BaseRenderer {
 public:
  BaseRenderer(const BaseRenderer&) = delete;
  BaseRenderer& operator=(const BaseRenderer&) = delete;

  virtual ~BaseRenderer();

  virtual void Flush();

 protected:
  BaseRenderer() = default;
  BaseRenderer(const char* vertex_src, const char* fragment_src);

  GLuint program_handle_ = 0;
  GLuint position_handle_ = 0;

  GLuint clip_rect_handle_ = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_BASE_RENDERER_H_
