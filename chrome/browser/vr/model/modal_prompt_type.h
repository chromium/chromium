// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_MODAL_PROMPT_TYPE_H_
#define CHROME_BROWSER_VR_MODEL_MODAL_PROMPT_TYPE_H_

#include "chrome/browser/vr/ui_unsupported_mode.h"

namespace vr {

enum ModalPromptType {
  kModalPromptTypeNone,
  kModalPromptTypeExitVRForSiteInfo,
  kModalPromptTypeExitVRForCertificateInfo,
  kModalPromptTypeExitVRForConnectionSecurityInfo,
  kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission,
  kModalPromptTypeGenericUnsupportedFeature,
  kModalPromptTypeUpdateKeyboard,

  kNumModalPromptTypes
};

UiUnsupportedMode GetReasonForPrompt(ModalPromptType prompt);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_MODAL_PROMPT_TYPE_H_
