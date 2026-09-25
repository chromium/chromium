// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/virtual_card_utils.h"

#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/autofill_resource_util.h"
#include "url/android/gurl_android.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/android/chrome_jni_headers/VirtualCardEnrollmentFields_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::ScopedJavaLocalRef;

namespace autofill {

ScopedJavaLocalRef<jobject> CreateVirtualCardEnrollmentFieldsJavaObject(
    VirtualCardEnrollmentFields* virtual_card_enrollment_fields) {
  JNIEnv* env = AttachCurrentThread();
  // Create VirtualCardEnrollmentFields java object.
  int network_icon_id = ResourceMapper::MapToJavaDrawableId(
      GetIconResourceID(virtual_card_enrollment_fields->credit_card
                            .CardIconForAutofillSuggestion()));
  ScopedJavaLocalRef<jobject> java_object =
      Java_VirtualCardEnrollmentFields_create(
          env,
          virtual_card_enrollment_fields->credit_card
              .CardNameForAutofillDisplay(),
          virtual_card_enrollment_fields->credit_card
              .ObfuscatedNumberWithVisibleLastFourDigits(),
          network_icon_id,
          virtual_card_enrollment_fields->credit_card.card_art_url());
  // Add Google legal messages.
  for (const auto& legal_message_line :
       virtual_card_enrollment_fields->google_legal_message) {
    Java_VirtualCardEnrollmentFields_addGoogleLegalMessageLine(
        env, java_object, legal_message_line.text());
    for (const auto& link : legal_message_line.links()) {
      Java_VirtualCardEnrollmentFields_addLinkToLastGoogleLegalMessageLine(
          env, java_object, link.range.start(), link.range.end(),
          link.url.spec());
    }
  }
  // Add issuer legal messages.
  for (const auto& legal_message_line :
       virtual_card_enrollment_fields->issuer_legal_message) {
    Java_VirtualCardEnrollmentFields_addIssuerLegalMessageLine(
        env, java_object, legal_message_line.text());
    for (const auto& link : legal_message_line.links()) {
      Java_VirtualCardEnrollmentFields_addLinkToLastIssuerLegalMessageLine(
          env, java_object, link.range.start(), link.range.end(),
          link.url.spec());
    }
  }
  return java_object;
}

}  // namespace autofill

DEFINE_JNI(VirtualCardEnrollmentFields)
