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

namespace {

// Context attributes that ui/gl/gl_context_egl.cc can pass to eglCreateContext
// but that are defined only by ANGLE or Chromium extensions, so a native
// Starboard EGL implementation will not recognise them.
//
// These are spelled numerically because the defining headers
// (eglext_angle.h) are not available here; gl_context_egl.cc does the same for
// EGL_CONTEXT_PASSTHROUGH_SHADERS_ANGLE. The values are checked against
// third_party/angle/include/EGL/eglext_angle.h.
//
// Every attribute in this list falls in 0x33ac..0x34f8, and every standard,
// KHR, EXT, IMG and NV attribute that gl_context_egl.cc can pass falls below
// 0x33a0. Listing them individually rather than filtering that numeric range
// keeps a future standard attribute in the same range from being dropped
// silently.
constexpr EGLint kAngleOnlyContextAttributes[] = {
    0x33ac,  // EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE
    0x33ad,  // EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM
    0x33af,  // EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE
    0x3452,  // EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE
    0x3453,  // EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE
    0x345f,  // EGL_EXTENSIONS_ENABLED_ANGLE
    0x3463,  // EGL_CONTEXT_PASSTHROUGH_SHADERS_ANGLE
    0x3481,  // EGL_CONTEXT_VIRTUALIZATION_GROUP_ANGLE
    0x3482,  // EGL_POWER_PREFERENCE_ANGLE
    0x3483,  // EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE
    0x348d,  // EGL_DISPLAY_SEMAPHORE_SHARE_GROUP_ANGLE
    0x348e,  // EGL_EXTERNAL_CONTEXT_ANGLE
    0x34f8,  // EGL_CONTEXT_HARDENED_ANGLE
};

bool IsAngleOnlyContextAttribute(EGLint attribute) {
  // Not an ANGLE extension, but some Starboard implementations reject it and
  // fail context creation, so it is dropped for the same reason.
  if (attribute == EGL_CONTEXT_OPENGL_NO_ERROR_KHR) {
    return true;
  }
  for (const EGLint angle_attribute : kAngleOnlyContextAttributes) {
    if (attribute == angle_attribute) {
      return true;
    }
  }
  return false;
}

}  // namespace

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

    // Filter out attributes that only ANGLE understands. A native Starboard
    // EGL implementation fails context creation outright when it is handed an
    // attribute it does not recognise, rather than ignoring it.
    if (IsAngleOnlyContextAttribute(*attribute_key)) {
      continue;
    }

    filtered_attributes.push_back(*attribute_key);
    filtered_attributes.push_back(*attribute_value);
  }
  // eglCreateContext requires the list to be EGL_NONE terminated. The loop
  // above can exit without copying the terminator, either because the caller's
  // list was itself malformed or because the terminator was the final entry of
  // an odd-length list. Reading past the end of the list would be an out of
  // bounds read inside the EGL implementation, so terminate it here.
  //
  // A null attrib_list is valid and distinct from an empty one, so it is passed
  // through untouched.
  if (attrib_list &&
      (filtered_attributes.empty() || filtered_attributes.back() != EGL_NONE)) {
    filtered_attributes.push_back(EGL_NONE);
  }
  const EGLint* final_attribs =
      attrib_list ? filtered_attributes.data() : nullptr;
  return SbGetEglInterface()->eglCreateContext(dpy, config, share_context,
                                               final_attribs);
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
  if (!procname) {
    return nullptr;
  }
  // First, look up an "Sb_" prefixed function in this shared library (which may
  // be opened with RTLD_LOCAL) or the global scope. If that fails, perform an
  // un-prefixed lookup via Starboard's EGL interface.
  const std::string prefixed_name = std::string("Sb_") + procname;
  static void* const self_handle = []() -> void* {
    Dl_info info = {};
    if (dladdr(reinterpret_cast<const void*>(&Sb_eglGetProcAddress), &info) &&
        info.dli_fname) {
      void* handle = dlopen(info.dli_fname, RTLD_NOLOAD | RTLD_LAZY);
      if (handle) {
        // RTLD_NOLOAD increments the DSO reference count; decrement it so
        // callers that later dlclose() this library can still unload it.
        dlclose(handle);
      }
      return handle;
    }
    return nullptr;
  }();
  __eglMustCastToProperFunctionPointerType addr = nullptr;
  if (self_handle) {
    addr = reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
        dlsym(self_handle, prefixed_name.c_str()));
  }
  if (!addr) {
    addr = reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
        dlsym(RTLD_DEFAULT, prefixed_name.c_str()));
  }
  if (!addr) {
    if (const SbEglInterface* egl = SbGetEglInterface();
        egl && egl->eglGetProcAddress) {
      addr = egl->eglGetProcAddress(procname);
    }
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
