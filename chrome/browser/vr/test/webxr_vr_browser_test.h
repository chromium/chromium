// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_

#include "build/build_config.h"
#include "chrome/browser/vr/test/conditional_skipping.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/webxr_browser_test.h"
#include "chrome/browser/vr/test/xr_browser_test.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "device/base/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/gfx/geometry/vector3d_f.h"

#if defined(OS_WIN)
#include "sandbox/policy/features.h"
#endif

namespace vr {

// WebXR for VR-specific test base class without any particular runtime.
class WebXrVrBrowserTestBase : public WebXrBrowserTestBase {
 public:
  WebXrVrBrowserTestBase();
  ~WebXrVrBrowserTestBase() override;
  void EnterSessionWithUserGesture(content::WebContents* web_contents) override;
  void EnterSessionWithUserGestureOrFail(
      content::WebContents* web_contents) override;
  void EndSession(content::WebContents* web_contents) override;
  void EndSessionOrFail(content::WebContents* web_contents) override;

  permissions::PermissionRequestManager* GetPermissionRequestManager();
  permissions::PermissionRequestManager* GetPermissionRequestManager(
      content::WebContents* web_contents);

  virtual gfx::Vector3dF GetControllerOffset() const;

  // Necessary to use the WebContents-less versions of functions.
  using WebXrBrowserTestBase::XrDeviceFound;
  using WebXrBrowserTestBase::EnterSessionWithUserGesture;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail;
  using WebXrBrowserTestBase::EndSession;
  using WebXrBrowserTestBase::EndSessionOrFail;

  permissions::PermissionRequestManager::AutoResponseType
      permission_auto_response_ =
          permissions::PermissionRequestManager::ACCEPT_ALL;
};

// Test class with all runtimes disabled.
class WebXrVrRuntimelessBrowserTest : public WebXrVrBrowserTestBase {
 public:
  WebXrVrRuntimelessBrowserTest();
};

class WebXrVrRuntimelessBrowserTestSensorless
    : public WebXrVrRuntimelessBrowserTest {
 public:
  WebXrVrRuntimelessBrowserTestSensorless();
};

// WMR feature only defined on Windows.
#ifdef OS_WIN
// WMR-specific subclass of WebXrVrBrowserTestBase.
class WebXrVrWmrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  WebXrVrWmrBrowserTestBase();
  ~WebXrVrWmrBrowserTestBase() override;
  void PreRunTestOnMainThread() override;
  // WMR enabled by default, so no need to add anything in the constructor.
  XrBrowserTestBase::RuntimeType GetRuntimeType() const override;

 private:
  // We create this before the test starts so that a test hook is always
  // registered, and thus the mock WMR wrappers are always used in tests. If a
  // test needs to actually use the test hook for input, then the one the test
  // creates will simply be registered over this one.
  std::unique_ptr<MockXRDeviceHookBase> dummy_hook_;
};

#if BUILDFLAG(ENABLE_OPENXR)
// OpenXR-specific subclass of WebXrVrBrowserTestBase.
class WebXrVrOpenXrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  WebXrVrOpenXrBrowserTestBase();
  ~WebXrVrOpenXrBrowserTestBase() override;
  XrBrowserTestBase::RuntimeType GetRuntimeType() const override;
};
#endif  // BUILDFLAG(ENABLE_OPENXR)

class WebXrVrWmrBrowserTest : public WebXrVrWmrBrowserTestBase {
 public:
  WebXrVrWmrBrowserTest();
};

#if BUILDFLAG(ENABLE_OPENXR)
class WebXrVrOpenXrBrowserTest : public WebXrVrOpenXrBrowserTestBase {
 public:
  WebXrVrOpenXrBrowserTest();
};
#endif  // BUILDFLAG(ENABLE_OPENXR)

class WebXrVrWmrBrowserTestWebXrDisabled : public WebXrVrWmrBrowserTestBase {
 public:
  WebXrVrWmrBrowserTestWebXrDisabled();
};

#if BUILDFLAG(ENABLE_OPENXR)
class WebXrVrOpenXrBrowserTestWebXrDisabled
    : public WebXrVrOpenXrBrowserTestBase {
 public:
  WebXrVrOpenXrBrowserTestWebXrDisabled();
};
#endif  // BUIDFLAG(ENABLE_OPENXR)

#endif  // OS_WIN

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
