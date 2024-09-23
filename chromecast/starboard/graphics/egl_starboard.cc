// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/graphics/egl_starboard.h"

#include <dlfcn.h>
#include <starboard/egl.h>
#include <starboard/gles.h>
#include <stdio.h>

#include <cstdlib>
#include <string>

extern "C" {

constexpr char kSupportedExtensions[] = "EGL_EXT_client_extensions";

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
  return SbGetEglInterface()->eglCreateContext(dpy, config, share_context,
                                               attrib_list);
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
  if (dpy == EGL_NO_DISPLAY && name == EGL_EXTENSIONS) {
    // Report that we do not support any additional client extensions. See
    // https://registry.khronos.org/EGL/sdk/docs/man/html/eglQueryString.xhtml
    // for more details about the eglQueryString API.
    //
    // If any ANGLE platforms were returned here, chromium would attempt to use
    // an ANGLE ozone implementation rather than the EGL implementation
    // supported by cast.
    return kSupportedExtensions;
  }
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
