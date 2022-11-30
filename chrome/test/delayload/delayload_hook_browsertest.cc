// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/test/browser_test.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

using ChromeDelayLoadHookTest = InProcessBrowserTest;

class EchoServiceProcessObserver : public content::ServiceProcessHost::Observer,
                                   public content::BrowserChildProcessObserver {
 public:
  EchoServiceProcessObserver() {
    content::ServiceProcessHost::AddObserver(this);
    content::BrowserChildProcessObserver::Add(this);
  }

  EchoServiceProcessObserver(const EchoServiceProcessObserver&) = delete;
  EchoServiceProcessObserver& operator=(const EchoServiceProcessObserver&) =
      delete;

  ~EchoServiceProcessObserver() override {
    content::BrowserChildProcessObserver::Remove(this);
    content::ServiceProcessHost::RemoveObserver(this);
  }

  void WaitForLaunch() { launch_loop_.Run(); }

  int WaitForCrash() {
    crash_loop_.Run();
    return exit_code_;
  }

 private:
  // content::ServiceProcessHost::Observer:
  void OnServiceProcessLaunched(
      const content::ServiceProcessInfo& info) override {
    if (info.IsService<echo::mojom::EchoService>())
      launch_loop_.Quit();
  }

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override {
    if (data.metrics_name == echo::mojom::EchoService::Name_) {
      exit_code_ = info.exit_code;
      crash_loop_.Quit();
    }
  }

  int exit_code_;
  base::RunLoop launch_loop_;
  base::RunLoop crash_loop_;
};

IN_PROC_BROWSER_TEST_F(ChromeDelayLoadHookTest, ObserveDelayLoadFailure) {
  EchoServiceProcessObserver observer;
  auto echo_service =
      content::ServiceProcessHost::Launch<echo::mojom::EchoService>();
  observer.WaitForLaunch();
  echo_service->DelayLoad();
  int exit_code = observer.WaitForCrash();
  EXPECT_EQ(EXCEPTION_BREAKPOINT, static_cast<DWORD>(exit_code));
}

}  // namespace chrome
