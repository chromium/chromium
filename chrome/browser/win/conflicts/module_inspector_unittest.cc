// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_inspector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CrashingUtilWinImpl : public chrome::mojom::UtilWin {
 public:
  explicit CrashingUtilWinImpl(
      mojo::PendingReceiver<chrome::mojom::UtilWin> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~CrashingUtilWinImpl() override = default;

 private:
  // chrome::mojom::UtilWin:
  void IsPinnedToTaskbar(IsPinnedToTaskbarCallback callback) override {}
  void UnpinShortcuts(const std::vector<base::FilePath>& shortcuts,
                      UnpinShortcutsCallback result_callback) override {}
  void CreateOrUpdateShortcuts(
      const std::vector<base::FilePath>& shortcut_paths,
      const std::vector<base::win::ShortcutProperties>& properties,
      base::win::ShortcutOperation operation,
      CreateOrUpdateShortcutsCallback callback) override {}

  void CallExecuteSelectFile(ui::SelectFileDialog::Type type,
                             uint32_t owner,
                             const std::u16string& title,
                             const base::FilePath& default_path,
                             const std::vector<ui::FileFilterSpec>& filter,
                             int32_t file_type_index,
                             const std::u16string& default_extension,
                             CallExecuteSelectFileCallback callback) override {}
  void InspectModule(const base::FilePath& module_path,
                     InspectModuleCallback callback) override {
    // Reset the mojo connection to simulate the utility process crashing.
    receiver_.reset();
  }
  void GetAntiVirusProducts(bool report_full_names,
                            GetAntiVirusProductsCallback callback) override {}
  void GetTpmIdentifier(bool report_full_names,
                        GetTpmIdentifierCallback callback) override {}

  mojo::Receiver<chrome::mojom::UtilWin> receiver_;
};

base::FilePath GetKernel32DllFilePath() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string sysroot;
  EXPECT_TRUE(env->GetVar("SYSTEMROOT", &sysroot));

  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(sysroot).Append(L"system32\\kernel32.dll");

  return path;
}

bool CreateInspectionResultsCacheWithEntry(
    const ModuleInfoKey& module_key,
    const ModuleInspectionResult& inspection_result) {
  // First create a cache with bogus data and create the cache file.
  InspectionResultsCache inspection_results_cache;

  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache);

  return WriteInspectionResultsCache(
      ModuleInspector::GetInspectionResultsCachePath(),
      inspection_results_cache);
}

class ModuleInspectorTest : public testing::Test {
 public:
  ModuleInspectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ModuleInspectorTest(const ModuleInspectorTest&) = delete;
  ModuleInspectorTest& operator=(const ModuleInspectorTest&) = delete;

  std::unique_ptr<ModuleInspector> CreateModuleInspector() {
    auto module_inspector =
        std::make_unique<ModuleInspector>(base::BindRepeating(
            &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));
    module_inspector->SetUtilWinFactoryCallbackForTesting(base::BindRepeating(
        &ModuleInspectorTest::CreateUtilWinService, base::Unretained(this)));
    return module_inspector;
  }

  std::unique_ptr<ModuleInspector> CreateModuleInspectorWithCrashingUtilWin() {
    auto module_inspector =
        std::make_unique<ModuleInspector>(base::BindRepeating(
            &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));
    module_inspector->SetUtilWinFactoryCallbackForTesting(
        base::BindRepeating(&ModuleInspectorTest::CreateCrashingUtilWinService,
                            base::Unretained(this)));
    return module_inspector;
  }

  // Callback for ModuleInspector.
  void OnModuleInspected(const ModuleInfoKey& module_key,
                         ModuleInspectionResult inspection_result) {
    inspected_modules_.push_back(std::move(inspection_result));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FastForwardToIdleTimer() {
    task_environment_.FastForwardBy(
        ModuleInspector::kFlushInspectionResultsTimerTimeout);
    task_environment_.RunUntilIdle();
  }

  const std::vector<ModuleInspectionResult>& inspected_modules() {
    return inspected_modules_;
  }

  void ClearInspectedModules() { inspected_modules_.clear(); }

  base::test::TaskEnvironment task_environment_;

  // Holds a test UtilWin service implementation.
  std::unique_ptr<chrome::mojom::UtilWin> util_win_impl_;

 private:
  mojo::Remote<chrome::mojom::UtilWin> CreateUtilWinService() {
    mojo::Remote<chrome::mojom::UtilWin> remote;

    util_win_impl_ =
        std::make_unique<UtilWinImpl>(remote.BindNewPipeAndPassReceiver());

    return remote;
  }

  mojo::Remote<chrome::mojom::UtilWin> CreateCrashingUtilWinService() {
    mojo::Remote<chrome::mojom::UtilWin> remote;

    util_win_impl_ = std::make_unique<CrashingUtilWinImpl>(
        remote.BindNewPipeAndPassReceiver());

    return remote;
  }

  std::vector<ModuleInspectionResult> inspected_modules_;
};

}  // namespace

TEST_F(ModuleInspectorTest, StartInspection) {
  auto module_inspector = CreateModuleInspector();

  module_inspector->AddModule({GetKernel32DllFilePath(), 0, 0});
  RunUntilIdle();

  // Modules are not inspected until StartInspection() is called.
  ASSERT_EQ(0u, inspected_modules().size());

  module_inspector->StartInspection();
  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, MultipleModules) {
  ModuleInfoKey kTestCases[] = {
      {base::FilePath(), 0, 0}, {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0}, {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0},
  };

  auto module_inspector = CreateModuleInspector();
  module_inspector->StartInspection();

  for (const auto& module : kTestCases)
    module_inspector->AddModule(module);

  RunUntilIdle();

  EXPECT_EQ(5u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, InspectionResultsCache) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::ScopedPathOverride scoped_user_data_dir_override(
      chrome::DIR_USER_DATA, scoped_temp_dir.GetPath());

  // First create a cache with bogus data and create the cache file.
  ModuleInfoKey module_key(GetKernel32DllFilePath(), 0, 0);
  ModuleInspectionResult inspection_result;
  inspection_result.location = u"BogusLocation";
  inspection_result.basename = u"BogusBasename";

  ASSERT_TRUE(
      CreateInspectionResultsCacheWithEntry(module_key, inspection_result));

  auto module_inspector = CreateModuleInspector();
  module_inspector->StartInspection();

  module_inspector->AddModule(module_key);

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());

  // The following comparisons can only succeed if the module was truly read
  // from the cache.
  ASSERT_EQ(inspected_modules()[0].location, inspection_result.location);
  ASSERT_EQ(inspected_modules()[0].basename, inspection_result.basename);
}

// Tests that when OnModuleDatabaseIdle() notification is received, the cache is
// flushed to disk.
TEST_F(ModuleInspectorTest, InspectionResultsCache_OnModuleDatabaseIdle) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::ScopedPathOverride scoped_user_data_dir_override(
      chrome::DIR_USER_DATA, scoped_temp_dir.GetPath());

  auto module_inspector = CreateModuleInspector();
  module_inspector->StartInspection();

  ModuleInfoKey module_key(GetKernel32DllFilePath(), 0, 0);
  module_inspector->AddModule(module_key);

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());

  module_inspector->OnModuleDatabaseIdle();
  RunUntilIdle();

  // If the cache was written to disk, it should contain the one entry for
  // Kernel32.dll.
  InspectionResultsCache inspection_results_cache;
  EXPECT_EQ(ReadInspectionResultsCache(
                ModuleInspector::GetInspectionResultsCachePath(), 0,
                &inspection_results_cache),
            ReadCacheResult::kSuccess);

  EXPECT_EQ(inspection_results_cache.size(), 1u);
  auto inspection_result =
      GetInspectionResultFromCache(module_key, &inspection_results_cache);
  EXPECT_TRUE(inspection_result);
}

// Tests that when the timer expires before the OnModuleDatabaseIdle()
// notification, the cache is flushed to disk.
TEST_F(ModuleInspectorTest, InspectionResultsCache_TimerExpired) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::ScopedPathOverride scoped_user_data_dir_override(
      chrome::DIR_USER_DATA, scoped_temp_dir.GetPath());

  auto module_inspector = CreateModuleInspector();
  module_inspector->StartInspection();

  ModuleInfoKey module_key(GetKernel32DllFilePath(), 0, 0);
  module_inspector->AddModule(module_key);

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());

  // Fast forwarding until the timer is fired.
  FastForwardToIdleTimer();

  // If the cache was flushed, it should contain the one entry for Kernel32.dll.
  InspectionResultsCache inspection_results_cache;
  EXPECT_EQ(ReadInspectionResultsCache(
                ModuleInspector::GetInspectionResultsCachePath(), 0,
                &inspection_results_cache),
            ReadCacheResult::kSuccess);

  EXPECT_EQ(inspection_results_cache.size(), 1u);
  auto inspection_result =
      GetInspectionResultFromCache(module_key, &inspection_results_cache);
  EXPECT_TRUE(inspection_result);
}

TEST_F(ModuleInspectorTest, MojoConnectionError) {
  auto module_inspector = CreateModuleInspectorWithCrashingUtilWin();
  module_inspector->StartInspection();
  EXPECT_NE(0,
            module_inspector->get_connection_error_retry_count_for_testing());

  module_inspector->AddModule({GetKernel32DllFilePath(), 0, 0});

  // This will repeatedly try to inspect the module, get a connection error and
  // restart the UtilWin service until the retry limit is hit.
  RunUntilIdle();

  EXPECT_EQ(0,
            module_inspector->get_connection_error_retry_count_for_testing());

  // No modules were inspected.
  EXPECT_EQ(0u, inspected_modules().size());
}

// This test case ensure that if a random connection error happens while the
// ModuleInspector is asynchronously waiting on the inspection result retrieved
// from the cache, StartInspectingModule() is not erroneously re-invoked from
// the connection error handler.
// Regression test for https://crbug.com/1213241.
TEST_F(ModuleInspectorTest, WaitingOnCacheConnectionError) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::ScopedPathOverride scoped_user_data_dir_override(
      chrome::DIR_USER_DATA, scoped_temp_dir.GetPath());

  // First create a cache with bogus data and create the cache file.
  ModuleInfoKey module_key(GetKernel32DllFilePath(), 0, 0);
  ModuleInspectionResult inspection_result;
  inspection_result.location = u"BogusLocation";
  inspection_result.basename = u"BogusBasename";

  ASSERT_TRUE(
      CreateInspectionResultsCacheWithEntry(module_key, inspection_result));

  auto module_inspector = CreateModuleInspector();
  module_inspector->StartInspection();

  // Inspect a module not in the cache to ensure the UtilWin service is started.
  module_inspector->AddModule(ModuleInfoKey(base::FilePath(), 0, 0));
  RunUntilIdle();

  // Now destroy the UtilWin service. This will queue up a task to handle the
  // connection error.
  util_win_impl_.reset();

  // Before handling the connection error, start inspecting a module that exists
  // in the inspection results cache. This will queue up OnInspectionFinished()
  // with the result from the cache.
  module_inspector->AddModule(module_key);

  // Now run all queued tasks. The connection error handler will run but will
  // not cause StartInspectingModule() to be invoked.
  RunUntilIdle();

  // 2 modules were added to the inspection queue and thus 2 results were
  // correctly received.
  ASSERT_EQ(2u, inspected_modules().size());
}
