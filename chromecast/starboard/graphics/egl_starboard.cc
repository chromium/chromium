// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/graphics/egl_starboard.h"

#include <EGL/eglext.h>  // for EGL_CONTEXT_OPENGL_NO_ERROR_KHR
#include <dlfcn.h>
#include <starboard/egl.h>
#include <starboard/gles.h>
#include <stdio.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "base/compiler_specific.h"  // for UNSAFE_BUFFERS

extern "C" {

EGLBoolean Sb_eglChooseConfig(EGLDisplay dpy,
                              const EGLint* attrib_list,
                              EGLConfig* configs,
                              EGLint config_size,
                              EGLint* num_config) {
  return SbGetEglInterface()->eglChooseConfig(dpy, attrib_list, configs,
                                              config_size, num_config);
}

EGLBoolean Sb_eglCopyBuffers(EGLDisplay dpy,
                             EGLSurface surface,
                             EGLNativePixmapType target) {
  return SbGetEglInterface()->eglCopyBuffers(dpy, surface, target);
}

EGLContext Sb_eglCreateContext(EGLDisplay dpy,
                               EGLConfig config,
                               EGLContext share_context,
                               const EGLint* attrib_list) {
  // Some starboard implementations do not support
  // EGL_CONTEXT_OPENGL_NO_ERROR_KHR; context creation will fail on those
  // platforms if that attribute is specified. So we manually remove that
  // attribute.
  //
  // Per EGL documentation, `attrib_list` contains key value pairs, and is
  // terminated by EGL_NONE (k1, v1, k2, v2, ... kn, vn, EGL_NONE).
  // `attrib_list` may be null.
  std::vector<EGLint> filtered_attributes;
  for (const EGLint* attribute_key = attrib_list; attribute_key != nullptr;
       // SAFETY: Required by the EGL API.
       UNSAFE_BUFFERS(attribute_key += 2)) {
    if (*attribute_key == EGL_CONTEXT_OPENGL_NO_ERROR_KHR) {
      continue;
    }
    // EGL_NONE is used to terminate the array.
    if (*attribute_key == EGL_NONE) {
      filtered_attributes.push_back(EGL_NONE);
      break;
    }

    // SAFETY: Required by the EGL API.
    const EGLint* const attribute_value = UNSAFE_BUFFERS(attribute_key + 1);
    if (attribute_value == nullptr) {
      // Not valid; stop parsing the attributes.
      break;
    }
    filtered_attributes.push_back(*attribute_key);
    filtered_attributes.push_back(*attribute_value);
  }
  return SbGetEglInterface()->eglCreateContext(dpy, config, share_context,
                                               filtered_attributes.data());
}

EGLSurface Sb_eglCreatePbufferSurface(EGLDisplay dpy,
                                      EGLConfig config,
                                      const EGLint* attrib_list) {
  return SbGetEglInterface()->eglCreatePbufferSurface(dpy, config, attrib_list);
}

EGLSurface Sb_eglCreatePixmapSurface(EGLDisplay dpy,
                                     EGLConfig config,
                                     EGLNativePixmapType pixmap,
                                     const EGLint* attrib_list) {
  return SbGetEglInterface()->eglCreatePixmapSurface(dpy, config, pixmap,
                                                     attrib_list);
}

EGLSurface Sb_eglCreateWindowSurface(EGLDisplay dpy,
                                     EGLConfig config,
                                     EGLNativeWindowType win,
                                     const EGLint* attrib_list) {
  return SbGetEglInterface()->eglCreateWindowSurface(dpy, config, win,
                                                     attrib_list);
}

EGLBoolean Sb_eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
  return SbGetEglInterface()->eglDestroyContext(dpy, ctx);
}

EGLBoolean Sb_eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
  return SbGetEglInterface()->eglDestroySurface(dpy, surface);
}

EGLBoolean Sb_eglGetConfigAttrib(EGLDisplay dpy,
                                 EGLConfig config,
                                 EGLint attribute,
                                 EGLint* value) {
  return SbGetEglInterface()->eglGetConfigAttrib(dpy, config, attribute, value);
}

EGLBoolean Sb_eglGetConfigs(EGLDisplay dpy,
                            EGLConfig* configs,
                            EGLint config_size,
                            EGLint* num_config) {
  return SbGetEglInterface()->eglGetConfigs(dpy, configs, config_size,
                                            num_config);
}

EGLDisplay Sb_eglGetCurrentDisplay(void) {
  return SbGetEglInterface()->eglGetCurrentDisplay();
}

EGLSurface Sb_eglGetCurrentSurface(EGLint readdraw) {
  return SbGetEglInterface()->eglGetCurrentSurface(readdraw);
}

EGLDisplay Sb_eglGetDisplay(EGLNativeDisplayType display_id) {
  return SbGetEglInterface()->eglGetDisplay(
      reinterpret_cast<SbEglNativeDisplayType>(display_id));
}

EGLint Sb_eglGetError(void) {
  return SbGetEglInterface()->eglGetError();
}

__eglMustCastToProperFunctionPointerType Sb_eglGetProcAddress(
    const char* procname) {
  // First, look up an "Sb_" prefixed function that has been loaded. If that
  // fails, perform an un-prefixed lookup via starboard.
  const std::string prefixed_name = std::string("Sb_") + procname;
  auto* addr = reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
      dlsym(RTLD_DEFAULT, prefixed_name.c_str()));
  if (!addr) {
    addr = SbGetEglInterface()->eglGetProcAddress(procname);
  }

  return addr;
}

EGLBoolean Sb_eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
  return SbGetEglInterface()->eglInitialize(dpy, major, minor);
}

EGLBoolean Sb_eglMakeCurrent(EGLDisplay dpy,
                             EGLSurface draw,
                             EGLSurface read,
                             EGLContext ctx) {
  return SbGetEglInterface()->eglMakeCurrent(dpy, draw, read, ctx);
}

EGLBoolean Sb_eglQueryContext(EGLDisplay dpy,
                              EGLContext ctx,
                              EGLint attribute,
                              EGLint* value) {
  return SbGetEglInterface()->eglQueryContext(dpy, ctx, attribute, value);
}

const char* Sb_eglQueryString(EGLDisplay dpy, EGLint name) {
  return SbGetEglInterface()->eglQueryString(dpy, name);
}

EGLBoolean Sb_eglQuerySurface(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLint attribute,
                              EGLint* value) {
  return SbGetEglInterface()->eglQuerySurface(dpy, surface, attribute, value);
}

EGLBoolean Sb_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  static const char* const cast_quirk_disable_ui =
      std::getenv("CAST_QUIRK_DISABLE_UI");
  if (cast_quirk_disable_ui && cast_quirk_disable_ui[0] != '0') {
    auto* gles = SbGetGlesInterface();
    gles->glClearColor(0, 0, 0, 0);
    gles->glClear(SB_GL_COLOR_BUFFER_BIT);
  }
  return SbGetEglInterface()->eglSwapBuffers(dpy, surface);
}

EGLBoolean Sb_eglTerminate(EGLDisplay dpy) {
  return SbGetEglInterface()->eglTerminate(dpy);
}

EGLBoolean Sb_eglWaitGL(void) {
  return SbGetEglInterface()->eglWaitGL();
}

EGLBoolean Sb_eglWaitNative(EGLint engine) {
  return SbGetEglInterface()->eglWaitNative(engine);
}

EGLBoolean Sb_eglBindTexImage(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLint buffer) {
  return SbGetEglInterface()->eglBindTexImage(dpy, surface, buffer);
}

EGLBoolean Sb_eglReleaseTexImage(EGLDisplay dpy,
                                 EGLSurface surface,
                                 EGLint buffer) {
  return SbGetEglInterface()->eglReleaseTexImage(dpy, surface, buffer);
}

EGLBoolean Sb_eglSurfaceAttrib(EGLDisplay dpy,
                               EGLSurface surface,
                               EGLint attribute,
                               EGLint value) {
  return SbGetEglInterface()->eglSurfaceAttrib(dpy, surface, attribute, value);
}

EGLBoolean Sb_eglSwapInterval(EGLDisplay dpy, EGLint interval) {
  return SbGetEglInterface()->eglSwapInterval(dpy, interval);
}

EGLBoolean Sb_eglBindAPI(EGLenum api) {
  return SbGetEglInterface()->eglBindAPI(api);
}

EGLenum Sb_eglQueryAPI(void) {
  return SbGetEglInterface()->eglQueryAPI();
}

EGLSurface Sb_eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
                                               EGLenum buftype,
                                               EGLClientBuffer buffer,
                                               EGLConfig config,
                                               const EGLint* attrib_list) {
  return SbGetEglInterface()->eglCreatePbufferFromClientBuffer(
      dpy, buftype, buffer, config, attrib_list);
}

EGLBoolean Sb_eglWaitClient(void) {
  return SbGetEglInterface()->eglWaitClient();
}

EGLBoolean Sb_eglReleaseThread(void) {
  return SbGetEglInterface()->eglReleaseThread();
}

EGLContext Sb_eglGetCurrentContext(void) {
  return SbGetEglInterface()->eglGetCurrentContext();
}

}  // extern "C"
