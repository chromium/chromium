// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_GVR_CONSENT_HELPER_H_
#define CHROME_BROWSER_VR_SERVICE_GVR_CONSENT_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/vr/service/xr_consent_prompt_level.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

typedef base::OnceCallback<void(XrConsentPromptLevel, bool)>
    OnUserConsentCallback;

// Used for displaying a user consent dialog. Breaks the cyclic dependency
// between device/vr and the Android UI code in browser.
class VR_EXPORT GvrConsentHelper {
 public:
  static void SetInstance(std::unique_ptr<GvrConsentHelper>);

  static GvrConsentHelper* GetInstance();

  virtual ~GvrConsentHelper();

  virtual void PromptUserAndGetConsent(int render_process_id,
                                       int render_frame_id,
                                       XrConsentPromptLevel consent_level,
                                       OnUserConsentCallback) = 0;

 protected:
  GvrConsentHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(GvrConsentHelper);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_GVR_CONSENT_HELPER_H_
