// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webxr_vr_browser_test.h"

#include <cstring>

#include "build/build_config.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
#endif

#if BUILDFLAG(ENABLE_OPENXR)
#include "content/public/common/content_switches.h"
#endif

using testing::_;
using testing::Invoke;

namespace vr {

WebXrVrBrowserTestBase::WebXrVrBrowserTestBase() {
  enable_features_.push_back(features::kWebXr);
}

WebXrVrBrowserTestBase::~WebXrVrBrowserTestBase() = default;

void WebXrVrBrowserTestBase::EnterSessionWithUserGesture(
    content::WebContents* web_contents) {

  // ExecJs runs with a user gesture, so we can just directly call
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

#if BUILDFLAG(IS_WIN)
  // Creating a session may take foreground from us, and Windows may not return
  // it when the session terminates. This means subsequent requests to enter an
  // immersive session may fail. The fix for testing is to call
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
  WaitForSessionEndOrFail(web_contents);
}

void WebXrVrBrowserTestBase::WaitForSessionEndOrFail(
    content::WebContents* web_contents) {
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
      kPollTimeoutLong, web_contents);
}

gfx::Vector3dF WebXrVrBrowserTestBase::GetControllerOffset() const {
  return gfx::Vector3dF();
}

void WebXrVrBrowserTestBase::SetPermissionAutoResponse(
    permissions::PermissionRequestManager::AutoResponseType
        permission_auto_response) {
  permission_auto_response_ = permission_auto_response;
  for (auto& it : mock_permissions_map_) {
    it.second->set_response_type(permission_auto_response_);
  }
}

permissions::MockPermissionPromptFactory*
WebXrVrBrowserTestBase::GetPermissionPromptFactory() {
  auto* web_contents = GetCurrentWebContents();
  CHECK(web_contents);
  if (!mock_permissions_map_.contains(web_contents)) {
    return nullptr;
  }

  return mock_permissions_map_[web_contents].get();
}

void WebXrVrBrowserTestBase::OnBeforeLoadFile() {
  auto* web_contents = GetCurrentWebContents();
  CHECK(web_contents);
  if (!mock_permissions_map_.contains(web_contents)) {
    mock_permissions_map_.insert_or_assign(
        web_contents,
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permissions::PermissionRequestManager::FromWebContents(
                GetCurrentWebContents())));
    // Set the requested auto-response so that any session is appropriately
    // granted or rejected (or the request ignored).
    mock_permissions_map_[web_contents]->set_response_type(
        permission_auto_response_);
  }
}

WebXrVrRuntimelessBrowserTest::WebXrVrRuntimelessBrowserTest() {
#if BUILDFLAG(ENABLE_OPENXR)
  disable_features_.push_back(device::features::kOpenXR);
#endif
}

WebXrVrRuntimelessBrowserTestSensorless::
    WebXrVrRuntimelessBrowserTestSensorless() {
  // WebXrOrientationSensorDevice is only defined when the enable_vr flag is
  // set.
#if BUILDFLAG(ENABLE_VR)
  disable_features_.push_back(device::features::kWebXrOrientationSensorDevice);
#endif  // BUILDFLAG(ENABLE_VR)
}

#if BUILDFLAG(ENABLE_OPENXR)
WebXrVrOpenXrBrowserTestBase::WebXrVrOpenXrBrowserTestBase() {
  enable_features_.push_back(device::features::kOpenXR);
  enable_features_.push_back(device::features::kWebXrHandInput);
}

WebXrVrOpenXrBrowserTestBase::~WebXrVrOpenXrBrowserTestBase() = default;

XrBrowserTestBase::RuntimeType WebXrVrOpenXrBrowserTestBase::GetRuntimeType()
    const {
  return XrBrowserTestBase::RuntimeType::RUNTIME_OPENXR;
}

void WebXrVrOpenXrBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kWebXrForceRuntime,
                                  switches::kWebXrRuntimeOpenXr);
}

WebXrVrOpenXrBrowserTest::WebXrVrOpenXrBrowserTest() {
#if BUILDFLAG(IS_WIN)
  runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);
#endif
}

WebXrVrOpenXrBrowserTestWebXrDisabled::WebXrVrOpenXrBrowserTestWebXrDisabled() {
  disable_features_.push_back(features::kWebXr);
}
#endif  // BUIDFLAG(ENABLE_OPENXR)

}  // namespace vr
