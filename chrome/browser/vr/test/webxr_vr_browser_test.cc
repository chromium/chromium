// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;

namespace vr {

WebXrVrBrowserTestBase::WebXrVrBrowserTestBase() {
  enable_features_.push_back(features::kWebXr);
}

WebXrVrBrowserTestBase::~WebXrVrBrowserTestBase() = default;

void WebXrVrBrowserTestBase::EnterSessionWithUserGesture(
    content::WebContents* web_contents) {
  // Before requesting the session, set the requested auto-response so that the
  // session is appropriately granted or rejected (or the request ignored).
  GetPermissionRequestManager()->set_auto_response_for_test(
      permission_auto_response_);

  // ExecuteScript runs with a user gesture, so we can just directly call
  // requestSession instead of having to do the hacky workaround the
  // instrumentation tests use of actually sending a click event to the canvas.
  RunJavaScriptOrFail("onRequestSession()", web_contents);
}

void WebXrVrBrowserTestBase::EnterSessionWithUserGestureOrFail(
    content::WebContents* web_contents) {
  EnterSessionWithUserGesture(web_contents);
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
      kPollTimeoutLong, web_contents);

#if defined(OS_WIN)
  // For WMR, creating a session may take foreground from us, and Windows may
  // not return it when the session terminates. This means subsequent requests
  // to enter an immersive session may fail. The fix for testing is to call
  // SetForegroundWindow manually. In real code, we'll have foreground if there
  // was a user gesture to enter VR.
  SetForegroundWindow(hwnd_);
#endif
}

void WebXrVrBrowserTestBase::EndSession(content::WebContents* web_contents) {
  RunJavaScriptOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
      web_contents);
}

void WebXrVrBrowserTestBase::EndSessionOrFail(
    content::WebContents* web_contents) {
  EndSession(web_contents);
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
      kPollTimeoutLong, web_contents);
}

gfx::Vector3dF WebXrVrBrowserTestBase::GetControllerOffset() const {
  return gfx::Vector3dF();
}

permissions::PermissionRequestManager*
WebXrVrBrowserTestBase::GetPermissionRequestManager() {
  return GetPermissionRequestManager(GetCurrentWebContents());
}

permissions::PermissionRequestManager*
WebXrVrBrowserTestBase::GetPermissionRequestManager(
    content::WebContents* web_contents) {
  return permissions::PermissionRequestManager::FromWebContents(web_contents);
}

WebXrVrRuntimelessBrowserTest::WebXrVrRuntimelessBrowserTest() {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  disable_features_.push_back(device::features::kWindowsMixedReality);
#endif
#if BUILDFLAG(ENABLE_OPENXR)
  disable_features_.push_back(device::features::kOpenXR);
#endif
}

WebXrVrRuntimelessBrowserTestSensorless::
    WebXrVrRuntimelessBrowserTestSensorless() {
  // WebXrOrientationSensorDevice is only defined when the enable_vr flag is
  // set.
#if BUILDFLAG(ENABLE_VR)
  disable_features_.push_back(device::kWebXrOrientationSensorDevice);
#endif  // BUILDFLAG(ENABLE_VR)
}

#if defined(OS_WIN)

WebXrVrWmrBrowserTestBase::WebXrVrWmrBrowserTestBase() {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  enable_features_.push_back(device::features::kWindowsMixedReality);
#endif
#if BUILDFLAG(ENABLE_OPENXR)
  disable_features_.push_back(device::features::kOpenXR);
#endif
}

WebXrVrWmrBrowserTestBase::~WebXrVrWmrBrowserTestBase() = default;

void WebXrVrWmrBrowserTestBase::PreRunTestOnMainThread() {
  dummy_hook_ = std::make_unique<MockXRDeviceHookBase>();
  WebXrVrBrowserTestBase::PreRunTestOnMainThread();
}

XrBrowserTestBase::RuntimeType WebXrVrWmrBrowserTestBase::GetRuntimeType()
    const {
  return XrBrowserTestBase::RuntimeType::RUNTIME_WMR;
}

#if BUILDFLAG(ENABLE_OPENXR)

WebXrVrOpenXrBrowserTestBase::WebXrVrOpenXrBrowserTestBase() {
  enable_features_.push_back(device::features::kOpenXR);
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  disable_features_.push_back(device::features::kWindowsMixedReality);
#endif
}

WebXrVrOpenXrBrowserTestBase::~WebXrVrOpenXrBrowserTestBase() = default;

XrBrowserTestBase::RuntimeType WebXrVrOpenXrBrowserTestBase::GetRuntimeType()
    const {
  return XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR;
}
#endif  // BUILDFLAG(ENABLE_OPENXR)

WebXrVrWmrBrowserTest::WebXrVrWmrBrowserTest() {
  runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);
}

#if BUILDFLAG(ENABLE_OPENXR)
WebXrVrOpenXrBrowserTest::WebXrVrOpenXrBrowserTest() {
  runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);
}
#endif  // BUILDFLAG(ENABLE_OPENXR)

// Test classes with WebXR disabled.
WebXrVrWmrBrowserTestWebXrDisabled::WebXrVrWmrBrowserTestWebXrDisabled() {
  disable_features_.push_back(features::kWebXr);
}

#if BUILDFLAG(ENABLE_OPENXR)
WebXrVrOpenXrBrowserTestWebXrDisabled::WebXrVrOpenXrBrowserTestWebXrDisabled() {
  disable_features_.push_back(features::kWebXr);
}
#endif  // BUIDFLAG(ENABLE_OPENXR)

#endif  // OS_WIN

}  // namespace vr
