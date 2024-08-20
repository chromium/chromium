// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs.h"

#include <memory>

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

class PrefsTest : public ::testing::Test {
#if BUILDFLAG(IS_WIN)
 protected:
  void SetUp() override { DeleteBrandCodeValueInRegistry(); }
  void TearDown() override { DeleteBrandCodeValueInRegistry(); }

 private:
  void DeleteBrandCodeValueInRegistry() {
    base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                      GetAppClientStateKey(L"someappid").c_str(),
                      Wow6432(KEY_SET_VALUE))
        .DeleteValue(kRegValueBrandCode);
  }
#endif
};

TEST_F(PrefsTest, PrefsCommitPendingWrites) {
  base::test::TaskEnvironment task_environment;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  // Writes something to prefs.
  metadata->SetBrandCode("someappid", "brand");
  EXPECT_STREQ(metadata->GetBrandCode("someappid").c_str(), "brand");

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(
      base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                        GetAppClientStateKey(L"someappid").c_str(),
                        Wow6432(KEY_SET_VALUE))
          .WriteValue(kRegValueBrandCode, L"nbrnd"),
      ERROR_SUCCESS);
  EXPECT_STREQ(metadata->GetBrandCode("someappid").c_str(), "nbrnd");
#endif

  // Tests writing to storage completes.
  PrefsCommitPendingWrites(pref.get());
}

}  // namespace updater
