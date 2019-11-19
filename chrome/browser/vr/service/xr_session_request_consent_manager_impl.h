// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_SESSION_REQUEST_CONSENT_MANAGER_IMPL_H_
#define CHROME_BROWSER_VR_SERVICE_XR_SESSION_REQUEST_CONSENT_MANAGER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/vr/service/xr_session_request_consent_manager.h"

namespace vr {

// Concrete implementation of XRSessionRequestConsentManager, part of
// "browser" component. Used on the browser's main thread.
class XRSessionRequestConsentManagerImpl
    : public XRSessionRequestConsentManager {
 public:
  XRSessionRequestConsentManagerImpl();
  ~XRSessionRequestConsentManagerImpl() override;

  // XRSessionRequestConsentManager:
  TabModalConfirmDialog* ShowDialogAndGetConsent(
      content::WebContents* web_contents,
      XrConsentPromptLevel consent_level,
      base::OnceCallback<void(XrConsentPromptLevel, bool)> response_callback)
      override;

 private:
  DISALLOW_COPY_AND_ASSIGN(XRSessionRequestConsentManagerImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_SESSION_REQUEST_CONSENT_MANAGER_IMPL_H_
