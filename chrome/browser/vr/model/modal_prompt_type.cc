// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/modal_prompt_type.h"

#include "base/notreached.h"

namespace vr {

UiUnsupportedMode GetReasonForPrompt(ModalPromptType prompt) {
  switch (prompt) {
    case kModalPromptTypeExitVRForSiteInfo:
      return UiUnsupportedMode::kUnhandledPageInfo;
    case kModalPromptTypeExitVRForCertificateInfo:
      return UiUnsupportedMode::kUnhandledCertificateInfo;
    case kModalPromptTypeExitVRForConnectionSecurityInfo:
      return UiUnsupportedMode::kUnhandledConnectionSecurityInfo;
    case kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission:
      return UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission;
    case kModalPromptTypeGenericUnsupportedFeature:
      return UiUnsupportedMode::kGenericUnsupportedFeature;
    case kModalPromptTypeUpdateKeyboard:
      return UiUnsupportedMode::kNeedsKeyboardUpdate;
    case kModalPromptTypeNone:
      return UiUnsupportedMode::kCount;
    case kNumModalPromptTypes:
      break;
  }
  NOTREACHED();
  return UiUnsupportedMode::kCount;
}

}  // namespace vr
