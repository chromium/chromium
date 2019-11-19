// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_XR_SESSION_REQUEST_CONSENT_MANAGER_H_
#define CHROME_BROWSER_VR_TEST_MOCK_XR_SESSION_REQUEST_CONSENT_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/vr/service/xr_session_request_consent_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockXRSessionRequestConsentManager
    : public XRSessionRequestConsentManager {
 public:
  MockXRSessionRequestConsentManager();
  ~MockXRSessionRequestConsentManager() override;

  MOCK_METHOD3(ShowDialogAndGetConsent,
               TabModalConfirmDialog*(
                   content::WebContents* web_contents,
                   XrConsentPromptLevel consent_level,
                   base::OnceCallback<void(XrConsentPromptLevel, bool)>
                       response_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXRSessionRequestConsentManager);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_XR_SESSION_REQUEST_CONSENT_MANAGER_H_
