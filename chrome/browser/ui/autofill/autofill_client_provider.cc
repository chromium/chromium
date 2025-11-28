// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_client_provider.h"

#include "base/check_deref.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/autofill/android/android_autofill_availability_status.h"
#include "components/android_autofill/browser/android_autofill_client.h"
#include "components/prefs/android/pref_service_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillClientProviderUtils_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {
namespace {

#if BUILDFLAG(IS_ANDROID)
void RecordAvailabilityStatus(AndroidAutofillAvailabilityStatus availability) {
  base::UmaHistogramEnumeration("Autofill.AndroidAutofillAvailabilityStatus",
                                availability);
}

// Counts how often the Chrome pref is reset because an platform autofill
// isn't allowed or doesn't fulfill all preconditions.
void RecordWhetherAndroidPrefResets(PrefService& prefs,
                                    bool uses_platform_autofill) {
  const bool will_reset_pref =
      prefs.GetBoolean(prefs::kAutofillUsingVirtualViewStructure) &&
      !uses_platform_autofill;
  base::UmaHistogramBoolean("Autofill.ResetAutofillPrefToChrome",
                            will_reset_pref);
}

// Sets a ahread pref that allows to learn whether deep-links into Chrome's
// settings are available to use.
void SetSharedPrefForDeepLink() {
  Java_AutofillClientProviderUtils_setAutofillOptionsDeepLinkPref(
      base::android::AttachCurrentThread(),

      base::FeatureList::IsEnabled(
          autofill::features::kAutofillDeepLinkAutofillOptions));
}

// Sets a shared pref that allows external apps to use a ContentResolver to
// figure out whether Chrome is using platform autofill over the default.
void SetSharedPrefForSettingsContentProvider(bool uses_platform_autofill) {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillThirdPartyModeContentProvider)) {
    Java_AutofillClientProviderUtils_setThirdPartyModePref(
        base::android::AttachCurrentThread(), uses_platform_autofill);
  } else {
    Java_AutofillClientProviderUtils_unsetThirdPartyModePref(
        base::android::AttachCurrentThread());
  }
}

AndroidAutofillAvailabilityStatus GetAndroidAutofillAvailabilityStatus(
    PrefService& prefs) {
  return static_cast<AndroidAutofillAvailabilityStatus>(
      Java_AutofillClientProviderUtils_getAndroidAutofillFrameworkAvailability(
          base::android::AttachCurrentThread(), &prefs));
}
#endif  // BUILDFLAG(IS_ANDROID)

bool UsesVirtualViewStructureForAutofill(PrefService& prefs) {
#if BUILDFLAG(IS_ANDROID)
  const AndroidAutofillAvailabilityStatus availability =
      GetAndroidAutofillAvailabilityStatus(prefs);
  RecordAvailabilityStatus(availability);
  return availability == AndroidAutofillAvailabilityStatus::kAvailable;
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

AutofillClientProvider::AutofillClientProvider(PrefService* prefs)
    : uses_platform_autofill_(
          UsesVirtualViewStructureForAutofill(CHECK_DEREF(prefs))) {
#if BUILDFLAG(IS_ANDROID)
  RecordWhetherAndroidPrefResets(*prefs, uses_platform_autofill_);
  // Ensure the pref is reset if platform autofill is restricted.
  prefs->SetBoolean(prefs::kAutofillUsingVirtualViewStructure,
                    uses_platform_autofill_);
  SetSharedPrefForSettingsContentProvider(uses_platform_autofill_);
  SetSharedPrefForDeepLink();
#endif  // BUILDFLAG(IS_ANDROID)
}

AutofillClientProvider::~AutofillClientProvider() = default;

void AutofillClientProvider::CreateClientForWebContents(
    content::WebContents* web_contents) {
  if (uses_platform_autofill()) {
#if BUILDFLAG(IS_ANDROID)
    android_autofill::AndroidAutofillClient::CreateForWebContents(web_contents);
#else
    NOTREACHED();
#endif
  } else {
    ChromeAutofillClient::CreateForWebContents(web_contents);
  }
}

}  // namespace autofill

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(AutofillClientProviderUtils)
#endif
