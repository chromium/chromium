// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webxr_browser_test.h"
#include "content/public/browser/web_contents.h"

namespace vr {

bool WebXrBrowserTestBase::XrDeviceFound(content::WebContents* web_contents) {
  return RunJavaScriptAndExtractBoolOrFail("xrDevice != null", web_contents);
}

void WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait(
    content::WebContents* web_contents) {
  EnterSessionWithUserGesture(web_contents);
  WaitOnJavaScriptStep(web_contents);
}

bool WebXrBrowserTestBase::XrDeviceFound() {
  return XrDeviceFound(GetCurrentWebContents());
}

void WebXrBrowserTestBase::EnterSessionWithUserGesture() {
  EnterSessionWithUserGesture(GetCurrentWebContents());
}

void WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait() {
  EnterSessionWithUserGestureAndWait(GetCurrentWebContents());
}

void WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail() {
  EnterSessionWithUserGestureOrFail(GetCurrentWebContents());
}

void WebXrBrowserTestBase::EndSession() {
  EndSession(GetCurrentWebContents());
}

void WebXrBrowserTestBase::EndSessionOrFail() {
  EndSessionOrFail(GetCurrentWebContents());
}

}  // namespace vr
