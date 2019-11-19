// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_ARCORE_CONSENT_PROMPT_INTERFACE_H_
#define CHROME_BROWSER_VR_SERVICE_ARCORE_CONSENT_PROMPT_INTERFACE_H_

#include "base/callback.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

// TODO(crbug.com/968233): Unify consent flow.
// This class solves layering problem until the above bug gets fixed.
class VR_EXPORT ArCoreConsentPromptInterface {
 public:
  static void SetInstance(ArCoreConsentPromptInterface*);
  static ArCoreConsentPromptInterface* GetInstance();

  virtual void ShowConsentPrompt(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> response_callback) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_ARCORE_CONSENT_PROMPT_INTERFACE_H_
