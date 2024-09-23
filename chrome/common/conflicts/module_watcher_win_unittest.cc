// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/conflicts/module_watcher_win.h"

#include <windows.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

class ModuleWatcherTest : public testing::Test {
 public:
  ModuleWatcherTest(const ModuleWatcherTest&) = delete;
  ModuleWatcherTest& operator=(const ModuleWatcherTest&) = delete;

 protected:
  ModuleWatcherTest()
      : module_(nullptr),
        module_event_count_(0),
        module_already_loaded_event_count_(0),
        module_loaded_event_count_(0) {}

  void OnModuleEvent(const ModuleWatcher::ModuleEvent& event) {
    ++module_event_count_;
    switch (event.event_type) {
      case ModuleWatcher::ModuleEventType::kModuleAlreadyLoaded:
        ++module_already_loaded_event_count_;
        break;
      case ModuleWatcher::ModuleEventType::kModuleLoaded:
        ++module_loaded_event_count_;
        break;
    }
  }

  void TearDown() override { UnloadModule(); }

  void LoadModule() {
    if (module_)
      return;
    // This module should not be a static dependency of the unit-test
    // executable, but should be a build-system dependency or a module that is
    // present on any Windows machine.
    static constexpr wchar_t kModuleName[] = L"conflicts_dll.dll";
    // The module should not already be loaded.
    ASSERT_FALSE(::GetModuleHandle(kModuleName));
    // It should load successfully.
    module_ = ::LoadLibrary(kModuleName);
    ASSERT_TRUE(module_);
  }

  void UnloadModule() {
    if (!module_)
      return;
    ::FreeLibrary(module_);
    module_ = nullptr;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  std::unique_ptr<ModuleWatcher> Create() {
    return ModuleWatcher::Create(base::BindRepeating(
        &ModuleWatcherTest::OnModuleEvent, base::Unretained(this)));
  }

  base::test::TaskEnvironment task_environment_;

  // Holds a handle to a loaded module.
  HMODULE module_;
  // Total number of module events seen.
  int module_event_count_;
  // Total number of MODULE_ALREADY_LOADED events seen.
  int module_already_loaded_event_count_;
  // Total number of MODULE_LOADED events seen.
  int module_loaded_event_count_;
};

TEST_F(ModuleWatcherTest, SingleModuleWatcherOnly) {
  std::unique_ptr<ModuleWatcher> mw1(Create());
  EXPECT_TRUE(mw1.get());

  std::unique_ptr<ModuleWatcher> mw2(Create());
  EXPECT_FALSE(mw2.get());
}

// TODO: crbug.com/347201817 - Fix ODR violation.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_ModuleEvents DISABLED_ModuleEvents
#else
#define MAYBE_ModuleEvents ModuleEvents
#endif
TEST_F(ModuleWatcherTest, MAYBE_ModuleEvents) {
  // Create the module watcher. This should immediately enumerate all already
  // loaded modules on a background task.
  std::unique_ptr<ModuleWatcher> mw(Create());
  RunUntilIdle();

  EXPECT_LT(0, module_event_count_);
  EXPECT_LT(0, module_already_loaded_event_count_);
  EXPECT_EQ(0, module_loaded_event_count_);

  // Dynamically load a module and ensure a notification is received for it.
  int previous_module_loaded_event_count = module_loaded_event_count_;
  LoadModule();
  EXPECT_LT(previous_module_loaded_event_count, module_loaded_event_count_);

  UnloadModule();

  // Dynamically load a module and ensure a notification is received for it.
  previous_module_loaded_event_count = module_loaded_event_count_;
  LoadModule();
  EXPECT_LT(previous_module_loaded_event_count, module_loaded_event_count_);

  UnloadModule();

  // Destroy the module watcher.
  mw.reset();

  // Load the module and ensure no notification is received this time.
  previous_module_loaded_event_count = module_loaded_event_count_;
  LoadModule();
  EXPECT_EQ(previous_module_loaded_event_count, module_loaded_event_count_);
}
