// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_database.h"

#include <memory>

#include "base/bind.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr content::ProcessType kProcessType1 = content::PROCESS_TYPE_BROWSER;
constexpr content::ProcessType kProcessType2 = content::PROCESS_TYPE_RENDERER;

constexpr wchar_t kDll1[] = L"dummy.dll";
constexpr wchar_t kDll2[] = L"foo.dll";

constexpr size_t kSize1 = 100 * 4096;
constexpr size_t kSize2 = 20 * 4096;

constexpr uint32_t kTime1 = 0xDEADBEEF;
constexpr uint32_t kTime2 = 0xBAADF00D;

}  // namespace

class ModuleDatabaseTest : public testing::Test {
 protected:
  ModuleDatabaseTest()
      : dll1_(kDll1),
        dll2_(kDll2),
        task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        module_database_(std::make_unique<ModuleDatabase>(
            /* third_party_blocking_policy_enabled = */ false)) {
    mojo::PendingRemote<chrome::mojom::UtilWin> remote;
    util_win_impl_.emplace(remote.InitWithNewPipeAndPassReceiver());
    module_database_->module_inspector_.SetRemoteUtilWinForTesting(
        std::move(remote));
  }

  ~ModuleDatabaseTest() override {
    module_database_ = nullptr;

    // Clear the outstanding delayed tasks that were posted by the
    // ModuleDatabase instance.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  const ModuleDatabase::ModuleMap& modules() {
    return module_database_->modules_;
  }

  ModuleDatabase* module_database() { return module_database_.get(); }

  void RunSchedulerUntilIdle() { task_environment_.RunUntilIdle(); }

  void FastForwardToIdleTimer() {
    task_environment_.FastForwardBy(ModuleDatabase::kIdleTimeout);
    task_environment_.RunUntilIdle();
  }

  const base::FilePath dll1_;
  const base::FilePath dll2_;

 private:
  // Must be before |module_database_|.
  content::BrowserTaskEnvironment task_environment_;

  ScopedTestingLocalState scoped_testing_local_state_;

  base::Optional<UtilWinImpl> util_win_impl_;

  std::unique_ptr<ModuleDatabase> module_database_;

  DISALLOW_COPY_AND_ASSIGN(ModuleDatabaseTest);
};

TEST_F(ModuleDatabaseTest, DatabaseIsConsistent) {
  EXPECT_EQ(0u, modules().size());

  // Load a module.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1);
  EXPECT_EQ(1u, modules().size());

  // Ensure that the process and module sets are up to date.
  auto m1 = modules().begin();
  EXPECT_EQ(dll1_, m1->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m1->second.process_types);

  // Provide a redundant load message for that module.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1);
  EXPECT_EQ(1u, modules().size());

  // Ensure that the process and module sets haven't changed.
  EXPECT_EQ(dll1_, m1->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m1->second.process_types);

  // Load a second module into the process.
  module_database()->OnModuleLoad(kProcessType1, dll2_, kSize2, kTime2);
  EXPECT_EQ(2u, modules().size());

  // Ensure that the process and module sets are up to date.
  auto m2 = modules().rbegin();
  EXPECT_EQ(dll2_, m2->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m2->second.process_types);

  // Load the dummy.dll in the second process as well.
  module_database()->OnModuleLoad(kProcessType2, dll1_, kSize1, kTime1);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER) |
                ProcessTypeToBit(content::PROCESS_TYPE_RENDERER),
            m1->second.process_types);
}

// A dummy observer that only counts how many notifications it receives.
class DummyObserver : public ModuleDatabaseObserver {
 public:
  DummyObserver() = default;
  ~DummyObserver() override = default;

  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override {
    new_module_count_++;
  }

  void OnKnownModuleLoaded(const ModuleInfoKey& module_key,
                           const ModuleInfoData& module_data) override {
    known_module_loaded_count_++;
  }

  void OnModuleDatabaseIdle() override {
    on_module_database_idle_called_ = true;
  }

  int new_module_count() { return new_module_count_; }
  int known_module_loaded_count() { return known_module_loaded_count_; }
  bool on_module_database_idle_called() {
    return on_module_database_idle_called_;
  }

 private:
  int new_module_count_ = 0;
  int known_module_loaded_count_ = 0;
  bool on_module_database_idle_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(DummyObserver);
};

TEST_F(ModuleDatabaseTest, Observers) {
  // Assume there is no shell extensions or IMEs.
  module_database()->OnShellExtensionEnumerationFinished();
  module_database()->OnImeEnumerationFinished();

  DummyObserver before_load_observer;
  EXPECT_EQ(0, before_load_observer.new_module_count());

  module_database()->AddObserver(&before_load_observer);
  EXPECT_EQ(0, before_load_observer.new_module_count());

  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1);
  RunSchedulerUntilIdle();

  EXPECT_EQ(1, before_load_observer.new_module_count());
  module_database()->RemoveObserver(&before_load_observer);

  // New observers get notified for past loaded modules.
  DummyObserver after_load_observer;
  EXPECT_EQ(0, after_load_observer.new_module_count());

  module_database()->AddObserver(&after_load_observer);
  EXPECT_EQ(1, after_load_observer.new_module_count());

  module_database()->RemoveObserver(&after_load_observer);
}

TEST_F(ModuleDatabaseTest, OnKnownModuleLoaded) {
  DummyObserver dummy_observer;
  module_database()->AddObserver(&dummy_observer);

  EXPECT_EQ(0, dummy_observer.new_module_count());
  EXPECT_EQ(0, dummy_observer.known_module_loaded_count());

  // Assume there is one shell extension.
  module_database()->OnShellExtensionEnumerated(dll1_, kSize1, kTime1);
  module_database()->OnShellExtensionEnumerationFinished();
  module_database()->OnImeEnumerationFinished();

  RunSchedulerUntilIdle();

  EXPECT_EQ(1, dummy_observer.new_module_count());
  EXPECT_EQ(0, dummy_observer.known_module_loaded_count());

  // Pretend the shell extension loads.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1);
  RunSchedulerUntilIdle();

  EXPECT_EQ(1, dummy_observer.new_module_count());
  EXPECT_EQ(1, dummy_observer.known_module_loaded_count());

  module_database()->RemoveObserver(&dummy_observer);
}

// Tests the idle cycle of the ModuleDatabase.
TEST_F(ModuleDatabaseTest, IsIdle) {
  // Assume there is no shell extensions or IMEs.
  module_database()->OnShellExtensionEnumerationFinished();
  module_database()->OnImeEnumerationFinished();

  // ModuleDatabase starts busy.
  EXPECT_FALSE(module_database()->IsIdle());

  // Can't fast forward to idle because a module load event is needed.
  FastForwardToIdleTimer();
  EXPECT_FALSE(module_database()->IsIdle());

  // A load module event starts the timer.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1);
  EXPECT_FALSE(module_database()->IsIdle());

  FastForwardToIdleTimer();
  EXPECT_TRUE(module_database()->IsIdle());

  // A new shell extension resets the timer.
  module_database()->OnShellExtensionEnumerated(dll1_, kSize1, kTime1);
  EXPECT_FALSE(module_database()->IsIdle());

  FastForwardToIdleTimer();
  EXPECT_TRUE(module_database()->IsIdle());

  // Adding an observer while idle immediately calls OnModuleDatabaseIdle().
  DummyObserver is_idle_observer;
  module_database()->AddObserver(&is_idle_observer);
  EXPECT_TRUE(is_idle_observer.on_module_database_idle_called());

  module_database()->RemoveObserver(&is_idle_observer);

  // Make the ModuleDabatase busy.
  module_database()->OnModuleLoad(kProcessType2, dll2_, kSize2, kTime2);
  EXPECT_FALSE(module_database()->IsIdle());

  // Adding an observer while busy doesn't.
  DummyObserver is_busy_observer;
  module_database()->AddObserver(&is_busy_observer);
  EXPECT_FALSE(is_busy_observer.on_module_database_idle_called());

  // Fast forward will call OnModuleDatabaseIdle().
  FastForwardToIdleTimer();
  EXPECT_TRUE(module_database()->IsIdle());
  EXPECT_TRUE(is_busy_observer.on_module_database_idle_called());

  module_database()->RemoveObserver(&is_busy_observer);
}

// The ModuleDatabase waits until shell extensions and IMEs are enumerated
// before notifying observers or going idle.
TEST_F(ModuleDatabaseTest, WaitUntilRegisteredModulesEnumerated) {
  // This observer is added before the first loaded module.
  DummyObserver before_load_observer;
  module_database()->AddObserver(&before_load_observer);
  EXPECT_EQ(0, before_load_observer.new_module_count());

  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1);
  FastForwardToIdleTimer();

  // Idle state is prevented.
  EXPECT_FALSE(module_database()->IsIdle());
  EXPECT_EQ(0, before_load_observer.new_module_count());
  EXPECT_FALSE(before_load_observer.on_module_database_idle_called());

  // This observer is added after the first loaded module.
  DummyObserver after_load_observer;
  module_database()->AddObserver(&after_load_observer);
  EXPECT_EQ(0, after_load_observer.new_module_count());
  EXPECT_FALSE(after_load_observer.on_module_database_idle_called());

  // Simulate the enumerations ending.
  module_database()->OnImeEnumerationFinished();
  module_database()->OnShellExtensionEnumerationFinished();

  EXPECT_EQ(1, before_load_observer.new_module_count());
  EXPECT_TRUE(before_load_observer.on_module_database_idle_called());
  EXPECT_EQ(1, after_load_observer.new_module_count());
  EXPECT_TRUE(after_load_observer.on_module_database_idle_called());

  module_database()->RemoveObserver(&after_load_observer);
  module_database()->RemoveObserver(&before_load_observer);
}
