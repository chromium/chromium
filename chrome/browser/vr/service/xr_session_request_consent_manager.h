// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_SESSION_REQUEST_CONSENT_MANAGER_H_
#define CHROME_BROWSER_VR_SERVICE_XR_SESSION_REQUEST_CONSENT_MANAGER_H_

#include "base/callback.h"
#include "chrome/browser/vr/service/xr_consent_prompt_level.h"
#include "chrome/browser/vr/vr_export.h"

namespace content {
class WebContents;
}

class TabModalConfirmDialog;

namespace vr {

// Abstract class to break a dependency loop between the "vr_common" component
// when accessing "chrome/browser/ui" component functionality such as
// the TabModalConfirmDialogDelegate. A concrete
// XRSessionRequestConsentManagerImpl object is injected through
// SetInstance() in chrome_browser_main.cc.
class VR_EXPORT XRSessionRequestConsentManager {
 public:
  // Must be called only after either SetInstance() or SetInstanceForTesting()
  // are called. If both are called, the pointer set using
  // SetInstanceForTesting() is returned.
  static XRSessionRequestConsentManager* Instance();

  // Must be called only once. The passed-in pointer is not owned. |instance|
  // cannot be nullptr.
  static void SetInstance(XRSessionRequestConsentManager* instance);

  // Can be called any number of times. The passed-in pointer is not owned.
  // |instance| can be nullptr.
  static void SetInstanceForTesting(XRSessionRequestConsentManager* instance);

  XRSessionRequestConsentManager();
  virtual ~XRSessionRequestConsentManager();

  // Displays a tab-modal consent dialog passing a delegate instantiated
  // using |web_contents| as an argument.
  // |response_callback| is guaranteed to be called with 'true' as arg if
  // the user presses the 'accept' button, or with 'false' if the user
  // either closes the dialog by any means or clicks on 'cancel' button.
  virtual TabModalConfirmDialog* ShowDialogAndGetConsent(
      content::WebContents* web_contents,
      XrConsentPromptLevel consent_level,
      base::OnceCallback<void(XrConsentPromptLevel, bool)>
          response_callback) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_SESSION_REQUEST_CONSENT_MANAGER_H_
