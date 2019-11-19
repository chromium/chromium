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
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "device/base/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/gfx/geometry/vector3d_f.h"

#if defined(OS_WIN)
#include "services/service_manager/sandbox/features.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "chrome/browser/vr/test/fake_xr_session_request_consent_manager.h"
#include "chrome/browser/vr/test/mock_xr_session_request_consent_manager.h"
#endif  // BUILDFLAG(ENABLE_VR)

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

  virtual gfx::Vector3dF GetControllerOffset() const;

  // Necessary to use the WebContents-less versions of functions.
  using WebXrBrowserTestBase::XrDeviceFound;
  using WebXrBrowserTestBase::EnterSessionWithUserGesture;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail;
  using WebXrBrowserTestBase::EndSession;
  using WebXrBrowserTestBase::EndSessionOrFail;

  // Methods/objects for managing consent. If SetupFakeConsentManager is never
  // called, the test will default to mocking out the consent prompt and always
  // provide consent. Once SetupFakeConsentManager is called, the test will show
  // the Consent Dialog, and then rely on it's configuration for whether to
  // accept or reject the dialog programmatically. While this is a more thorough
  // end-to-end test, the extra overhead should be avoided unless that is the
  // feature under test.
  // Consent dialogs don't appear on platforms with enable_vr = false.
#if BUILDFLAG(ENABLE_VR)
  void SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse user_response);
  ::testing::NiceMock<MockXRSessionRequestConsentManager> consent_manager_;
  std::unique_ptr<FakeXRSessionRequestConsentManager> fake_consent_manager_;
#endif  // BUILDFLAG(ENABLE_VR)
};

// Test class with OpenVR disabled.
class WebXrVrRuntimelessBrowserTest : public WebXrVrBrowserTestBase {
 public:
  WebXrVrRuntimelessBrowserTest();
};

class WebXrVrRuntimelessBrowserTestSensorless
    : public WebXrVrRuntimelessBrowserTest {
 public:
  WebXrVrRuntimelessBrowserTestSensorless();
};

// OpenVR and WMR feature only defined on Windows.
#ifdef OS_WIN
// OpenVR-specific subclass of WebXrVrBrowserTestBase.
class WebXrVrOpenVrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  WebXrVrOpenVrBrowserTestBase();
  XrBrowserTestBase::RuntimeType GetRuntimeType() const override;
  gfx::Vector3dF GetControllerOffset() const override;
};

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

// Test class with standard features enabled: WebXR and OpenVR.
class WebXrVrOpenVrBrowserTest : public WebXrVrOpenVrBrowserTestBase {
 public:
  WebXrVrOpenVrBrowserTest();
};

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

// Test classes with WebXR disabled.
class WebXrVrOpenVrBrowserTestWebXrDisabled
    : public WebXrVrOpenVrBrowserTestBase {
 public:
  WebXrVrOpenVrBrowserTestWebXrDisabled();
};

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
