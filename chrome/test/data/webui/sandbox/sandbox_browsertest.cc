// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
class SandboxStatusUITest : public WebUIMochaBrowserTest {
 protected:
  SandboxStatusUITest() { set_test_loader_host(chrome::kChromeUISandboxHost); }

  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "sandbox/sandbox_test.js",
        base::StringPrintf("runMochaTest('Sandbox', '%s');", testCase.c_str()));
  }
};

// This test is for Linux only.
// PLEASE READ:
// - If failures of this test are a problem on a bot under your care,
//   the proper way to address such failures is to install the SUID
//   sandbox. See:
//     https://chromium.googlesource.com/chromium/src/+/main/docs/linux/suid_sandbox_development.md
// - PLEASE DO NOT GLOBALLY DISABLE THIS TEST.
IN_PROC_BROWSER_TEST_F(SandboxStatusUITest, testSUIDorNamespaceSandboxEnabled) {
  RunTestCase("SUIDorNamespaceSandboxEnabled");
}

// The seccomp-bpf sandbox is also not compatible with ASAN.
IN_PROC_BROWSER_TEST_F(SandboxStatusUITest, testBPFSandboxEnabled) {
  RunTestCase("BPFSandboxEnabled");
}
#endif

#if BUILDFLAG(IS_WIN)
// This test is for Windows only.
using SandboxStatusWindowsUITest = WebUIMochaBrowserTest;
// TODO(crbug.com/40670321) Flaky on Windows.
IN_PROC_BROWSER_TEST_F(SandboxStatusWindowsUITest, DISABLED_testSandboxStatus) {
  set_test_loader_host(chrome::kChromeUISandboxHost);
  RunTestWithoutTestLoader("sandbox/sandbox_test.js",
                           "runMochaTest('Sandbox', 'SandboxStatus')");
}
#endif

using GPUSandboxStatusUITest = WebUIMochaBrowserTest;

// This test is disabled because it can only pass on real hardware. We
// arrange for it to run on real hardware in specific configurations
// (such as Chrome OS hardware, via Autotest), then run it with
// --gtest_also_run_disabled_tests on those configurations.

IN_PROC_BROWSER_TEST_F(GPUSandboxStatusUITest, DISABLED_testGPUSandboxEnabled) {
  set_test_loader_host(content::kChromeUIGpuHost);
  RunTestWithoutTestLoader("sandbox/gpu_test.js", "mocha.run()");
}
