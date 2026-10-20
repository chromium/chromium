// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/enterprise_signals_disclaimer/enterprise_signals_disclaimer_bridge.h"

#include <jni.h>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/signin_prefs.h"
#include "google_apis/gaia/gaia_id.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/enterprise_signals_disclaimer/jni_headers/EnterpriseSignalsDisclaimerBridge_jni.h"

namespace enterprise_signals {

namespace {

void SetDeviceSignalsPermanentConsentReceived(Profile* profile,
                                              const GaiaId& gaia_id,
                                              bool received) {
  CHECK(profile);
  CHECK(!gaia_id.empty());
  SigninPrefs(*profile->GetPrefs())
      .SetDeviceSignalsPermanentConsentReceived(gaia_id, received);
}

}  // namespace

static bool
JNI_EnterpriseSignalsDisclaimerBridge_HasAccountAcknowledgedSignalsDisclaimer(
    JNIEnv* env,
    Profile* profile,
    const GaiaId& gaia_id) {
  CHECK(profile);
  CHECK(!gaia_id.empty());
  return SigninPrefs(*profile->GetPrefs())
      .GetDeviceSignalsPermanentConsentReceived(gaia_id);
}

static void
JNI_EnterpriseSignalsDisclaimerBridge_SetAccountAcknowledgedSignalsDisclaimer(
    JNIEnv* env,
    Profile* profile,
    const GaiaId& gaia_id) {
  SetDeviceSignalsPermanentConsentReceived(profile, gaia_id,
                                           /*received=*/true);
}

static void
JNI_EnterpriseSignalsDisclaimerBridge_ClearAccountAcknowledgedSignalsDisclaimer(
    JNIEnv* env,
    Profile* profile,
    const GaiaId& gaia_id) {
  SetDeviceSignalsPermanentConsentReceived(profile, gaia_id,
                                           /*received=*/false);
}

DEFINE_JNI(EnterpriseSignalsDisclaimerBridge)

}  // namespace enterprise_signals
