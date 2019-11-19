// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_VR_GL_UTIL_H_
#define CHROME_BROWSER_VR_VR_GL_UTIL_H_

#include <array>
#include <string>

#include "chrome/browser/vr/gl_bindings.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "third_party/skia/include/core/SkColor.h"

#define SHADER(Src) "#version 100\n" #Src
#define OEIE_SHADER(Src) \
  "#version 100\n#extension GL_OES_EGL_image_external : require\n" #Src
#define VOID_OFFSET(x) reinterpret_cast<void*>(x)

namespace gfx {
class Transform;
}  // namespace gfx

namespace vr {

VR_BASE_EXPORT std::array<float, 16> MatrixToGLArray(
    const gfx::Transform& matrix);

// Compile a shader. This is intended for browser-internal shaders only,
// don't use this for user-supplied arbitrary shaders since data is handed
// directly to the GL driver without further sanity checks.
VR_BASE_EXPORT GLuint CompileShader(GLenum shader_type,
                                    const std::string& shader_source,
                                    std::string& error);

// Compile and link a program.
VR_BASE_EXPORT GLuint CreateAndLinkProgram(GLuint vertex_shader_handle,
                                           GLuint fragment_shader_handle,
                                           std::string& error);

// Sets default texture parameters given a texture type.
VR_BASE_EXPORT void SetTexParameters(GLenum texture_type);

// Sets color uniforms given an SkColor.
VR_BASE_EXPORT void SetColorUniform(GLuint handle, SkColor c);

// Sets color uniforms (but not alpha) given an SkColor. The alpha is assumed to
// be 1.0 in this case.
VR_BASE_EXPORT void SetOpaqueColorUniform(GLuint handle, SkColor c);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_VR_GL_UTIL_H_
