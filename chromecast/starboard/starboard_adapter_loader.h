// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_STARBOARD_ADAPTER_LOADER_H_
#define CHROMECAST_STARBOARD_STARBOARD_ADAPTER_LOADER_H_

#include <memory>

namespace chromecast {
class CastStarboardApiAdapter;

namespace media {
class StarboardApiWrapper;
}  // namespace media

// StarboardAdapterLoader is responsible for dynamically loading the Starboard
// partner library (libcast_starboard_api.so) and the matching Chromecast
// adapter library (libcast_starboard_adapter.so.V) at runtime.
class __attribute__((visibility("default"))) StarboardAdapterLoader {
 public:
  // Abstraction interface for dynamic library loading operations (dlopen,
  // dlsym).
  class LibraryLoader {
   public:
    virtual ~LibraryLoader() = default;
    virtual void* DlOpen(const char* filename, int flags) = 0;
    virtual void* DlSym(void* handle, const char* symbol) = 0;
  };

  // Returns the global instance of the loader.
  static StarboardAdapterLoader* GetInstance();

  // Allows unit tests to create a standalone instance with a custom
  // LibraryLoader.
  static std::unique_ptr<StarboardAdapterLoader> CreateForTesting(
      LibraryLoader* loader);

  virtual ~StarboardAdapterLoader() = default;

  // Returns the loaded CastStarboardApiAdapter, or nullptr if loading failed.
  virtual CastStarboardApiAdapter* GetAdapter() = 0;

  // Returns a new instance of StarboardApiWrapper, or nullptr if loading
  // failed. The caller is responsible for the lifetime of the returned object.
  virtual media::StarboardApiWrapper* CreateMediaWrapper() = 0;

  // Returns the Starboard API version that was loaded, or -1 if none.
  virtual int GetVersion() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_STARBOARD_ADAPTER_LOADER_H_
