// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/enterprise_signals_disclaimer/enterprise_signals_disclaimer_bridge.h"

#include <jni.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/android/enterprise_signals_disclaimer/acknowledgment_manager.h"
#include "google_apis/gaia/gaia_id.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/enterprise_signals_disclaimer/jni_headers/EnterpriseSignalsDisclaimerBridge_jni.h"

namespace enterprise_signals {

static bool
JNI_EnterpriseSignalsDisclaimerBridge_HasAccountAcknowledgedSignalsDisclaimer(
    JNIEnv* env,
    const GaiaId& gaia_id) {
  return enterprise_signals_disclaimer::HasAccountAcknowledgedSignalsDisclaimer(
      g_browser_process->local_state(), gaia_id);
}

static void
JNI_EnterpriseSignalsDisclaimerBridge_SetAccountAcknowledgedSignalsDisclaimer(
    JNIEnv* env,
    const GaiaId& gaia_id) {
  enterprise_signals_disclaimer::SetAccountAcknowledgedSignalsDisclaimer(
      g_browser_process->local_state(), gaia_id);
}

DEFINE_JNI(EnterpriseSignalsDisclaimerBridge)

}  // namespace enterprise_signals
