// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {

TEST(PrefsTest, PrefsCommitPendingWrites) {
  base::test::TaskEnvironment task_environment;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  // Writes something to prefs.
  metadata->SetBrandCode("someappid", "brand");
  EXPECT_EQ(metadata->GetBrandCode("someappid"), "brand");

  metadata->SetLang("someappid", "somelang");
#if BUILDFLAG(IS_WIN)
  std::wstring registry_lang_w;
  EXPECT_EQ(
      base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                        GetAppClientStateKey(L"someappid").c_str(),
                        Wow6432(KEY_QUERY_VALUE))
          .ReadValue(kRegValueLang, &registry_lang_w),
      ERROR_SUCCESS);
  EXPECT_EQ(registry_lang_w, L"somelang");
#endif
  EXPECT_EQ(metadata->GetLang("someappid"), "somelang");

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(
      base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                        GetAppClientStateKey(L"someappid").c_str(),
                        Wow6432(KEY_SET_VALUE))
          .WriteValue(kRegValueBrandCode, L"nbrnd"),
      ERROR_SUCCESS);
  EXPECT_EQ(metadata->GetBrandCode("someappid"), "nbrnd");

  EXPECT_EQ(
      base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                        GetAppClientStateKey(L"someappid").c_str(),
                        Wow6432(KEY_SET_VALUE))
          .WriteValue(kRegValueLang, L"newlang"),
      ERROR_SUCCESS);
  EXPECT_EQ(metadata->GetLang("someappid"), "newlang");
#endif

  // Tests writing to storage completes.
  PrefsCommitPendingWrites(pref.get());

#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(base::win::RegKey(
                  UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                  GetAppClientStateKey("someappid").c_str(), Wow6432(KEY_READ))
                  .Valid());
#endif
  metadata->RemoveApp("someappid");
#if BUILDFLAG(IS_WIN)
  for (const auto& subkey : [&] {
         std::vector<std::wstring> subkeys = {
             GetAppClientStateKey("someappid")};
         if (IsSystemInstall(GetUpdaterScopeForTesting())) {
           subkeys.push_back(GetAppClientStateMediumKey("someappid"));
         }
         return subkeys;
       }()) {
    EXPECT_FALSE(
        base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                          subkey.c_str(), Wow6432(KEY_READ))
            .Valid());
  }
#endif
}

class PrefsInvalidFileTest : public testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    // Cannot create global prefs in the macOS system scope because Mach
    // bootstrap_check_in fails outside a LaunchDaemon context.
    if (IsSystemInstall(scope_)) {
      GTEST_SKIP();
    }
#endif  // BUILDFLAG(IS_MAC)

    global_dir_ = GetInstallDirectory(scope_);
    local_dir_ = GetVersionedInstallDirectory(scope_);
    ASSERT_TRUE(global_dir_);
    ASSERT_TRUE(local_dir_);
    ASSERT_TRUE(base::CreateDirectory(*global_dir_));
    ASSERT_TRUE(base::CreateDirectory(*local_dir_));
    ClearPrefs();
  }

  void TearDown() override {
    if (global_dir_ && local_dir_) {
      ClearPrefs();
    }
  }

  void ClearPrefs() {
    for (const auto& dir : {*global_dir_, *local_dir_}) {
      base::DeletePathRecursively(dir.Append(FILE_PATH_LITERAL("prefs.json")));
      base::DeletePathRecursively(dir.Append(FILE_PATH_LITERAL("prefs.bad")));
    }
  }

  base::test::TaskEnvironment task_environment_;
  const UpdaterScope scope_ = GetUpdaterScopeForTesting();
  std::optional<base::FilePath> global_dir_;
  std::optional<base::FilePath> local_dir_;
};

TEST_F(PrefsInvalidFileTest, InvalidJsonWithExistingBadPrefsDir) {
  // Invalid JSON syntax when prefs.bad already exists as a non-empty
  // directory, so moving prefs.json to prefs.bad fails.
  for (const auto& dir : {*global_dir_, *local_dir_}) {
    ASSERT_TRUE(base::WriteFile(dir.Append(FILE_PATH_LITERAL("prefs.json")),
                                "{invalid json"));
    ASSERT_TRUE(
        base::CreateDirectory(dir.Append(FILE_PATH_LITERAL("prefs.bad"))
                                  .Append(FILE_PATH_LITERAL("subdir"))));
  }

  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope_);
  ASSERT_TRUE(global_prefs);
  global_prefs->SetActiveVersion("1.2.3.4");
  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  scoped_refptr<LocalPrefs> local_prefs = CreateLocalPrefs(scope_);
  ASSERT_TRUE(local_prefs);
  local_prefs->SetQualified(true);
  PrefsCommitPendingWrites(local_prefs->GetPrefService());
}

TEST_F(PrefsInvalidFileTest, NonDictJson) {
  // Valid JSON that is not a dictionary (e.g. null, list, number).
  for (const auto& dir : {*global_dir_, *local_dir_}) {
    ASSERT_TRUE(
        base::WriteFile(dir.Append(FILE_PATH_LITERAL("prefs.json")), "null"));
  }

  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope_);
  ASSERT_TRUE(global_prefs);
  EXPECT_EQ(global_prefs->GetActiveVersion(), "0");
  global_prefs->SetActiveVersion("2.3.4.5");
  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  scoped_refptr<LocalPrefs> local_prefs = CreateLocalPrefs(scope_);
  ASSERT_TRUE(local_prefs);
  EXPECT_FALSE(local_prefs->GetQualified());
  local_prefs->SetQualified(true);
  PrefsCommitPendingWrites(local_prefs->GetPrefService());
}

}  // namespace updater
