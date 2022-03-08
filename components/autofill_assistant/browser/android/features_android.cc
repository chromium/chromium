// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "components/autofill_assistant/android/jni_headers_public/AssistantFeatures_jni.h"
#include "components/autofill_assistant/browser/features.h"

namespace autofill_assistant {
namespace {

// Array of features exposed through the Java Features bridge class. Entries in
// this array must be replicated in the same order in AssistantFeatures.java.
const std::array<const base::Feature*, 8> kFeaturesExposedToJava = {
    &features::kAutofillAssistant,
    &features::kAutofillAssistantChromeEntry,
    &features::kAutofillAssistantDirectActions,
    &features::kAutofillAssistantDisableOnboardingFlow,
    &features::kAutofillAssistantDisableProactiveHelpTiedToMSBB,
    &features::kAutofillAssistantFeedbackChip,
    &features::kAutofillAssistantLoadDFMForTriggerScripts,
    &features::kAutofillAssistantProactiveHelp,
};

}  // namespace

static jlong JNI_AssistantFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  DCHECK_LE(ordinal, static_cast<int>(kFeaturesExposedToJava.size()));
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace autofill_assistant
