// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_SPEECH_RECOGNITION_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_SPEECH_RECOGNITION_MODEL_H_

#include <string>

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

struct VR_BASE_EXPORT SpeechRecognitionModel {
  int speech_recognition_state = 0;
  bool has_or_can_request_record_audio_permission = true;
  std::u16string recognition_result;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_SPEECH_RECOGNITION_MODEL_H_
