// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_client_provider.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/autofill/android/android_autofill_availability_status.h"
#include "chrome/browser/autofill/android/jni_headers/AutofillClientProviderUtils_jni.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "components/android_autofill/browser/android_autofill_client.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {
namespace {

#if BUILDFLAG(IS_ANDROID)
AndroidAutofillAvailabilityStatus getAndroidAutofillAvailabilityStatus(
    PrefService& prefs) {
  return static_cast<AndroidAutofillAvailabilityStatus>(
      Java_AutofillClientProviderUtils_getAndroidAutofillFrameworkAvailability(
          base::android::AttachCurrentThread(), prefs.GetJavaObject()));
}
#endif

bool UsesVirtualViewStructureForAutofill(PrefService& prefs) {
#if BUILDFLAG(IS_ANDROID)
  switch (getAndroidAutofillAvailabilityStatus(prefs)) {
    case AndroidAutofillAvailabilityStatus::kAvailable:
      return true;
    case AndroidAutofillAvailabilityStatus::kSettingTurnedOff:
    case AndroidAutofillAvailabilityStatus::kNotAllowedByPolicy:
      return false;
    case AndroidAutofillAvailabilityStatus::kAndroidVersionTooOld:
    case AndroidAutofillAvailabilityStatus::kAndroidAutofillManagerNotAvailable:
    case AndroidAutofillAvailabilityStatus::kAndroidAutofillNotSupported:
    case AndroidAutofillAvailabilityStatus::kUnknownAndroidAutofillService:
    case AndroidAutofillAvailabilityStatus::kAndroidAutofillServiceIsGoogle:
      return features::
          kAutofillVirtualViewStructureAndroidSkipsCompatibilityCheck.Get();
  }
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

AutofillClientProvider::AutofillClientProvider(PrefService* prefs)
    : uses_platform_autofill_(UsesVirtualViewStructureForAutofill(*prefs)) {
#if BUILDFLAG(IS_ANDROID)
  // Ensure the pref is reset if platform autofill is restricted.
  prefs->SetBoolean(prefs::kAutofillUsingVirtualViewStructure,
                    uses_platform_autofill_);
#endif  // BUILDFLAG(IS_ANDROID)
}

AutofillClientProvider::~AutofillClientProvider() = default;

void AutofillClientProvider::CreateClientForWebContents(
    content::WebContents* web_contents) {
  if (uses_platform_autofill()) {
#if BUILDFLAG(IS_ANDROID)
    android_autofill::AndroidAutofillClient::CreateForWebContents(web_contents);
#else
    NOTREACHED_IN_MIGRATION();
#endif
  } else {
    ChromeAutofillClient::CreateForWebContents(web_contents);
  }
}

}  // namespace autofill
