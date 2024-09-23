// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_test_util.h"
#include "components/permissions/permission_prompt.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace permissions {
class PermissionRequest;
}

// Use this class to test on a default window or an app window. Inheriting from
// ExtensionBrowserTest allows us to easily load and launch apps, and doesn't
// really add any extra work.
class PermissionBubbleBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  PermissionBubbleBrowserTest();

  PermissionBubbleBrowserTest(const PermissionBubbleBrowserTest&) = delete;
  PermissionBubbleBrowserTest& operator=(const PermissionBubbleBrowserTest&) =
      delete;

  ~PermissionBubbleBrowserTest() override;

  void SetUpOnMainThread() override;

  // Opens an app window and returns its WebContents.
  content::WebContents* OpenExtensionAppWindow();

  permissions::PermissionPrompt::Delegate* test_delegate() {
    return &test_delegate_;
  }

 private:
  TestPermissionBubbleViewDelegate test_delegate_;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
};

// Use this class to test on a kiosk window.
class PermissionBubbleKioskBrowserTest : public PermissionBubbleBrowserTest {
 public:
  PermissionBubbleKioskBrowserTest();

  PermissionBubbleKioskBrowserTest(const PermissionBubbleKioskBrowserTest&) =
      delete;
  PermissionBubbleKioskBrowserTest& operator=(
      const PermissionBubbleKioskBrowserTest&) = delete;

  ~PermissionBubbleKioskBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
#if BUILDFLAG(IS_MAC)
  // Toggling fullscreen mode on Mac can be flaky for tests run in parallel
  // because only one window may be animating into or out of fullscreen at a
  // time.
  ui::test::ScopedFakeNSWindowFullscreen faked_fullscreen_;
#endif
};

#endif  // CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
