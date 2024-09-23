// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <unistd.h>

#include <cassert>
#include <iostream>

#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/cast_egl_platform_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/starboard/starboard_buildflags.h"

#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
#include <starboard/window.h>

#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#endif

namespace chromecast {
namespace {

// TODO(b/333131992): remove [[maybe_unused]] after removing the
// REMOVE_STARBOARD_HEADERS build flag.
[[maybe_unused]] constexpr char kGraphicsLibraryName[] = "libGL_starboard.so";

// Starboard CastEglPlatform implementation.
class CastEglPlatformStarboard : public CastEglPlatform {
 public:
  CastEglPlatformStarboard()
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
      : sb_adapter_(CastStarboardApiAdapter::GetInstance())
#endif
  {
  }

  const int* GetEGLSurfaceProperties(const int* desired) override {
    return desired;
  }

  ~CastEglPlatformStarboard() override {}

  bool InitializeHardware() override {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    return false;
#else
    if (!sb_adapter_->EnsureInitialized()) {
      return false;
    }
    graphics_lib_ =
        dlopen(kGraphicsLibraryName, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if (!graphics_lib_) {
      std::cerr << "Failed to dlopen " << kGraphicsLibraryName << std::endl;
      return false;
    }

    get_proc_address_ = reinterpret_cast<GLGetProcAddressProc>(
        dlsym(graphics_lib_, "Sb_eglGetProcAddress"));

    if (!get_proc_address_) {
      std::cerr << "Failed to dlsym Sb_eglGetProcAddress from "
                << kGraphicsLibraryName << std::endl;
      return false;
    }
    return true;
#endif
  }

  void* GetEglLibrary() override { return nullptr; }

  void* GetGles2Library() override { return nullptr; }

  GLGetProcAddressProc GetGLProcAddressProc() override {
    return get_proc_address_;
  }

  NativeDisplayType CreateDisplayType(const Size& size) override {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    return nullptr;
#else
    // TODO(b/334138792): The need to create a window before getting the display
    // is an implementation detail. Luckily for us, created windows are not
    // visible and GlOzoneEglCast always couples the call to CreateDisplayType
    // and CreateWindow, so there is no downside to creating |window_| early.
    if (!SbWindowIsValid(window_)) {
      SbWindowOptions options{};
      options.name = "cast";
      options.size.width = size.width;
      options.size.height = size.height;
      window_ = sb_adapter_->GetWindow(&options);
    }

    NativeDisplayType ndt = reinterpret_cast<NativeDisplayType>(
        sb_adapter_->GetEglNativeDisplayType());
    return ndt;
#endif
  }

  NativeWindowType CreateWindow(NativeDisplayType display_type,
                                const Size& size) override {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    return nullptr;
#else
    assert(SbWindowIsValid(window_));
    auto* result =
        static_cast<NativeWindowType>(SbWindowGetPlatformHandle(window_));

    return result;
#endif
  }

 private:
  GLGetProcAddressProc get_proc_address_ = nullptr;
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  void* graphics_lib_ = nullptr;
  SbWindow window_ = nullptr;
  CastStarboardApiAdapter* sb_adapter_;
#endif
};
}  // namespace

CastEglPlatform* CastEglPlatformShlib::Create(
    const std::vector<std::string>& argv) {
  return new CastEglPlatformStarboard();
}

}  // namespace chromecast
