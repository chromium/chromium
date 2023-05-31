// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/win/conflicts/module_blocklist_cache_updater.h"
#include "chrome/browser/win/conflicts/module_blocklist_cache_util.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/proto/module_list.pb.h"
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"
#include "chrome/common/chrome_features.h"
#include "chrome/install_static/install_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

// This classes watches the third-party blocking key to detect when the path to
// the freshly created cache is written into the registry.
class ThirdPartyRegistryKeyObserver {
 public:
  ThirdPartyRegistryKeyObserver()
      : registry_key_(HKEY_CURRENT_USER,
                      GetRegistryKeyPath().c_str(),
                      KEY_CREATE_SUB_KEY | KEY_READ | KEY_NOTIFY) {}

  ThirdPartyRegistryKeyObserver(const ThirdPartyRegistryKeyObserver&) = delete;
  ThirdPartyRegistryKeyObserver& operator=(
      const ThirdPartyRegistryKeyObserver&) = delete;

  bool StartWatching() {
    return registry_key_.StartWatching(base::BindOnce(
        &ThirdPartyRegistryKeyObserver::OnChange, base::Unretained(this)));
  }

  void WaitForCachePathWritten() {
    if (path_written_)
      return;

    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnChange() {
    if (!registry_key_.HasValue(third_party_dlls::kBlFilePathRegValue))
      return;

    path_written_ = true;

    if (run_loop_quit_closure_)
      std::move(run_loop_quit_closure_).Run();
  }

 private:
  std::wstring GetRegistryKeyPath() {
    return install_static::GetRegistryPath().append(
        third_party_dlls::kThirdPartyRegKeyName);
  }

  base::win::RegKey registry_key_;

  // Remembers if the path of the cache was written in the registry in case the
  // callback is invoked before WaitForCachePathWritten() was called.
  bool path_written_ = false;

  base::OnceClosure run_loop_quit_closure_;
};

// Creates an empty serialized ModuleList proto in the module list component
// directory and returns its path.
void CreateModuleList(base::FilePath* module_list_path) {
  chrome::conflicts::ModuleList module_list;
  // Include an empty blocklist and allowlist.
  module_list.mutable_blocklist();
  module_list.mutable_allowlist();

  std::string contents;
  ASSERT_TRUE(module_list.SerializeToString(&contents));

  // Put the module list beside the module blocklist cache.
  *module_list_path = ModuleBlocklistCacheUpdater::GetModuleBlocklistCachePath()
                          .DirName()
                          .Append(FILE_PATH_LITERAL("ModuleList.bin"));

  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  ASSERT_TRUE(base::CreateDirectory(module_list_path->DirName()));
  ASSERT_TRUE(base::WriteFile(*module_list_path, contents));
}

class ThirdPartyBlockingBrowserTest : public InProcessBrowserTest {
 public:
  ThirdPartyBlockingBrowserTest(const ThirdPartyBlockingBrowserTest&) = delete;
  ThirdPartyBlockingBrowserTest& operator=(
      const ThirdPartyBlockingBrowserTest&) = delete;

 protected:
  ThirdPartyBlockingBrowserTest() = default;
  ~ThirdPartyBlockingBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kThirdPartyModulesBlocking);

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    InProcessBrowserTest::SetUp();
  }

  // Creates a copy of a test DLL into a temp directory that will act as the
  // third-party module and return its path. It can't be located in the output
  // directory because modules in the same directory as chrome.exe are
  // allowlisted in non-official builds.
  void CreateThirdPartyModule(base::FilePath* third_party_module_path) {
    base::FilePath test_dll_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_dll_path));
    test_dll_path =
        test_dll_path.Append(FILE_PATH_LITERAL("conflicts_dll.dll"));
    *third_party_module_path = scoped_temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("third_party_module.dll"));
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    ASSERT_TRUE(base::CopyFile(test_dll_path, *third_party_module_path));
  }

  // Enables the ThirdPartyModulesBlocking feature.
  base::test::ScopedFeatureList scoped_feature_list_;

  registry_util::RegistryOverrideManager registry_override_manager_;

  // Temp directory where the third-party module is located.
  base::ScopedTempDir scoped_temp_dir_;
};

}  // namespace

// This is an integration test for the blocking of third-party modules.
//
// This test makes sure that all the different classes interact together
// correctly to produce a valid module blocklist cache and to write its path in
// the registry.
//
// Note: This doesn't test that the modules are actually blocked on the next
//       browser launch.
IN_PROC_BROWSER_TEST_F(ThirdPartyBlockingBrowserTest,
                       CreateModuleBlocklistCache) {
  // Create the observer early so the change is guaranteed to be observed.
  ThirdPartyRegistryKeyObserver third_party_registry_key_observer;
  ASSERT_TRUE(third_party_registry_key_observer.StartWatching());

  base::RunLoop run_loop;
  ModuleDatabase::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([quit_closure = run_loop.QuitClosure()]() {
        ModuleDatabase* module_database = ModuleDatabase::GetInstance();

        base::FilePath module_list_path;
        ASSERT_NO_FATAL_FAILURE(CreateModuleList(&module_list_path));
        ASSERT_FALSE(module_list_path.empty());

        // Simulate the download of the module list component.
        module_database->third_party_conflicts_manager()->LoadModuleList(
            module_list_path);

        quit_closure.Run();
      }));
  run_loop.Run();

  // Injects the third-party DLL into the process.
  base::FilePath third_party_module_path;
  ASSERT_NO_FATAL_FAILURE(CreateThirdPartyModule(&third_party_module_path));
  ASSERT_FALSE(third_party_module_path.empty());

  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  base::ScopedNativeLibrary dll(third_party_module_path);
  ASSERT_TRUE(dll.is_valid());

  // Now the module blocklist cache will eventually be created and its path
  // written in the registry.
  third_party_registry_key_observer.WaitForCachePathWritten();

  base::FilePath module_blocklist_cache_path =
      ModuleBlocklistCacheUpdater::GetModuleBlocklistCachePath();
  ASSERT_FALSE(module_blocklist_cache_path.empty());
  ASSERT_TRUE(base::PathExists(module_blocklist_cache_path));

  // Now check that the third-party DLL was added to the module blocklist cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  base::MD5Digest md5_digest;
  ASSERT_EQ(ReadResult::kSuccess,
            ReadModuleBlocklistCache(module_blocklist_cache_path, &metadata,
                                     &blocklisted_modules, &md5_digest));

  EXPECT_GE(blocklisted_modules.size(), 1u);
}
