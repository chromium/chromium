// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/starboard_adapter_loader.h"

#include <dlfcn.h>

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chromecast/starboard/starboard_buildflags.h"

#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#endif

namespace chromecast {

namespace {

const char kPartnerLibName[] = "libcast_starboard_api.so";
const char kPartnerLibPattern[] = "libcast_starboard_api.so.%d";

constexpr int kMinVersionedPartnerLib = 15;
// Maximum Starboard API version currently supported and validated.
constexpr int kMaxProbedPartnerLib = 18;

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

StarboardAdapterLoader::LibraryLoader* g_testing_library_loader = nullptr;

StarboardAdapterLoader::LibraryLoader* GetLibraryLoader() {
  if (g_testing_library_loader) {
    return g_testing_library_loader;
  }
  return GetDefaultLibraryLoader();
}

#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
void ReleaseInProcessStarboardAdapter() {
  CastStarboardApiAdapterImpl::GetInstance()->ReleaseForAtExit();
}
#endif

class StarboardAdapterLoaderImpl : public StarboardAdapterLoader {
 public:
  explicit StarboardAdapterLoaderImpl(LibraryLoader* loader = nullptr)
      : loader_(loader ? loader : GetLibraryLoader()) {
    Load();
  }

  CastStarboardApiAdapter* GetAdapter() override { return adapter_; }

  media::StarboardApiWrapper* CreateMediaWrapper() override {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    return nullptr;
#else
    if (version_ < 0) {
      return nullptr;
    }
    if (version_ >= 15) {
      return media::CreateStarboardApiWrapper15().release();
    }
    return media::CreateStarboardApiWrapper14().release();
#endif
  }

  int GetVersion() override { return version_; }

 private:
  void Load() {
    std::vector<std::string> partner_lib_candidates = {kPartnerLibName};
    for (int v = kMaxProbedPartnerLib; v >= kMinVersionedPartnerLib; --v) {
      partner_lib_candidates.push_back(
          base::StringPrintf(kPartnerLibPattern, v));
    }
    std::string primary_partner_err;
    for (const std::string& candidate : partner_lib_candidates) {
      partner_handle_ =
          loader_->DlOpen(candidate.c_str(), RTLD_LAZY | RTLD_GLOBAL);
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

    int deduced_version = FindStarboardVersion(partner_handle_);
    if (deduced_version == -1 ||
        !VerifyRequiredStarboardSymbols(partner_handle_, deduced_version)) {
      LOG(ERROR) << "Failed to deduce or validate Starboard version.";
      return;
    }
    version_ = deduced_version;

#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
    auto* adapter_impl = CastStarboardApiAdapterImpl::GetInstance();
    adapter_impl->SetSbApiVersion(version_);
    adapter_ = adapter_impl;
    base::AtExitManager::RegisterTask(
        base::BindOnce(&ReleaseInProcessStarboardAdapter));
    LOG(INFO) << "Initialized in-process Starboard adapter (Starboard API "
                 "version "
              << version_ << ")";
#endif
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

      // Starboard 16 removed SbMemoryAllocate, but retained SbCPUFeaturesGet
      // and SbTimeZoneGetCurrent (removed in Starboard 17). Note that
      // kSbCanMapExecutableMemory remains exported in Starboard 16, 17, and 18
      // and must not be used to distinguish Starboard 16 from 17+.
      if (loader_->DlSym(partner_handle, "SbCPUFeaturesGet") != nullptr ||
          loader_->DlSym(partner_handle, "SbTimeZoneGetCurrent") != nullptr) {
        LOG(INFO) << "Detected Starboard 16 via SbRunStarboardMain and "
                     "Starboard 16 footprint.";
        return 16;
      }

      // Starboard 17 removed SbCPUFeaturesGet and SbTimeZoneGetCurrent, but
      // retained SbThreadGetPriority (removed in Starboard 18).
      if (loader_->DlSym(partner_handle, "SbThreadGetPriority") != nullptr) {
        LOG(INFO) << "Detected Starboard 17 via SbRunStarboardMain with "
                     "SbThreadGetPriority.";
        return 17;
      }

      // Starboard 18+ removed SbThreadGetPriority in favor of direct POSIX
      // APIs.
      LOG(INFO) << "Detected Starboard 18 via SbRunStarboardMain (POSIX "
                   "Starboard API).";
      return 18;
    }

    // Legacy Starboard ABI (Starboard 14 and 13)
    LOG(INFO) << "Detected Starboard 14/13 (Legacy Starboard C ABI) -> using "
                 "unversioned adapter (version 14).";
    return 14;
  }

  // Validates at runtime that the partner's Starboard shared library exports
  // the subset of Starboard C functions required by Cast so newer Starboard
  // versions (18+) can safely use the SB17+ adapter without silently falling
  // back to weak no-op stubs.
  bool VerifyRequiredStarboardSymbols(void* partner_handle, int version) {
    static constexpr const char* kCommonRequiredSymbols[] = {
        "SbWindowCreate",
        "SbWindowGetPlatformHandle",
        "SbPlayerCreate",
        "SbPlayerSetBounds",
        "SbPlayerWriteEndOfStream",
        "SbPlayerSetVolume",
        "SbPlayerSetPlaybackRate",
        "SbPlayerDestroy",
        "SbDrmCreateSystem",
        "SbDrmGenerateSessionUpdateRequest",
        "SbDrmUpdateSession",
        "SbDrmCloseSession",
        "SbDrmIsServerCertificateUpdatable",
        "SbDrmUpdateServerCertificate",
        "SbDrmDestroySystem",
        "SbMediaCanPlayMimeAndKeySystem",
        "SbGetEglInterface",
        "SbGetGlesInterface",
    };
    static constexpr const char* kSb15PlusRequiredSymbols[] = {
        "SbRunStarboardMain", "SbSystemRequestStop", "SbPlayerWriteSamples",
        "SbPlayerSeek",       "SbPlayerGetInfo",
    };
    static constexpr const char* kSb14RequiredSymbols[] = {
        "CastStarboardApiInitialize", "CastStarboardApiFinalize",
        "SbPlayerWriteSample2",       "SbPlayerSeek2",
        "SbPlayerGetInfo2",
    };

    for (const char* symbol : kCommonRequiredSymbols) {
      if (loader_->DlSym(partner_handle, symbol) == nullptr) {
        LOG(ERROR) << "Starboard partner library (version " << version
                   << ") is missing required symbol: " << symbol;
        return false;
      }
    }
    if (version >= 15) {
      for (const char* symbol : kSb15PlusRequiredSymbols) {
        if (loader_->DlSym(partner_handle, symbol) == nullptr) {
          LOG(ERROR) << "Starboard 15+ partner library (version " << version
                     << ") is missing required symbol: " << symbol;
          return false;
        }
      }
    } else {
      for (const char* symbol : kSb14RequiredSymbols) {
        if (loader_->DlSym(partner_handle, symbol) == nullptr) {
          LOG(ERROR) << "Legacy Starboard partner library (version " << version
                     << ") is missing required symbol: " << symbol;
          return false;
        }
      }
    }
    return true;
  }

  LibraryLoader* loader_ = nullptr;
  void* partner_handle_ = nullptr;
  int version_ = -1;
  CastStarboardApiAdapter* adapter_ = nullptr;
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
