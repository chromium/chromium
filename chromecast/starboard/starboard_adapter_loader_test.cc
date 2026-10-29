// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/starboard_adapter_loader.h"

#include <dlfcn.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/starboard_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#endif

namespace chromecast {
namespace {

class MockLibraryLoader : public StarboardAdapterLoader::LibraryLoader {
 public:
  void SetPartnerSymbols(bool has_sb_main,
                         bool has_sb_memory_allocate,
                         bool has_sb16_symbols,
                         bool has_sb17_symbols = true,
                         bool has_executable_memory_constant = true) {
    has_sb_main_ = has_sb_main;
    has_sb_memory_allocate_ = has_sb_memory_allocate;
    has_sb16_symbols_ = has_sb16_symbols;
    has_sb17_symbols_ = has_sb17_symbols;
    has_executable_memory_constant_ = has_executable_memory_constant;
    is_null_ = false;
  }

  void SetNull() { is_null_ = true; }

  void SetUnavailablePartnerLibs(const std::vector<std::string>& unavailable) {
    unavailable_partner_libs_ = unavailable;
  }

  void SetMissingPartnerSymbols(const std::set<std::string>& missing) {
    missing_partner_symbols_ = missing;
  }

  const std::vector<std::string>& opened_partner_libs() const {
    return opened_partner_libs_;
  }

  const std::vector<std::string>& attempted_partner_libs() const {
    return attempted_partner_libs_;
  }

  void* DlOpen(const char* filename, int flags) override {
    if (is_null_) {
      return nullptr;
    }
    std::string name(filename);
    if (name.find("libcast_starboard_api.so") != std::string::npos) {
      attempted_partner_libs_.push_back(name);
      for (const auto& unavail : unavailable_partner_libs_) {
        if (name == unavail) {
          return nullptr;
        }
      }
      opened_partner_libs_.push_back(name);
      return reinterpret_cast<void*>(0x1234);
    }
    return nullptr;
  }

  void* DlSym(void* handle, const char* symbol) override {
    if (handle == reinterpret_cast<void*>(0x1234)) {
      std::string sym(symbol);
      if (missing_partner_symbols_.count(sym) > 0) {
        return nullptr;
      }
      if (sym == "SbRunStarboardMain") {
        return has_sb_main_ ? reinterpret_cast<void*>(0x9ABC) : nullptr;
      }
      if (sym == "SbMemoryAllocate") {
        return has_sb_memory_allocate_ ? reinterpret_cast<void*>(0x1515)
                                       : nullptr;
      }
      if (sym == "SbCPUFeaturesGet" || sym == "SbTimeZoneGetCurrent") {
        return has_sb16_symbols_ ? reinterpret_cast<void*>(0x1616) : nullptr;
      }
      if (sym == "SbThreadGetPriority") {
        return has_sb17_symbols_ ? reinterpret_cast<void*>(0x1717) : nullptr;
      }
      if (sym == "kSbCanMapExecutableMemory") {
        return has_executable_memory_constant_ ? reinterpret_cast<void*>(0x1818)
                                               : nullptr;
      }
      if (sym == "SbSystemRequestStop" || sym == "SbPlayerWriteSamples" ||
          sym == "SbPlayerSeek" || sym == "SbPlayerGetInfo") {
        return has_sb_main_ ? reinterpret_cast<void*>(0x2000) : nullptr;
      }
      if (sym == "CastStarboardApiInitialize" ||
          sym == "CastStarboardApiFinalize" || sym == "SbPlayerWriteSample2" ||
          sym == "SbPlayerSeek2" || sym == "SbPlayerGetInfo2") {
        return !has_sb_main_ ? reinterpret_cast<void*>(0x2014) : nullptr;
      }
      return reinterpret_cast<void*>(0x2020);
    }
    return nullptr;
  }

 private:
  bool is_null_ = false;
  bool has_sb_main_ = false;
  bool has_sb_memory_allocate_ = false;
  bool has_sb16_symbols_ = false;
  bool has_sb17_symbols_ = true;
  bool has_executable_memory_constant_ = true;
  std::set<std::string> missing_partner_symbols_;
  std::vector<std::string> unavailable_partner_libs_;
  std::vector<std::string> opened_partner_libs_;
  std::vector<std::string> attempted_partner_libs_;
};

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion18) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false,
                           /*has_sb17_symbols=*/false,
                           /*has_executable_memory_constant=*/true);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 18);
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  EXPECT_NE(instance->GetAdapter(), nullptr);
  EXPECT_EQ(CastStarboardApiAdapterImpl::GetInstance()->sb_api_version(), 18);
  std::unique_ptr<media::StarboardApiWrapper> wrapper(
      instance->CreateMediaWrapper());
  EXPECT_NE(wrapper, nullptr);
#endif
}

TEST(StarboardAdapterLoaderTest, FailsWhenRequiredStarboardSymbolMissing) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false,
                           /*has_sb17_symbols=*/false);
  loader.SetMissingPartnerSymbols({"SbPlayerWriteSamples"});

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), -1);
  EXPECT_EQ(instance->GetAdapter(), nullptr);
  EXPECT_EQ(instance->CreateMediaWrapper(), nullptr);
}

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion17) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false,
                           /*has_sb17_symbols=*/true,
                           /*has_executable_memory_constant=*/true);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 17);
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  EXPECT_NE(instance->GetAdapter(), nullptr);
  EXPECT_EQ(CastStarboardApiAdapterImpl::GetInstance()->sb_api_version(), 17);
  std::unique_ptr<media::StarboardApiWrapper> wrapper(
      instance->CreateMediaWrapper());
  EXPECT_NE(wrapper, nullptr);
#endif
}

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion16) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/true);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 16);
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  EXPECT_NE(instance->GetAdapter(), nullptr);
  EXPECT_EQ(CastStarboardApiAdapterImpl::GetInstance()->sb_api_version(), 16);
  std::unique_ptr<media::StarboardApiWrapper> wrapper(
      instance->CreateMediaWrapper());
  EXPECT_NE(wrapper, nullptr);
#endif
}

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion15) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/true,
                           /*has_sb16_symbols=*/false);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 15);
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  EXPECT_NE(instance->GetAdapter(), nullptr);
  EXPECT_EQ(CastStarboardApiAdapterImpl::GetInstance()->sb_api_version(), 15);
  std::unique_ptr<media::StarboardApiWrapper> wrapper(
      instance->CreateMediaWrapper());
  EXPECT_NE(wrapper, nullptr);
#endif
}

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion14) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/false,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 14);
#if !BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  EXPECT_NE(instance->GetAdapter(), nullptr);
  EXPECT_EQ(CastStarboardApiAdapterImpl::GetInstance()->sb_api_version(), 14);
  std::unique_ptr<media::StarboardApiWrapper> wrapper(
      instance->CreateMediaWrapper());
  EXPECT_NE(wrapper, nullptr);
#endif
}

TEST(StarboardAdapterLoaderTest,
     FallsBackToVersionedPartnerLibraryWhenUnversionedMissing) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/true);
  std::vector<std::string> unavailable = {"libcast_starboard_api.so"};
  for (int v = 18; v >= 17; --v) {
    unavailable.push_back("libcast_starboard_api.so." +
                          base::NumberToString(v));
  }
  loader.SetUnavailablePartnerLibs(unavailable);

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 16);
  ASSERT_FALSE(loader.opened_partner_libs().empty());
  EXPECT_EQ(loader.opened_partner_libs().front(),
            "libcast_starboard_api.so.16");
}

TEST(StarboardAdapterLoaderTest, HandlesMissingStarboardSymbolsGracefully) {
  MockLibraryLoader loader;
  loader.SetNull();
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), -1);
  EXPECT_EQ(instance->GetAdapter(), nullptr);
  EXPECT_EQ(instance->CreateMediaWrapper(), nullptr);
}

TEST(StarboardAdapterLoaderTest, SingletonAccessInstanceIsNotNull) {
  StarboardAdapterLoader* loader = StarboardAdapterLoader::GetInstance();
  EXPECT_NE(loader, nullptr);
}

TEST(StarboardAdapterLoaderTest, RegistersAtExitReleaseTask) {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  GTEST_SKIP() << "Starboard adapter is not built when "
                  "remove_starboard_headers is true.";
#else
  base::ShadowingAtExitManager shadow_at_exit;
  CastStarboardApiAdapterImpl* adapter =
      CastStarboardApiAdapterImpl::GetInstance();
  adapter->ResetForTesting();

  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_NE(instance->GetAdapter(), nullptr);
  {
    base::AutoLock autolock(adapter->lock_);
    EXPECT_FALSE(adapter->at_exit_ran_);
  }

  base::AtExitManager::ProcessCallbacksNow();
  {
    base::AutoLock autolock(adapter->lock_);
    EXPECT_TRUE(adapter->at_exit_ran_);
  }
  adapter->ResetForTesting();
#endif  // BUILDFLAG(REMOVE_STARBOARD_HEADERS)
}

TEST(StarboardAdapterLoaderTest, GlStarboardLibraryLoadsAndUnloadsCleanly) {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  GTEST_SKIP() << "libGL_starboard.so Starboard symbols are not built when "
                  "remove_starboard_headers is true.";
#else
  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));

  // Verify that the single version-agnostic libGL_starboard.so loads cleanly
  // with RTLD_LOCAL, exports Sb_eglGetProcAddress, and resolves Sb_egl* and
  // Sb_gl* wrappers against its own DSO handle.
  base::FilePath gl_path = exe_dir.AppendASCII("libGL_starboard.so");
  void* gl_handle = dlopen(gl_path.value().c_str(), RTLD_LAZY | RTLD_LOCAL);
  ASSERT_NE(gl_handle, nullptr)
      << "Failed to load " << gl_path.value() << ": " << dlerror();
  using GetProcAddressFn = void* (*)(const char*);
  auto get_proc_address = reinterpret_cast<GetProcAddressFn>(
      dlsym(gl_handle, "Sb_eglGetProcAddress"));
  ASSERT_NE(get_proc_address, nullptr)
      << "Missing Sb_eglGetProcAddress in libGL_starboard.so";
  EXPECT_NE(get_proc_address("eglGetDisplay"), nullptr);
  EXPECT_NE(get_proc_address("eglGetError"), nullptr);
  EXPECT_NE(get_proc_address("glClearColor"), nullptr);
  EXPECT_EQ(dlclose(gl_handle), 0);
  void* noload_handle =
      dlopen(gl_path.value().c_str(), RTLD_NOLOAD | RTLD_LAZY);
  EXPECT_EQ(noload_handle, nullptr)
      << "libGL_starboard.so remained loaded after dlclose(); "
         "Sb_eglGetProcAddress may have leaked an RTLD_NOLOAD reference count.";
  if (noload_handle) {
    dlclose(noload_handle);
  }
#endif  // BUILDFLAG(REMOVE_STARBOARD_HEADERS)
}

TEST(StarboardAdapterLoaderTest, RealAdapterHandlesSubscribeAndUnsubscribe) {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  GTEST_SKIP() << "Starboard adapter is not built when "
                  "remove_starboard_headers is true.";
#else
  StarboardAdapterLoader* loader = StarboardAdapterLoader::GetInstance();

  // The default partner stub staged by starboard_cast_dummy:cast_starboard_api
  // is compiled at the in-tree SB_API_VERSION (15).
  EXPECT_EQ(loader->GetVersion(), SB_API_VERSION)
      << "Unexpected Starboard adapter version: " << loader->GetVersion();

  CastStarboardApiAdapter* adapter = loader->GetAdapter();
  ASSERT_NE(adapter, nullptr);

  void* context = reinterpret_cast<void*>(0x1234);
  adapter->Subscribe(context, nullptr);
  adapter->Unsubscribe(context);
#endif  // BUILDFLAG(REMOVE_STARBOARD_HEADERS)
}

}  // namespace
}  // namespace chromecast
