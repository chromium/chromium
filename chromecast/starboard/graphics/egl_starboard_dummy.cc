// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A dummy implementation of egl_starboard.h. This can be used to compile
// without starboard headers. It should never be used in production.
//
// TODO(b/333131992): remove this

#include "chromecast/starboard/graphics/egl_starboard.h"

extern "C" {

EGLBoolean Sb_eglChooseConfig(EGLDisplay dpy,
                              const EGLint* attrib_list,
                              EGLConfig* configs,
                              EGLint config_size,
                              EGLint* num_config) {
  return 0;
}

EGLBoolean Sb_eglCopyBuffers(EGLDisplay dpy,
                             EGLSurface surface,
                             EGLNativePixmapType target) {
  return 0;
}

EGLContext Sb_eglCreateContext(EGLDisplay dpy,
                               EGLConfig config,
                               EGLContext share_context,
                               const EGLint* attrib_list) {
  return nullptr;
}

EGLSurface Sb_eglCreatePbufferSurface(EGLDisplay dpy,
                                      EGLConfig config,
                                      const EGLint* attrib_list) {
  return nullptr;
}

EGLSurface Sb_eglCreatePixmapSurface(EGLDisplay dpy,
                                     EGLConfig config,
                                     EGLNativePixmapType pixmap,
                                     const EGLint* attrib_list) {
  return nullptr;
}

EGLSurface Sb_eglCreateWindowSurface(EGLDisplay dpy,
                                     EGLConfig config,
                                     EGLNativeWindowType win,
                                     const EGLint* attrib_list) {
  return nullptr;
}

EGLBoolean Sb_eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
  return 0;
}

EGLBoolean Sb_eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
  return 0;
}

EGLBoolean Sb_eglGetConfigAttrib(EGLDisplay dpy,
                                 EGLConfig config,
                                 EGLint attribute,
                                 EGLint* value) {
  return 0;
}

EGLBoolean Sb_eglGetConfigs(EGLDisplay dpy,
                            EGLConfig* configs,
                            EGLint config_size,
                            EGLint* num_config) {
  return 0;
}

EGLDisplay Sb_eglGetCurrentDisplay(void) {
  return nullptr;
}

EGLSurface Sb_eglGetCurrentSurface(EGLint readdraw) {
  return nullptr;
}

EGLDisplay Sb_eglGetDisplay(EGLNativeDisplayType display_id) {
  return nullptr;
}

EGLint Sb_eglGetError(void) {
  return 0;
}

__eglMustCastToProperFunctionPointerType Sb_eglGetProcAddress(
    const char* procname) {
  return nullptr;
}

EGLBoolean Sb_eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
  return 0;
}

EGLBoolean Sb_eglMakeCurrent(EGLDisplay dpy,
                             EGLSurface draw,
                             EGLSurface read,
                             EGLContext ctx) {
  return 0;
}

EGLBoolean Sb_eglQueryContext(EGLDisplay dpy,
                              EGLContext ctx,
                              EGLint attribute,
                              EGLint* value) {
  return 0;
}

const char* Sb_eglQueryString(EGLDisplay dpy, EGLint name) {
  return "";
}

EGLBoolean Sb_eglQuerySurface(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLint attribute,
                              EGLint* value) {
  return 0;
}

EGLBoolean Sb_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  return 0;
}

EGLBoolean Sb_eglTerminate(EGLDisplay dpy) {
  return 0;
}

EGLBoolean Sb_eglWaitGL(void) {
  return 0;
}

EGLBoolean Sb_eglWaitNative(EGLint engine) {
  return 0;
}

EGLBoolean Sb_eglBindTexImage(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLint buffer) {
  return 0;
}

EGLBoolean Sb_eglReleaseTexImage(EGLDisplay dpy,
                                 EGLSurface surface,
                                 EGLint buffer) {
  return 0;
}

EGLBoolean Sb_eglSurfaceAttrib(EGLDisplay dpy,
                               EGLSurface surface,
                               EGLint attribute,
                               EGLint value) {
  return 0;
}

EGLBoolean Sb_eglSwapInterval(EGLDisplay dpy, EGLint interval) {
  return 0;
}

EGLBoolean Sb_eglBindAPI(EGLenum api) {
  return 0;
}

EGLenum Sb_eglQueryAPI(void) {
  return 0;
}

EGLSurface Sb_eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
                                               EGLenum buftype,
                                               EGLClientBuffer buffer,
                                               EGLConfig config,
                                               const EGLint* attrib_list) {
  return nullptr;
}

EGLBoolean Sb_eglWaitClient(void) {
  return 0;
}

EGLBoolean Sb_eglReleaseThread(void) {
  return 0;
}

EGLContext Sb_eglGetCurrentContext(void) {
  return nullptr;
}

}  // extern "C"
