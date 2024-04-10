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
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/installed_applications.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/proto/module_list.pb.h"
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

// This class allows to wait until the kIncompatibleApplications preference is
// modified. This can only happen if a new incompatible application is found,
// since the pref starts empty during testing.
//
// Note: The browser process must be initialized before the creation of an
// instance of this class.
class IncompatibleApplicationsObserver {
 public:
  IncompatibleApplicationsObserver() {
    pref_change_registrar_.Init(g_browser_process->local_state());
    pref_change_registrar_.Add(
        prefs::kIncompatibleApplications,
        base::BindRepeating(&IncompatibleApplicationsObserver::
                                OnIncompatibleApplicationsChanged,
                            base::Unretained(this)));
  }

  IncompatibleApplicationsObserver(const IncompatibleApplicationsObserver&) =
      delete;
  IncompatibleApplicationsObserver& operator=(
      const IncompatibleApplicationsObserver&) = delete;

  ~IncompatibleApplicationsObserver() = default;

  // Wait until the kIncompatibleApplications preference is modified.
  void WaitForIncompatibleApplicationsChanged() {
    if (incompatible_applications_changed_)
      return;

    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // Callback for |pref_change_registrar_|.
  void OnIncompatibleApplicationsChanged() {
    incompatible_applications_changed_ = true;

    if (run_loop_quit_closure_)
      std::move(run_loop_quit_closure_).Run();
  }

  bool incompatible_applications_changed_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  base::RepeatingClosure run_loop_quit_closure_;
};

class TestInstalledApplications : public InstalledApplications {
 public:
  // For browser_tests, only search in HKCU, because read/write HKLM
  // may:
  //  1. get interfered by the test bot.
  //  2. break sandbox operation and things fail unpredictably.
  std::vector<std::pair<HKEY, REGSAM>> GenRegistryKeyCombinations()
      const override {
    std::vector<std::pair<HKEY, REGSAM>> registry_key_combinations;
    registry_key_combinations.emplace_back(HKEY_CURRENT_USER, 0);
    return registry_key_combinations;
  }
};

class IncompatibleApplicationsBrowserTest : public InProcessBrowserTest {
 public:
  IncompatibleApplicationsBrowserTest(
      const IncompatibleApplicationsBrowserTest&) = delete;
  IncompatibleApplicationsBrowserTest& operator=(
      const IncompatibleApplicationsBrowserTest&) = delete;

 protected:
  // The name of the application deemed incompatible.
  static constexpr wchar_t kApplicationName[] = L"FooBar123";

  IncompatibleApplicationsBrowserTest() : scoped_domain_(true) {}

  ~IncompatibleApplicationsBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    scoped_feature_list_.InitAndEnableFeature(
        features::kIncompatibleApplicationsWarning);

    ASSERT_NO_FATAL_FAILURE(CreateModuleList());
    ASSERT_NO_FATAL_FAILURE(InstallThirdPartyApplication());

    InProcessBrowserTest::SetUp();
  }

  // Returns the path to the module list.
  base::FilePath GetModuleListPath() const {
    return scoped_temp_dir_.GetPath().Append(L"ModuleList.bin");
  }

  // Returns the path to the DLL that is injected into the process.
  base::FilePath GetDllPath() const {
    return scoped_temp_dir_.GetPath()
        .Append(kApplicationName)
        .Append(L"foo_bar.dll");
  }

 private:
  // Writes an empty serialized ModuleList proto to |GetModuleListPath()|.
  void CreateModuleList() {
    chrome::conflicts::ModuleList module_list;
    // Include an empty blocklist and allowlist.
    module_list.mutable_blocklist();
    module_list.mutable_allowlist();

    std::string contents;
    ASSERT_TRUE(module_list.SerializeToString(&contents));
    ASSERT_TRUE(base::WriteFile(GetModuleListPath(), contents));
  }

  // Registers an uninstallation entry for the third-party application, and
  // creates a DLL meant to be injected into the process.
  void InstallThirdPartyApplication() {
    // This module should not be a static dependency of the test executable, but
    // should be a build-system dependency or a module that is present on any
    // Windows machine.
    static constexpr wchar_t kTestDllName[] = L"conflicts_dll.dll";

    // Note: Using the application name for the product id.
    const std::wstring registry_key_path =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" +
        std::wstring(kApplicationName);
    base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_key_path.c_str(),
                                   KEY_WRITE);

    const base::FilePath dll_path = GetDllPath();
    ASSERT_EQ(registry_key.WriteValue(L"DisplayName", kApplicationName),
              ERROR_SUCCESS);
    ASSERT_EQ(registry_key.WriteValue(L"InstallLocation",
                                      dll_path.DirName().value().c_str()),
              ERROR_SUCCESS);

    // The UninstallString is required but its value doesn't matter.
    ASSERT_EQ(
        registry_key.WriteValue(L"UninstallString", L"c:\\foo\\uninstall.exe"),
        ERROR_SUCCESS);

    // Copy the test DLL to the install directory so that it will get associated
    // with the application by the IncompatibleApplicationsUpdater.
    base::FilePath test_dll_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_dll_path));
    test_dll_path = test_dll_path.Append(kTestDllName);

    ASSERT_TRUE(base::CreateDirectory(dll_path.DirName()));
    ASSERT_TRUE(base::CopyFile(test_dll_path, dll_path));
  }

  // The feature is always disabled on domain-joined machines.
  base::win::ScopedDomainStateForTesting scoped_domain_;

  // Temp directory used to host the install directory and the module list.
  base::ScopedTempDir scoped_temp_dir_;

  // Overrides HKLM and HKCU so that the InstalledApplications instance doesn't
  // pick up real applications on the test machine.
  registry_util::RegistryOverrideManager registry_override_manager_;

  // Enables the IncompatibleApplicationsWarning feature.
  base::test::ScopedFeatureList scoped_feature_list_;
};

// static
constexpr wchar_t IncompatibleApplicationsBrowserTest::kApplicationName[];

// This is an integration test for the identification of incompatible
// applications.
//
// This test makes sure that all the different classes interact together
// correctly.
//
// Note: This doesn't test that the chrome://settings/incompatibleApplications
// page is shown after a browser crash.
IN_PROC_BROWSER_TEST_F(IncompatibleApplicationsBrowserTest,
                       InjectIncompatibleDLL) {
  // Create the observer early so the change is guaranteed to be observed.
  auto incompatible_applications_observer =
      std::make_unique<IncompatibleApplicationsObserver>();

  base::RunLoop run_loop;
  ModuleDatabase::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([module_list_path = GetModuleListPath(),
                                  quit_closure = run_loop.QuitClosure()]() {
        ModuleDatabase* module_database = ModuleDatabase::GetInstanceForTesting(
            std::make_unique<TestInstalledApplications>());

        // Simulate the download of the module list component.
        module_database->third_party_conflicts_manager()->LoadModuleList(
            module_list_path);

        quit_closure.Run();
      }));
  run_loop.Run();

  // Injects the DLL into the process.
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  base::ScopedNativeLibrary dll(GetDllPath());
  ASSERT_TRUE(dll.is_valid());

  // Wait until the application gets marked as problematic.
  incompatible_applications_observer->WaitForIncompatibleApplicationsChanged();

  // Verify the cache.
  EXPECT_TRUE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto incompatible_applications =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(incompatible_applications.size(), 1u);
  const auto& incompatible_application = incompatible_applications[0];
  EXPECT_EQ(incompatible_application.info.name, kApplicationName);
  EXPECT_EQ(incompatible_application.blocklist_action->message_type(),
            chrome::conflicts::BlocklistMessageType::UNINSTALL);
}
