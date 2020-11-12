// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "components/permissions/permission_prompt.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace permissions {
class PermissionRequest;
}

class TestPermissionBubbleViewDelegate
    : public permissions::PermissionPrompt::Delegate {
 public:
  TestPermissionBubbleViewDelegate();
  ~TestPermissionBubbleViewDelegate() override;

  const std::vector<permissions::PermissionRequest*>& Requests() override;

  GURL GetRequestingOrigin() const override;

  GURL GetEmbeddingOrigin() const override;

  void Accept() override {}
  void AcceptThisTime() override {}
  void Deny() override {}
  void Closing() override {}

  bool WasCurrentRequestAlreadyDisplayed() override;

  void set_requests(std::vector<permissions::PermissionRequest*> requests) {
    requests_ = requests;
  }

 private:
  std::vector<permissions::PermissionRequest*> requests_;

  DISALLOW_COPY_AND_ASSIGN(TestPermissionBubbleViewDelegate);
};

// Use this class to test on a default window or an app window. Inheriting from
// ExtensionBrowserTest allows us to easily load and launch apps, and doesn't
// really add any extra work.
class PermissionBubbleBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  PermissionBubbleBrowserTest();
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

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleBrowserTest);
};

// Use this class to test on a kiosk window.
class PermissionBubbleKioskBrowserTest : public PermissionBubbleBrowserTest {
 public:
  PermissionBubbleKioskBrowserTest();
  ~PermissionBubbleKioskBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
#if defined(OS_MAC)
  // Toggling fullscreen mode on Mac can be flaky for tests run in parallel
  // because only one window may be animating into or out of fullscreen at a
  // time.
  ui::test::ScopedFakeNSWindowFullscreen faked_fullscreen_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleKioskBrowserTest);
};

#endif  // CHROME_BROWSER_UI_PERMISSION_BUBBLE_PERMISSION_BUBBLE_BROWSER_TEST_UTIL_H_
