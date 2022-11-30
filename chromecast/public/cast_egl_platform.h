// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_H_
#define CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_H_

namespace chromecast {

struct Size;

// Interface representing all the hardware-specific elements of an Ozone
// implementation for Cast.  Supply an implementation of this interface
// to OzonePlatformCast to create a complete Ozone implementation.
class CastEglPlatform {
 public:
  typedef void* (*GLGetProcAddressProc)(const char* name);
  typedef void* NativeDisplayType;
  typedef void* NativeWindowType;

  virtual ~CastEglPlatform() {}

  // Returns an array of EGL properties, which can be used in any EGL function
  // used to select a display configuration. Note that all properties should be
  // immediately followed by the corresponding desired value and array should be
  // terminated with EGL_NONE. Ownership of the array is not transferred to
  // caller. desired_list contains list of desired EGL properties and values.
  virtual const int* GetEGLSurfaceProperties(const int* desired_list) = 0;

  // InitializeHardware is called once (before creating display type or window)
  // and must return false on failure.
  virtual bool InitializeHardware() = 0;

  // These three are called once after hardware is successfully initialized.
  // The implementation must load the libraries containing EGL and GLES2
  // bindings (return the pointer obtained from dlopen).  It must also supply
  // a function pointer to eglGetProcAddress or equivalent.
  virtual void* GetEglLibrary() = 0;
  virtual void* GetGles2Library() = 0;
  virtual GLGetProcAddressProc GetGLProcAddressProc() = 0;

  // Creates an EGLNativeDisplayType.  Called once when initializing display.
  virtual NativeDisplayType CreateDisplayType(const Size& size) = 0;

  // Creates an EGLNativeWindow using previously-created DisplayType.  Called
  // once when initializing display.
  virtual NativeWindowType CreateWindow(NativeDisplayType display_type,
                                        const Size& size) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_H_
