// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/arcore_consent_prompt_interface.h"

namespace vr {

namespace {
ArCoreConsentPromptInterface* g_arcore_consent_prompt = nullptr;
}

// static
void ArCoreConsentPromptInterface::SetInstance(
    ArCoreConsentPromptInterface* instance) {
  g_arcore_consent_prompt = instance;
}

// static
ArCoreConsentPromptInterface* ArCoreConsentPromptInterface::GetInstance() {
  return g_arcore_consent_prompt;
}

}  // namespace vr
