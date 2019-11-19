// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/remoting/remote_desktop_browsertest.h"

namespace remoting {

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_Launch DISABLED_MANUAL_Launch
#else
#define MAYBE_MANUAL_Launch MANUAL_Launch
#endif
IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MAYBE_MANUAL_Launch) {
  VerifyInternetAccess();

  Install();

  LaunchChromotingApp(false);

  Cleanup();
}

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_LaunchDeferredStart DISABLED_MANUAL_LaunchDeferredStart
#else
#define MAYBE_MANUAL_LaunchDeferredStart MANUAL_LaunchDeferredStart
#endif
IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest,
                       MAYBE_MANUAL_LaunchDeferredStart) {
  VerifyInternetAccess();

  Install();

  LaunchChromotingApp(true);
  StartChromotingApp();

  Cleanup();
}

}  // namespace remoting
