// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_WEBVR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_WEBVR_BROWSER_TEST_H_

#include "build/build_config.h"
#include "chrome/browser/vr/test/conditional_skipping.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/vr/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "chrome/browser/vr/test/mock_xr_session_request_consent_manager.h"
#include "services/service_manager/sandbox/features.h"
#endif

namespace vr {

// WebVR-specific test base class.
class WebVrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  bool XrDeviceFound(content::WebContents* web_contents) override;
  void EnterSessionWithUserGesture(content::WebContents* web_contents) override;
  void EnterSessionWithUserGestureOrFail(
      content::WebContents* web_contents) override;
  void EndSession(content::WebContents* web_contents) override;
  void EndSessionOrFail(content::WebContents* web_contents) override;

  // Necessary to use the WebContents-less versions of functions.
  using WebXrBrowserTestBase::XrDeviceFound;
  using WebXrBrowserTestBase::EnterSessionWithUserGesture;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail;
  using WebXrBrowserTestBase::EndSession;
  using WebXrBrowserTestBase::EndSessionOrFail;
};

// Test class with OpenVR support disabled.
class WebVrRuntimelessBrowserTest : public WebVrBrowserTestBase {
 public:
  WebVrRuntimelessBrowserTest() {
    enable_blink_features_.push_back("WebVR");

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};

// OpenVR feature only defined on Windows.
#ifdef OS_WIN
// Test class with standard features enabled: WebVR and OpenVR support.
class WebVrOpenVrBrowserTest : public WebVrBrowserTestBase {
 public:
  WebVrOpenVrBrowserTest() {
    enable_blink_features_.push_back("WebVR");
    enable_features_.push_back(features::kOpenVR);

    runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};

// Test class with WebVR disabled.
class WebVrOpenVrBrowserTestWebVrDisabled : public WebVrBrowserTestBase {
 public:
  WebVrOpenVrBrowserTestWebVrDisabled() {
    enable_features_.push_back(features::kOpenVR);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};

#if BUILDFLAG(ENABLE_OPENXR)
// OpenXR Test class with standard features enabled: WebVR, OpenXR support
// and the Gamepad API.
class WebVrOpenXrBrowserTest : public WebVrBrowserTestBase {
 public:
  WebVrOpenXrBrowserTest() {
    enable_blink_features_.push_back("WebVR");
    enable_features_.push_back(features::kOpenXR);

    runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};
#endif  // BUILDFLAG(ENABLE_OPENXR)
#endif  // OS_WIN

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBVR_BROWSER_TEST_H_
