// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_WEBXR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_WEBXR_BROWSER_TEST_H_

#include "chrome/browser/vr/test/xr_browser_test.h"
#include "content/public/browser/web_contents.h"

namespace vr {

// Base browser test class for running WebXR/WebVR-related tests.
class WebXrBrowserTestBase : public XrBrowserTestBase {
 public:
  // Checks whether an XRDevice was actually found.
  virtual bool XrDeviceFound(content::WebContents* web_contents);

  // Enters a WebXR or WebVR session of some kind.
  virtual void EnterSessionWithUserGesture(
      content::WebContents* web_contents) = 0;

  // Enters a WebXR or WebVR session of some kind and waits until the page
  // page reports it is finished with its JavaScript step.
  void EnterSessionWithUserGestureAndWait(content::WebContents* web_contents);

  // Attempts to enter a WebXR or WebVR session of some kind, failing if it is
  // unable to.
  virtual void EnterSessionWithUserGestureOrFail(
      content::WebContents* web_contents) = 0;

  // Ends whatever type of session a subclass enters with
  // EnterSessionWithUserGesture.
  virtual void EndSession(content::WebContents* web_contents) = 0;

  // Attempts to end whatever type of session a subclass enters with
  // EnterSessionWithUserGesture, failing if it is unable to.
  virtual void EndSessionOrFail(content::WebContents* web_contents) = 0;

  // Convenience function for calling XrDeviceFound with the return value of
  // GetCurrentWebContents.
  bool XrDeviceFound();

  // Convenience function for calling EnterSessionWithUserGesture with the
  // return value of GetCurrentWebContents.
  void EnterSessionWithUserGesture();

  // Convenience function for calling EnterSessionWithUserGestureAndWait with
  // the return value of GetCurrentWebContents.
  void EnterSessionWithUserGestureAndWait();

  // Convenience function for calling EnterSessionWithUserGestureOrFail with the
  // return value of GetCurrentWebContents.
  void EnterSessionWithUserGestureOrFail();

  // Convenience function for calling EndSession with the return value of
  // GetCurrentWebContents.
  void EndSession();

  // Convenience function for calling EndSessionOrFail with the return value of
  // GetCurrentWebContents.
  void EndSessionOrFail();
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBXR_BROWSER_TEST_H_
