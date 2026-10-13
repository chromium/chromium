// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/starboard_adapter_loader.h"

#include <dlfcn.h>

#include <set>

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "chromecast/starboard/starboard_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

class MockLibraryLoader : public StarboardAdapterLoader::LibraryLoader {
 public:
  void SetPartnerSymbols(bool has_sb_main,
                         bool has_sb_memory_allocate,
                         bool has_sb16_symbols) {
    has_sb_main_ = has_sb_main;
    has_sb_memory_allocate_ = has_sb_memory_allocate;
    has_sb16_symbols_ = has_sb16_symbols;
    is_null_ = false;
  }

  void SetAdapterSymbols(void* create_adapter_fn,
                         void* create_wrapper_fn,
                         void* release_fn) {
    create_adapter_fn_ = create_adapter_fn;
    create_wrapper_fn_ = create_wrapper_fn;
    release_fn_ = release_fn;
  }

  void SetNull() { is_null_ = true; }

  void SetUnavailableAdapters(const std::vector<std::string>& unavailable) {
    unavailable_adapters_ = unavailable;
  }

  void SetUnavailablePartnerLibs(const std::vector<std::string>& unavailable) {
    unavailable_partner_libs_ = unavailable;
  }

  const std::vector<std::string>& opened_adapters() const {
    return opened_adapters_;
  }

  const std::vector<std::string>& opened_partner_libs() const {
    return opened_partner_libs_;
  }

  const std::vector<std::string>& attempted_adapters() const {
    return attempted_adapters_;
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
    if (name.find("libcast_starboard_adapter.so") != std::string::npos) {
      attempted_adapters_.push_back(name);
      for (const auto& unavail : unavailable_adapters_) {
        if (name == unavail) {
          return nullptr;
        }
      }
      opened_adapters_.push_back(name);
      return reinterpret_cast<void*>(0x5678);
    }
    return nullptr;
  }

  void* DlSym(void* handle, const char* symbol) override {
    if (handle == reinterpret_cast<void*>(0x1234)) {
      if (has_sb_main_ && std::string(symbol) == "SbRunStarboardMain") {
        return reinterpret_cast<void*>(0x9ABC);
      }
      if (has_sb_memory_allocate_ &&
          std::string(symbol) == "SbMemoryAllocate") {
        return reinterpret_cast<void*>(0x1515);
      }
      if (has_sb16_symbols_ &&
          (std::string(symbol) == "kSbCanMapExecutableMemory" ||
           std::string(symbol) == "SbThreadGetPriority")) {
        return reinterpret_cast<void*>(0x1616);
      }
    }
    if (handle == reinterpret_cast<void*>(0x5678)) {
      if (std::string(symbol) == "CreateCastStarboardApiAdapter") {
        return create_adapter_fn_;
      }
      if (std::string(symbol) == "CreateStarboardApiWrapper") {
        return create_wrapper_fn_;
      }
      if (std::string(symbol) == "ReleaseCastStarboardApiAdapter") {
        return release_fn_;
      }
    }
    return nullptr;
  }

 private:
  bool is_null_ = false;
  bool has_sb_main_ = false;
  bool has_sb_memory_allocate_ = false;
  bool has_sb16_symbols_ = false;
  void* create_adapter_fn_ = nullptr;
  void* create_wrapper_fn_ = nullptr;
  void* release_fn_ = nullptr;
  std::vector<std::string> unavailable_partner_libs_;
  std::vector<std::string> unavailable_adapters_;
  std::vector<std::string> opened_partner_libs_;
  std::vector<std::string> opened_adapters_;
  std::vector<std::string> attempted_partner_libs_;
  std::vector<std::string> attempted_adapters_;
};

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion17) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 17);
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(),
            "libcast_starboard_adapter.so.17");
}

TEST(StarboardAdapterLoaderTest,
     FallsBackToPriorAdapterWhenExactVersionMissing) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  // Simulate adapter .17 missing on disk.
  loader.SetUnavailableAdapters({"libcast_starboard_adapter.so.17"});

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 17);
  // Should gracefully fall back to .16.
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(),
            "libcast_starboard_adapter.so.16");
}

TEST(StarboardAdapterLoaderTest, FallsBackToAdapter15When17And16Missing) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  // Simulate both .17 and .16 missing on disk.
  loader.SetUnavailableAdapters(
      {"libcast_starboard_adapter.so.17", "libcast_starboard_adapter.so.16"});

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 17);
  // Should gracefully fall back to .15.
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(),
            "libcast_starboard_adapter.so.15");
}

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion16) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/true);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 16);
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(),
            "libcast_starboard_adapter.so.16");
}

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion15) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/true,
                           /*has_sb16_symbols=*/false);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 15);
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(),
            "libcast_starboard_adapter.so.15");
}

TEST(StarboardAdapterLoaderTest, ProbesStarboardVersion14) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/false,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 14);
  ASSERT_FALSE(loader.opened_adapters().empty());
  // Version suffixes (.so.15+) were introduced in Starboard 15; Starboard 14
  // and earlier use the unversioned libcast_starboard_adapter.so name.
  EXPECT_EQ(loader.opened_adapters().front(), "libcast_starboard_adapter.so");
}

TEST(StarboardAdapterLoaderTest,
     FallsBackToUnversionedAdapterWhenVersionedAdaptersMissing) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  loader.SetUnavailableAdapters({"libcast_starboard_adapter.so.17",
                                 "libcast_starboard_adapter.so.16",
                                 "libcast_starboard_adapter.so.15"});

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 17);
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(), "libcast_starboard_adapter.so");
  const std::vector<std::string> kExpectedAttempts = {
      "libcast_starboard_adapter.so.17",
      "libcast_starboard_adapter.so.16",
      "libcast_starboard_adapter.so.15",
      "libcast_starboard_adapter.so",
  };
  EXPECT_EQ(loader.attempted_adapters(), kExpectedAttempts);
}

TEST(StarboardAdapterLoaderTest,
     Starboard14FallsBackToVersioned14AdapterBeforeNewerAdapters) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/false,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  loader.SetUnavailableAdapters({"libcast_starboard_adapter.so"});

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 14);
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(),
            "libcast_starboard_adapter.so.14");
  const std::vector<std::string> kExpectedAttempts = {
      "libcast_starboard_adapter.so",
      "libcast_starboard_adapter.so.14",
  };
  EXPECT_EQ(loader.attempted_adapters(), kExpectedAttempts);
}

TEST(StarboardAdapterLoaderTest,
     FallsBackToVersionedPartnerLibraryWhenUnversionedMissing) {
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/true);
  loader.SetUnavailablePartnerLibs(
      {"libcast_starboard_api.so", "libcast_starboard_api.so.17"});

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), 16);
  ASSERT_FALSE(loader.opened_partner_libs().empty());
  EXPECT_EQ(loader.opened_partner_libs().front(),
            "libcast_starboard_api.so.16");
  const std::vector<std::string> kExpectedPartnerAttempts = {
      "libcast_starboard_api.so",
      "libcast_starboard_api.so.17",
      "libcast_starboard_api.so.16",
  };
  EXPECT_EQ(loader.attempted_partner_libs(), kExpectedPartnerAttempts);
  ASSERT_FALSE(loader.opened_adapters().empty());
  EXPECT_EQ(loader.opened_adapters().front(),
            "libcast_starboard_adapter.so.16");
}

TEST(StarboardAdapterLoaderTest, HandlesMissingStarboardSymbolsGracefully) {
  MockLibraryLoader loader;
  loader.SetNull();
  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetVersion(), -1);
  EXPECT_EQ(instance->GetAdapter(), nullptr);
}

TEST(StarboardAdapterLoaderTest, SingletonAccessInstanceIsNotNull) {
  StarboardAdapterLoader* loader = StarboardAdapterLoader::GetInstance();
  EXPECT_NE(loader, nullptr);
}

namespace {
bool g_mock_release_called = false;
void MockReleaseAdapterCallback() {
  g_mock_release_called = true;
}

CastStarboardApiAdapter* MockCreateAdapter() {
  return reinterpret_cast<CastStarboardApiAdapter*>(0xABCD);
}

media::StarboardApiWrapper* MockCreateWrapper() {
  return nullptr;
}
}  // namespace

TEST(StarboardAdapterLoaderTest, RegistersAtExitReleaseTask) {
  base::ShadowingAtExitManager shadow_at_exit;
  g_mock_release_called = false;
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  loader.SetAdapterSymbols(
      reinterpret_cast<void*>(&MockCreateAdapter),
      reinterpret_cast<void*>(&MockCreateWrapper),
      reinterpret_cast<void*>(&MockReleaseAdapterCallback));

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_NE(instance->GetAdapter(), nullptr);
  EXPECT_FALSE(g_mock_release_called);

  base::AtExitManager::ProcessCallbacksNow();
  EXPECT_TRUE(g_mock_release_called);
}

TEST(StarboardAdapterLoaderTest, HandlesMissingReleaseSymbolGracefully) {
  base::ShadowingAtExitManager shadow_at_exit;
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  loader.SetAdapterSymbols(reinterpret_cast<void*>(&MockCreateAdapter),
                           reinterpret_cast<void*>(&MockCreateWrapper),
                           /*release_fn=*/nullptr);

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_NE(instance->GetAdapter(), nullptr);

  // Should not crash when processing exit callbacks if release_fn was null.
  base::AtExitManager::ProcessCallbacksNow();
}

TEST(StarboardAdapterLoaderTest, DoesNotRegisterReleaseWhenAdapterNull) {
  base::ShadowingAtExitManager shadow_at_exit;
  g_mock_release_called = false;
  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  loader.SetAdapterSymbols(
      /*create_adapter_fn=*/nullptr,
      reinterpret_cast<void*>(&MockCreateWrapper),
      reinterpret_cast<void*>(&MockReleaseAdapterCallback));

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(instance->GetAdapter(), nullptr);

  base::AtExitManager::ProcessCallbacksNow();
  EXPECT_FALSE(g_mock_release_called);
}

class FakeLifecycleAdapter : public CastStarboardApiAdapter {
 public:
  void Subscribe(void* context,
                 void (*callback)(void* context,
                                  const SbEvent* event)) override {
    subscribers_.insert(context);
  }

  void Unsubscribe(void* context) override {
    subscribers_.erase(context);
    if (released_ && subscribers_.empty()) {
      stopped_ = true;
    }
  }

  void Release() {
    released_ = true;
    if (subscribers_.empty()) {
      stopped_ = true;
    }
  }

  bool IsStopped() const { return stopped_; }
  bool IsReleased() const { return released_; }
  size_t SubscriberCount() const { return subscribers_.size(); }

  SbEglNativeDisplayType GetEglNativeDisplayType() override {
    return static_cast<SbEglNativeDisplayType>(0);
  }
  SbWindow GetWindow(const SbWindowOptions*) override { return nullptr; }
  bool SbWindowIsValid(SbWindow) override { return false; }
  void* SbWindowGetPlatformHandle(SbWindow) override { return nullptr; }
  const void* GetExtension(const char*) override { return nullptr; }

 private:
  std::set<void*> subscribers_;
  bool released_ = false;
  bool stopped_ = false;
};

FakeLifecycleAdapter* g_fake_lifecycle_adapter = nullptr;

CastStarboardApiAdapter* MockCreateLifecycleAdapter() {
  return g_fake_lifecycle_adapter;
}

void MockReleaseLifecycleAdapter() {
  if (g_fake_lifecycle_adapter) {
    g_fake_lifecycle_adapter->Release();
  }
}

TEST(StarboardAdapterLoaderTest,
     ImmediateUnsubscriptionAllowsCleanShutdownOnExit) {
  base::ShadowingAtExitManager shadow_at_exit;
  FakeLifecycleAdapter adapter;
  g_fake_lifecycle_adapter = &adapter;

  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  loader.SetAdapterSymbols(
      reinterpret_cast<void*>(&MockCreateLifecycleAdapter),
      reinterpret_cast<void*>(&MockCreateWrapper),
      reinterpret_cast<void*>(&MockReleaseLifecycleAdapter));

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  auto* loaded_adapter = instance->GetAdapter();
  ASSERT_NE(loaded_adapter, nullptr);

  // Component (like CastEglPlatformStarboard with fix) subscribes and
  // immediately unsubscribes.
  void* dummy_context = reinterpret_cast<void*>(0x123);
  loaded_adapter->Subscribe(dummy_context, nullptr);
  EXPECT_EQ(adapter.SubscriberCount(), 1u);
  loaded_adapter->Unsubscribe(dummy_context);
  EXPECT_EQ(adapter.SubscriberCount(), 0u);

  EXPECT_FALSE(adapter.IsReleased());
  EXPECT_FALSE(adapter.IsStopped());

  base::AtExitManager::ProcessCallbacksNow();

  EXPECT_TRUE(adapter.IsReleased());
  EXPECT_TRUE(adapter.IsStopped());
  g_fake_lifecycle_adapter = nullptr;
}

TEST(StarboardAdapterLoaderTest,
     LingeringSubscriptionBlocksShutdownUntilUnsubscribed) {
  base::ShadowingAtExitManager shadow_at_exit;
  FakeLifecycleAdapter adapter;
  g_fake_lifecycle_adapter = &adapter;

  MockLibraryLoader loader;
  loader.SetPartnerSymbols(/*has_sb_main=*/true,
                           /*has_sb_memory_allocate=*/false,
                           /*has_sb16_symbols=*/false);
  loader.SetAdapterSymbols(
      reinterpret_cast<void*>(&MockCreateLifecycleAdapter),
      reinterpret_cast<void*>(&MockCreateWrapper),
      reinterpret_cast<void*>(&MockReleaseLifecycleAdapter));

  auto instance = StarboardAdapterLoader::CreateForTesting(&loader);
  ASSERT_NE(instance, nullptr);
  auto* loaded_adapter = instance->GetAdapter();
  ASSERT_NE(loaded_adapter, nullptr);

  // Component (like CastEglPlatformStarboard before fix) subscribes but does
  // not unsubscribe.
  void* dummy_context = reinterpret_cast<void*>(0x456);
  loaded_adapter->Subscribe(dummy_context, nullptr);
  EXPECT_EQ(adapter.SubscriberCount(), 1u);

  // Process exit begins.
  base::AtExitManager::ProcessCallbacksNow();

  // Release was invoked, but Starboard must NOT stop while subscribers remain.
  EXPECT_TRUE(adapter.IsReleased());
  EXPECT_FALSE(adapter.IsStopped());

  // Once the lingering subscriber finally unregisters, Starboard can stop
  // cleanly.
  loaded_adapter->Unsubscribe(dummy_context);
  EXPECT_EQ(adapter.SubscriberCount(), 0u);
  EXPECT_TRUE(adapter.IsStopped());
  g_fake_lifecycle_adapter = nullptr;
}

TEST(StarboardAdapterLoaderTest,
     VersionedAdapterLibrariesExportCorrectAbiSymbols) {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  GTEST_SKIP() << "The versioned adapter libraries are not built when "
                  "remove_starboard_headers is true.";
#else
  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));

  // Verify that libcast_starboard_adapter.so (used for Starboard 14 and
  // earlier, before the .so.<version> suffix was added in Starboard 15) is
  // compiled with SB_API_VERSION=14 and exports legacy
  // CastStarboardApiInitialize / CastStarboardApiFinalize, without exporting
  // SB15+ symbols (SbRunStarboardMain / SbSystemRequestStop).
  base::FilePath lib14_path =
      exe_dir.AppendASCII("libcast_starboard_adapter.so");
  void* handle14 = dlopen(lib14_path.value().c_str(), RTLD_LAZY | RTLD_LOCAL);
  ASSERT_NE(handle14, nullptr)
      << "Failed to load " << lib14_path.value() << ": " << dlerror();
  EXPECT_NE(dlsym(handle14, "CreateCastStarboardApiAdapter"), nullptr);
  EXPECT_NE(dlsym(handle14, "CreateStarboardApiWrapper"), nullptr);
  EXPECT_NE(dlsym(handle14, "ReleaseCastStarboardApiAdapter"), nullptr);
  EXPECT_NE(dlsym(handle14, "CastStarboardApiInitialize"), nullptr);
  EXPECT_NE(dlsym(handle14, "CastStarboardApiFinalize"), nullptr);
  EXPECT_EQ(dlsym(handle14, "SbRunStarboardMain"), nullptr)
      << "libcast_starboard_adapter.so must not export SbRunStarboardMain";
  EXPECT_EQ(dlsym(handle14, "SbSystemRequestStop"), nullptr)
      << "libcast_starboard_adapter.so must not export SbSystemRequestStop";
  dlclose(handle14);

  // Verify that SB15+ adapter libraries export SbRunStarboardMain and
  // SbSystemRequestStop, and do not export legacy SB14 init/finalize symbols.
  for (const char* lib_name :
       {"libcast_starboard_adapter.so.15", "libcast_starboard_adapter.so.16",
        "libcast_starboard_adapter.so.17"}) {
    base::FilePath lib_path = exe_dir.AppendASCII(lib_name);
    void* handle = dlopen(lib_path.value().c_str(), RTLD_LAZY | RTLD_LOCAL);
    ASSERT_NE(handle, nullptr)
        << "Failed to load " << lib_path.value() << ": " << dlerror();
    EXPECT_NE(dlsym(handle, "CreateCastStarboardApiAdapter"), nullptr)
        << "Missing CreateCastStarboardApiAdapter in " << lib_name;
    EXPECT_NE(dlsym(handle, "CreateStarboardApiWrapper"), nullptr)
        << "Missing CreateStarboardApiWrapper in " << lib_name;
    EXPECT_NE(dlsym(handle, "ReleaseCastStarboardApiAdapter"), nullptr)
        << "Missing ReleaseCastStarboardApiAdapter in " << lib_name;
    EXPECT_NE(dlsym(handle, "SbRunStarboardMain"), nullptr)
        << "Missing SbRunStarboardMain in " << lib_name;
    EXPECT_NE(dlsym(handle, "SbSystemRequestStop"), nullptr)
        << "Missing SbSystemRequestStop in " << lib_name;
    EXPECT_EQ(dlsym(handle, "CastStarboardApiInitialize"), nullptr)
        << "Unexpected CastStarboardApiInitialize in " << lib_name;
    EXPECT_EQ(dlsym(handle, "CastStarboardApiFinalize"), nullptr)
        << "Unexpected CastStarboardApiFinalize in " << lib_name;
    dlclose(handle);
  }

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

// Drives Subscribe()/Unsubscribe() through the real, dlopen'ed
// libcast_starboard_adapter.so.V. Every other test in this file either mocks
// the loader or links CastStarboardApiAdapterImpl straight into the test
// executable, so this is the only coverage of the adapter running from inside
// the shared library. That distinction matters: the library statically links
// its own //base with hidden visibility, so reaching for a process-wide //base
// singleton from adapter code (for example base::AtExitManager) resolves to
// the library's private, never-initialized copy and crashes.
//
// Teardown (ReleaseCastStarboardApiAdapter -> ReleaseForAtExit -> Release) is
// additionally exercised at process exit by the AtExitManager task the loader
// registered; a failure there surfaces as a nonzero exit for the batch.
TEST(StarboardAdapterLoaderTest,
     RealAdapterLibraryHandlesSubscribeAndUnsubscribe) {
#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)
  GTEST_SKIP() << "The versioned adapter libraries are not built when "
                  "remove_starboard_headers is true.";
#else
  StarboardAdapterLoader* loader = StarboardAdapterLoader::GetInstance();

  // Pin which library is under test. The default partner stub staged by
  // starboard_cast_dummy:cast_starboard_api probes as SB_API_VERSION (14..17).
  EXPECT_GE(loader->GetVersion(), 14);
  EXPECT_LE(loader->GetVersion(), 17);

  CastStarboardApiAdapter* adapter = loader->GetAdapter();
  ASSERT_NE(adapter, nullptr);

  // Subscribe() starts Starboard, which is where the adapter library does its
  // process-wide initialization work.
  void* context = reinterpret_cast<void*>(0x1234);
  adapter->Subscribe(context, nullptr);
  adapter->Unsubscribe(context);
#endif  // BUILDFLAG(REMOVE_STARBOARD_HEADERS)
}

}  // namespace
}  // namespace chromecast
