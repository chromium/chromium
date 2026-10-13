// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/starboard_adapter_loader.h"

#include <dlfcn.h>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chromecast/starboard/starboard_adapter_factory.h"

namespace chromecast {

namespace {

const char kPartnerLibName[] = "libcast_starboard_api.so";
const char kUnversionedAdapterLibName[] = "libcast_starboard_adapter.so";
const char kAdapterLibPattern[] = "libcast_starboard_adapter.so.%d";

class DefaultLibraryLoader : public StarboardAdapterLoader::LibraryLoader {
 public:
  void* DlOpen(const char* filename, int flags) override {
    return dlopen(filename, flags);
  }
  void* DlSym(void* handle, const char* symbol) override {
    return dlsym(handle, symbol);
  }
};

DefaultLibraryLoader* GetDefaultLibraryLoader() {
  static base::NoDestructor<DefaultLibraryLoader> loader;
  return loader.get();
}

class StarboardAdapterLoaderImpl : public StarboardAdapterLoader {
 public:
  explicit StarboardAdapterLoaderImpl(LibraryLoader* loader = nullptr)
      : loader_(loader ? loader : GetDefaultLibraryLoader()) {
    Load();
  }

  CastStarboardApiAdapter* GetAdapter() override { return adapter_; }

  media::StarboardApiWrapper* CreateMediaWrapper() override {
    if (!create_media_wrapper_fn_) {
      return nullptr;
    }
    return create_media_wrapper_fn_();
  }

  int GetVersion() override { return version_; }

 private:
  void Load() {
    const char* kPartnerLibCandidates[] = {
        kPartnerLibName,
        "libcast_starboard_api.so.17",
        "libcast_starboard_api.so.16",
        "libcast_starboard_api.so.15",
    };
    std::string primary_partner_err;
    for (const char* candidate : kPartnerLibCandidates) {
      partner_handle_ = loader_->DlOpen(candidate, RTLD_LAZY | RTLD_GLOBAL);
      if (partner_handle_) {
        break;
      }
      if (const char* err = dlerror()) {
        if (primary_partner_err.empty() ||
            std::string(err).find("No such file") == std::string::npos) {
          primary_partner_err = err;
        }
      }
    }
    if (!partner_handle_) {
      const char* last_err = dlerror();
      const char* reported_err =
          !primary_partner_err.empty() ? primary_partner_err.c_str() : last_err;
      LOG(ERROR) << "Failed to load Starboard partner library"
                 << (reported_err ? base::StringPrintf(": %s", reported_err)
                                  : ".");
      return;
    }

    version_ = FindStarboardVersion(partner_handle_);
    if (version_ == -1) {
      LOG(ERROR) << "Failed to deduce Starboard version.";
      return;
    }

    // Version suffixes (.so.15+) were introduced in Starboard 15. Starboard 14
    // and earlier use the unversioned libcast_starboard_adapter.so library.
    std::string adapter_lib_name =
        version_ >= 15 ? base::StringPrintf(kAdapterLibPattern, version_)
                       : std::string(kUnversionedAdapterLibName);
    std::string loaded_lib_name = adapter_lib_name;
    adapter_handle_ =
        loader_->DlOpen(adapter_lib_name.c_str(), RTLD_LAZY | RTLD_LOCAL);

    if (!adapter_handle_) {
      const char* dl_err = dlerror();
      std::string primary_err = dl_err ? dl_err : "";

      // Fallback to checking any available versioned or unversioned adapter.
      // For Starboard 14/13, check libcast_starboard_adapter.so.14 before
      // SB15+ adapters since SB14 uses CastStarboardApiInitialize.
      const char* const kFallbacksSb14[] = {
          "libcast_starboard_adapter.so.14", "libcast_starboard_adapter.so.17",
          "libcast_starboard_adapter.so.16", "libcast_starboard_adapter.so.15",
          kUnversionedAdapterLibName,
      };
      const char* const kFallbacksSb15Plus[] = {
          "libcast_starboard_adapter.so.17", "libcast_starboard_adapter.so.16",
          "libcast_starboard_adapter.so.15", kUnversionedAdapterLibName,
          "libcast_starboard_adapter.so.14",
      };
      const auto& fallbacks =
          version_ < 15 ? kFallbacksSb14 : kFallbacksSb15Plus;
      for (const char* fallback : fallbacks) {
        if (adapter_lib_name == fallback) {
          continue;
        }
        adapter_handle_ = loader_->DlOpen(fallback, RTLD_LAZY | RTLD_LOCAL);
        if (adapter_handle_) {
          loaded_lib_name = fallback;
          LOG(INFO) << "Fell back to Starboard adapter library: " << fallback;
          break;
        }
      }

      if (!adapter_handle_) {
        const char* last_err = dlerror();
        const char* reported_err =
            !primary_err.empty() ? primary_err.c_str() : last_err;
        LOG(ERROR) << "Failed to load Starboard adapter library "
                   << adapter_lib_name
                   << (reported_err ? base::StringPrintf(": %s", reported_err)
                                    : ".");
        return;
      }
    }
    LOG(INFO) << "Successfully loaded Starboard adapter library "
              << loaded_lib_name << " (Starboard API version " << version_
              << ")";

    auto* create_adapter_fn =
        reinterpret_cast<decltype(&CreateCastStarboardApiAdapter)>(
            loader_->DlSym(adapter_handle_, "CreateCastStarboardApiAdapter"));
    create_media_wrapper_fn_ =
        reinterpret_cast<decltype(&CreateStarboardApiWrapper)>(
            loader_->DlSym(adapter_handle_, "CreateStarboardApiWrapper"));
    auto* release_adapter_fn =
        reinterpret_cast<decltype(&ReleaseCastStarboardApiAdapter)>(
            loader_->DlSym(adapter_handle_, "ReleaseCastStarboardApiAdapter"));

    if (!create_adapter_fn || !create_media_wrapper_fn_) {
      LOG(ERROR) << "Failed to find factory functions in adapter library.";
      return;
    }

    adapter_ = create_adapter_fn();

    if (adapter_ && release_adapter_fn) {
      base::AtExitManager::RegisterTask(base::BindOnce(release_adapter_fn));
    }
  }

  // Deduces the Starboard API version exposed by `libcast_starboard_api.so`
  // by probing exported C symbols via dlsym instead of inspecting file path
  // strings or compile-time preprocessor flags.
  int FindStarboardVersion(void* partner_handle) {
    if (!partner_handle) {
      return -1;
    }

    // Starboard 15+ deprecates CastStarboardApiInitialize and exports the
    // canonical un-mangled C symbol `SbRunStarboardMain`.
    if (loader_->DlSym(partner_handle, "SbRunStarboardMain") != nullptr) {
      // Starboard 15 retains legacy memory allocation symbols
      // (SbMemoryAllocate).
      if (loader_->DlSym(partner_handle, "SbMemoryAllocate") != nullptr) {
        LOG(INFO) << "Detected Starboard 15 via SbRunStarboardMain with "
                     "SbMemoryAllocate.";
        return 15;
      }

      // Starboard 16 removed SbMemoryAllocate, transitioning toward POSIX, but
      // retained SbThreadGetPriority and/or kSbCanMapExecutableMemory.
      if (loader_->DlSym(partner_handle, "SbThreadGetPriority") != nullptr ||
          loader_->DlSym(partner_handle, "kSbCanMapExecutableMemory") !=
              nullptr) {
        LOG(INFO) << "Detected Starboard 16 via SbRunStarboardMain and "
                     "Starboard 16 footprint.";
        return 16;
      }

      // Starboard 17+ fully removed Starboard thread priority and memory
      // symbols in favor of direct POSIX APIs.
      LOG(INFO) << "Detected Starboard 17 via SbRunStarboardMain (POSIX "
                   "Starboard API).";
      return 17;
    }

    // Legacy Starboard ABI (Starboard 14 and 13)
    LOG(INFO) << "Detected Starboard 14/13 (Legacy Starboard C ABI) -> using "
                 "unversioned adapter (version 14).";
    return 14;
  }

  LibraryLoader* loader_ = nullptr;
  void* partner_handle_ = nullptr;
  void* adapter_handle_ = nullptr;
  int version_ = -1;
  CastStarboardApiAdapter* adapter_ = nullptr;
  decltype(&CreateStarboardApiWrapper) create_media_wrapper_fn_ = nullptr;
};

}  // namespace

StarboardAdapterLoader* StarboardAdapterLoader::GetInstance() {
  static base::NoDestructor<StarboardAdapterLoaderImpl> instance;
  return instance.get();
}

std::unique_ptr<StarboardAdapterLoader>
StarboardAdapterLoader::CreateForTesting(LibraryLoader* loader) {
  return std::make_unique<StarboardAdapterLoaderImpl>(loader);
}

}  // namespace chromecast
