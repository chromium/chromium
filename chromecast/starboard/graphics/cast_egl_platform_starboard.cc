// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <unistd.h>

#include "base/check.h"
#include "base/logging.h"
#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/cast_egl_platform_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/starboard/starboard_buildflags.h"

#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
#include <starboard/window.h>

#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "chromecast/starboard/starboard_adapter_loader.h"
#endif

namespace chromecast {
namespace {

// TODO(b/333131992): remove [[maybe_unused]] after removing the
// REMOVE_STARBOARD_HEADERS build flag.
[[maybe_unused]] constexpr char kGraphicsLibraryName[] = "libGL_starboard.so";

// Starboard CastEglPlatform implementation.
class CastEglPlatformStarboard : public CastEglPlatform {
 public:
  CastEglPlatformStarboard() = default;

  const int* GetEGLSurfaceProperties(const int* desired) override {
    return desired;
  }

  ~CastEglPlatformStarboard() override {
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    if (graphics_lib_) {
      dlclose(graphics_lib_);
    }
#endif
  }

  bool InitializeHardware() override {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    LOG(FATAL) << "Cannot initialize hardware for Starboard as "
                  "REMOVE_STARBOARD_HEADERS is set.";
#else
    // Ensure the Starboard partner library (libcast_starboard_api.so*) is
    // loaded with RTLD_GLOBAL before dlopening libGL_starboard.so so that
    // SbGetEglInterface and SbGetGlesInterface resolve to the partner's
    // implementation rather than the weak link-time stubs.
    if (!StarboardAdapterLoader::GetInstance()->GetAdapter()) {
      LOG(ERROR) << "Failed to load Starboard adapter before initializing "
                    "graphics hardware.";
      return false;
    }

    graphics_lib_ = dlopen(kGraphicsLibraryName, RTLD_LAZY | RTLD_LOCAL);
    if (!graphics_lib_) {
      const char* err = dlerror();
      LOG(ERROR) << "Failed to dlopen graphics library " << kGraphicsLibraryName
                 << (err ? ": " : "!") << (err ? err : "");
      return false;
    }
    LOG(INFO) << "Successfully dlopened " << kGraphicsLibraryName;

    get_proc_address_ = reinterpret_cast<GLGetProcAddressProc>(
        dlsym(graphics_lib_, "Sb_eglGetProcAddress"));

    if (!get_proc_address_) {
      LOG(ERROR) << "Failed to dlsym Sb_eglGetProcAddress: " << dlerror();
      dlclose(graphics_lib_);
      graphics_lib_ = nullptr;
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
    auto* sb_adapter = CastStarboardApiAdapter::GetInstance();
    if (!sb_adapter->SbWindowIsValid(window_)) {
      LOG(INFO) << "Subscribing to CastStarboardApiAdapter. this=" << this;
      sb_adapter->Subscribe(this, nullptr);
      SbWindowOptions options{};
      options.name = "cast";
      options.size.width = size.width;
      options.size.height = size.height;
      window_ = sb_adapter->GetWindow(&options);
      ndt_ = reinterpret_cast<NativeDisplayType>(
          sb_adapter->GetEglNativeDisplayType());
      LOG(INFO) << "Unsubscribing from CastStarboardApiAdapter. this=" << this;
      sb_adapter->Unsubscribe(this);
    }

    return ndt_;
#endif
  }

  NativeWindowType CreateWindow(NativeDisplayType display_type,
                                const Size& size) override {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    return nullptr;
#else
    auto* sb_adapter = CastStarboardApiAdapter::GetInstance();
    if (!sb_adapter->SbWindowIsValid(window_)) {
      LOG(INFO) << "CreateWindow: subscribing to CastStarboardApiAdapter. this="
                << this;
      sb_adapter->Subscribe(this, nullptr);
      SbWindowOptions options{};
      options.name = "cast";
      options.size.width = size.width;
      options.size.height = size.height;
      window_ = sb_adapter->GetWindow(&options);
      LOG(INFO)
          << "CreateWindow: unsubscribing from CastStarboardApiAdapter. this="
          << this;
      sb_adapter->Unsubscribe(this);
    }
    CHECK(sb_adapter->SbWindowIsValid(window_));
    auto* result = static_cast<NativeWindowType>(
        sb_adapter->SbWindowGetPlatformHandle(window_));

    return result;
#endif
  }

 private:
  GLGetProcAddressProc get_proc_address_ = nullptr;
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  void* graphics_lib_ = nullptr;
  SbWindow window_ = nullptr;
  NativeDisplayType ndt_ = nullptr;
#endif
};
}  // namespace

CastEglPlatform* CastEglPlatformShlib::Create(
    const std::vector<std::string>& argv) {
  return new CastEglPlatformStarboard();
}

}  // namespace chromecast
