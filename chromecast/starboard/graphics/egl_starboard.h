// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_GRAPHICS_EGL_STARBOARD_H_
#define CHROMECAST_STARBOARD_GRAPHICS_EGL_STARBOARD_H_

#include <EGL/egl.h>

extern "C" {

// egl 1.0

__attribute__((visibility("default"))) EGLBoolean Sb_eglChooseConfig(
    EGLDisplay dpy,
    const EGLint* attrib_list,
    EGLConfig* configs,
    EGLint config_size,
    EGLint* num_config);

__attribute__((visibility("default"))) EGLBoolean Sb_eglCopyBuffers(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLNativePixmapType target);

__attribute__((visibility("default"))) EGLContext Sb_eglCreateContext(
    EGLDisplay dpy,
    EGLConfig config,
    EGLContext share_context,
    const EGLint* attrib_list);

__attribute__((visibility("default"))) EGLSurface Sb_eglCreatePbufferSurface(
    EGLDisplay dpy,
    EGLConfig config,
    const EGLint* attrib_list);

__attribute__((visibility("default"))) EGLSurface Sb_eglCreatePixmapSurface(
    EGLDisplay dpy,
    EGLConfig config,
    EGLNativePixmapType pixmap,
    const EGLint* attrib_list);

__attribute__((visibility("default"))) EGLSurface Sb_eglCreateWindowSurface(
    EGLDisplay dpy,
    EGLConfig config,
    EGLNativeWindowType win,
    const EGLint* attrib_list);

__attribute__((visibility("default"))) EGLBoolean Sb_eglDestroyContext(
    EGLDisplay dpy,
    EGLContext ctx);

__attribute__((visibility("default"))) EGLBoolean Sb_eglDestroySurface(
    EGLDisplay dpy,
    EGLSurface surface);

__attribute__((visibility("default"))) EGLBoolean Sb_eglGetConfigAttrib(
    EGLDisplay dpy,
    EGLConfig config,
    EGLint attribute,
    EGLint* value);

__attribute__((visibility("default"))) EGLBoolean Sb_eglGetConfigs(
    EGLDisplay dpy,
    EGLConfig* configs,
    EGLint config_size,
    EGLint* num_config);

__attribute__((visibility("default"))) EGLDisplay Sb_eglGetCurrentDisplay(void);

__attribute__((visibility("default"))) EGLSurface Sb_eglGetCurrentSurface(
    EGLint readdraw);

__attribute__((visibility("default"))) EGLDisplay Sb_eglGetDisplay(
    EGLNativeDisplayType display_id);

__attribute__((visibility("default"))) __eglMustCastToProperFunctionPointerType
Sb_eglGetProcAddress(const char* procname);

__attribute__((visibility("default"))) EGLBoolean
Sb_eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor);

__attribute__((visibility("default"))) EGLBoolean Sb_eglMakeCurrent(
    EGLDisplay dpy,
    EGLSurface draw,
    EGLSurface read,
    EGLContext ctx);

__attribute__((visibility("default"))) EGLBoolean Sb_eglQueryContext(
    EGLDisplay dpy,
    EGLContext ctx,
    EGLint attribute,
    EGLint* value);

__attribute__((visibility("default"))) const char* Sb_eglQueryString(
    EGLDisplay dpy,
    EGLint name);

__attribute__((visibility("default"))) EGLBoolean Sb_eglQuerySurface(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint attribute,
    EGLint* value);

__attribute__((visibility("default"))) EGLBoolean Sb_eglSwapBuffers(
    EGLDisplay dpy,
    EGLSurface surface);

__attribute__((visibility("default"))) EGLBoolean Sb_eglTerminate(
    EGLDisplay dpy);

__attribute__((visibility("default"))) EGLBoolean Sb_eglWaitGL(void);

__attribute__((visibility("default"))) EGLBoolean Sb_eglWaitNative(
    EGLint engine);

// egl 1.1

__attribute__((visibility("default"))) EGLBoolean
Sb_eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

__attribute__((visibility("default"))) EGLBoolean
Sb_eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

__attribute__((visibility("default"))) EGLBoolean Sb_eglSurfaceAttrib(
    EGLDisplay dpy,
    EGLSurface surface,
    EGLint attribute,
    EGLint value);

__attribute__((visibility("default"))) EGLBoolean Sb_eglSwapInterval(
    EGLDisplay dpy,
    EGLint interval);

// egl 1.2
__attribute__((visibility("default"))) EGLBoolean Sb_eglBindAPI(EGLenum api);

__attribute__((visibility("default"))) EGLenum Sb_eglQueryAPI(void);

__attribute__((visibility("default"))) EGLSurface
Sb_eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
                                    EGLenum buftype,
                                    EGLClientBuffer buffer,
                                    EGLConfig config,
                                    const EGLint* attrib_list);

__attribute__((visibility("default"))) EGLBoolean Sb_eglReleaseThread(void);

__attribute__((visibility("default"))) EGLBoolean Sb_eglWaitClient(void);

// egl 1.3
// does not define new function prototypes.

// egl 1.4

__attribute__((visibility("default"))) EGLContext Sb_eglGetCurrentContext(void);

}  // extern "C"

#endif  // CHROMECAST_STARBOARD_GRAPHICS_EGL_STARBOARD_H_
