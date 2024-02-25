// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"

#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/proto/module_list.pb.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ThirdPartyConflictsManagerTest : public testing::Test,
                                       public ModuleDatabaseEventSource {
 public:
  ThirdPartyConflictsManagerTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  ThirdPartyConflictsManagerTest(const ThirdPartyConflictsManagerTest&) =
      delete;
  ThirdPartyConflictsManagerTest& operator=(
      const ThirdPartyConflictsManagerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    scoped_feature_list_.InitWithFeatures(
        // Enabled features.
        {features::kIncompatibleApplicationsWarning,
         features::kThirdPartyModulesBlocking},
        // Disabled features.
        {});
  }

  // Returns the path to the module list.
  base::FilePath GetModuleListPath() const {
    return scoped_temp_dir_.GetPath().Append(L"ModuleList.bin");
  }

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

  void OnManagerInitializationComplete(
      base::OnceClosure quit_closure,
      ThirdPartyConflictsManager::State final_state) {
    final_state_ = final_state;
    std::move(quit_closure).Run();
  }

  const std::optional<ThirdPartyConflictsManager::State>& final_state() {
    return final_state_;
  }

  // ModuleDatabaseEventSource:
  void AddObserver(ModuleDatabaseObserver* observer) override {}
  void RemoveObserver(ModuleDatabaseObserver* observer) override {}

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;

  // Temp directory used to host module list.
  base::ScopedTempDir scoped_temp_dir_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::optional<ThirdPartyConflictsManager::State> final_state_;
};

std::pair<ModuleInfoKey, ModuleInfoData> CreateExeModuleInfo() {
  base::FilePath exe_path;
  base::PathService::Get(base::FILE_EXE, &exe_path);

  std::pair<ModuleInfoKey, ModuleInfoData> module_info(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(exe_path), 0, 0),
      std::forward_as_tuple());

  module_info.second.inspection_result =
      std::make_optional<ModuleInspectionResult>();

  return module_info;
}

TEST_F(ThirdPartyConflictsManagerTest, InitializeUpdaters) {
  ThirdPartyConflictsManager third_party_conflicts_manager(this);

  // The ThirdPartyConflictsManager class looks for the certificate info of the
  // current exe via the ModuleDatabaseObserver interface.
  auto exe_module_info = CreateExeModuleInfo();
  third_party_conflicts_manager.OnNewModuleFound(exe_module_info.first,
                                                 exe_module_info.second);

  third_party_conflicts_manager.OnModuleDatabaseIdle();
  ASSERT_NO_FATAL_FAILURE(CreateModuleList());
  third_party_conflicts_manager.LoadModuleList(GetModuleListPath());

  base::RunLoop run_loop;
  third_party_conflicts_manager.ForceInitialization(base::BindRepeating(
      &ThirdPartyConflictsManagerTest::OnManagerInitializationComplete,
      base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();

  ASSERT_TRUE(final_state().has_value());

  EXPECT_EQ(final_state().value(),
            ThirdPartyConflictsManager::State::kWarningAndBlockingInitialized);
}

TEST_F(ThirdPartyConflictsManagerTest, InvalidModuleList) {
  ThirdPartyConflictsManager third_party_conflicts_manager(this);

  third_party_conflicts_manager.OnModuleDatabaseIdle();

  // Pass in an empty path which will ensure that the deserialization will fail.
  third_party_conflicts_manager.LoadModuleList(GetModuleListPath());

  base::RunLoop run_loop;
  third_party_conflicts_manager.ForceInitialization(base::BindRepeating(
      &ThirdPartyConflictsManagerTest::OnManagerInitializationComplete,
      base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();

  ASSERT_TRUE(final_state().has_value());
  EXPECT_EQ(final_state().value(),
            ThirdPartyConflictsManager::State::kModuleListInvalidFailure);
}

TEST_F(ThirdPartyConflictsManagerTest, DestroyManager) {
  auto third_party_conflicts_manager =
      std::make_unique<ThirdPartyConflictsManager>(this);

  third_party_conflicts_manager->OnModuleDatabaseIdle();
  ASSERT_NO_FATAL_FAILURE(CreateModuleList());
  third_party_conflicts_manager->LoadModuleList(GetModuleListPath());

  base::RunLoop run_loop;
  third_party_conflicts_manager->ForceInitialization(base::BindRepeating(
      &ThirdPartyConflictsManagerTest::OnManagerInitializationComplete,
      base::Unretained(this), run_loop.QuitClosure()));

  // Delete the instance while it is initializing.
  third_party_conflicts_manager = nullptr;
  run_loop.Run();

  ASSERT_TRUE(final_state().has_value());
  EXPECT_EQ(final_state().value(),
            ThirdPartyConflictsManager::State::kDestroyed);
}
