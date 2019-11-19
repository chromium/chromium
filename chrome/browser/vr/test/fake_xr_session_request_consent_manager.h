// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_FAKE_XR_SESSION_REQUEST_CONSENT_MANAGER_H_
#define CHROME_BROWSER_VR_TEST_FAKE_XR_SESSION_REQUEST_CONSENT_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/vr/service/xr_session_request_consent_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class FakeXRSessionRequestConsentManager
    : public XRSessionRequestConsentManager {
 public:
  enum class UserResponse {
    kClickAllowButton,
    kClickCancelButton,
    kCloseDialog,
  };

  FakeXRSessionRequestConsentManager(
      XRSessionRequestConsentManager* consent_manager,
      UserResponse user_response);
  ~FakeXRSessionRequestConsentManager() override;

  TabModalConfirmDialog* ShowDialogAndGetConsent(
      content::WebContents* web_contents,
      XrConsentPromptLevel conesent_level,
      base::OnceCallback<void(XrConsentPromptLevel, bool)> response_callback)
      override;

  uint32_t ShownCount() { return shown_count_; }

 private:
  XRSessionRequestConsentManager* consent_manager_;
  UserResponse user_response_;
  uint32_t shown_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeXRSessionRequestConsentManager);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_FAKE_XR_SESSION_REQUEST_CONSENT_MANAGER_H_
