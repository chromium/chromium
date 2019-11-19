// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/conflicts/remote_module_watcher_win.h"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class RemoteModuleWatcherTest : public testing::Test,
                                public mojom::ModuleEventSink {
 public:
  RemoteModuleWatcherTest() = default;
  ~RemoteModuleWatcherTest() override = default;

  mojo::PendingRemote<mojom::ModuleEventSink> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::ModuleEventSink:
  void OnModuleEvents(
      const std::vector<uint64_t>& module_load_addresses) override {
    module_event_count_ += module_load_addresses.size();
  }

  void LoadModule() {
    if (module_handle_)
      return;
    // This module should not be a static dependency of the unit-test
    // executable, but should be a build-system dependency or a module that is
    // present on any Windows machine.
    static constexpr wchar_t kModuleName[] = L"conflicts_dll.dll";
    // The module should not already be loaded.
    ASSERT_FALSE(::GetModuleHandle(kModuleName));
    // It should load successfully.
    module_handle_ = ::LoadLibrary(kModuleName);
    ASSERT_TRUE(module_handle_);
  }

  void UnloadModule() {
    if (!module_handle_)
      return;
    ::FreeLibrary(module_handle_);
    module_handle_ = nullptr;
  }

  // Runs the task scheduler until no tasks are running.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FastForwardByIdleDelay() {
    task_environment_.FastForwardBy(RemoteModuleWatcher::kIdleDelay);
  }

  HMODULE module_handle() { return module_handle_; }

  int module_event_count() { return module_event_count_; }

 private:
  // Must be first.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Binds a ModuleEventSinkRequest to this implementation of ModuleEventSink.
  mojo::Receiver<mojom::ModuleEventSink> receiver_{this};

  // Holds a handle to a loaded module.
  HMODULE module_handle_ = nullptr;

  // Total number of module events seen.
  int module_event_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RemoteModuleWatcherTest);
};

}  // namespace

TEST_F(RemoteModuleWatcherTest, ModuleEvents) {
  auto remote_module_watcher =
      RemoteModuleWatcher::Create(base::ThreadTaskRunnerHandle::Get(), Bind());

  // Wait until the watcher is initialized and events for already loaded modules
  // are received.
  RunUntilIdle();
  // Now wait for the timer used to batch events to expire.
  FastForwardByIdleDelay();

  EXPECT_GT(module_event_count(), 0);

  // Dynamically load a module and ensure a notification is received for it.
  int previous_module_event_count = module_event_count();
  LoadModule();
  FastForwardByIdleDelay();
  EXPECT_GT(module_event_count(), previous_module_event_count);

  UnloadModule();

  // Destroy the module watcher.
  remote_module_watcher = nullptr;
  RunUntilIdle();

  // Load the module and ensure no notification is received this time.
  previous_module_event_count = module_event_count();
  LoadModule();
  FastForwardByIdleDelay();

  EXPECT_EQ(module_event_count(), previous_module_event_count);

  UnloadModule();
}
