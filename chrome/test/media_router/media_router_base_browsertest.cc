// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_base_browsertest.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"

namespace {
// Command line argument to specify unpacked extension location.
const char kExtensionUnpacked[] = "extension-unpacked";
}  // namespace


namespace media_router {

MediaRouterBaseBrowserTest::MediaRouterBaseBrowserTest()
    : extension_load_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED),
      extension_host_created_(false) {
}

MediaRouterBaseBrowserTest::~MediaRouterBaseBrowserTest() {
}

void MediaRouterBaseBrowserTest::SetUp() {
  ParseCommandLine();
  ExtensionBrowserTest::SetUp();
}

void MediaRouterBaseBrowserTest::TearDown() {
  ExtensionBrowserTest::TearDown();
}

void MediaRouterBaseBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  extensions::ProcessManager* process_manager = extensions::ProcessManager::Get(
      browser()->profile()->GetOriginalProfile());
  DCHECK(process_manager);
  process_manager->AddObserver(this);
  InstallAndEnableMRExtension();
  extension_load_event_.Wait();
}

void MediaRouterBaseBrowserTest::TearDownOnMainThread() {
  UninstallMRExtension();
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  DCHECK(process_manager);
  process_manager->RemoveObserver(this);
  ExtensionBrowserTest::TearDownOnMainThread();
}

void MediaRouterBaseBrowserTest::InstallAndEnableMRExtension() {
  const extensions::Extension* extension = LoadExtension(extension_unpacked_);
  extension_id_ = extension->id();
}

void MediaRouterBaseBrowserTest::UninstallMRExtension() {
  if (!extension_id_.empty()) {
    UninstallExtension(extension_id_);
  }
}

bool MediaRouterBaseBrowserTest::ConditionalWait(
    base::TimeDelta timeout,
    base::TimeDelta interval,
    const base::Callback<bool(void)>& callback) {
  base::ElapsedTimer timer;
  do {
    if (callback.Run())
      return true;

    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), interval);
    run_loop.Run();
  } while (timer.Elapsed() < timeout);

  return false;
}

void MediaRouterBaseBrowserTest::Wait(base::TimeDelta timeout) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), timeout);
  run_loop.Run();
}

void MediaRouterBaseBrowserTest::OnBackgroundHostCreated(
    extensions::ExtensionHost* host) {
  extension_host_created_ = true;
  DVLOG(0) << "Host created";
  extension_load_event_.Signal();
}

void MediaRouterBaseBrowserTest::ParseCommandLine() {
  DVLOG(0) << "ParseCommandLine";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  extension_unpacked_ = command_line->GetSwitchValuePath(kExtensionUnpacked);

  // No extension provided. Use the default component extension in Chromium.
  if (extension_unpacked_.empty()) {
    base::FilePath base_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &base_dir));
    base::FilePath extension_path = base_dir.Append(FILE_PATH_LITERAL(
        "gen/chrome/browser/resources/media_router/extension"));
    if (PathExists(extension_path)) {
      extension_unpacked_ = extension_path;
    }
  }

  // An unpacked component extension must be provided.
  ASSERT_FALSE(extension_unpacked_.empty());
}

Browser* MediaRouterBaseBrowserTest::browser() {
  return ExtensionBrowserTest::browser();
}

}  // namespace media_router
