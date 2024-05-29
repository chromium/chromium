// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/virtual_card_utils.h"

#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/VirtualCardEnrollmentFields_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace autofill {

ScopedJavaLocalRef<jobject> CreateVirtualCardEnrollmentFieldsJavaObject(
    autofill::VirtualCardEnrollmentFields* virtual_card_enrollment_fields) {
  JNIEnv* env = AttachCurrentThread();
  // Create VirtualCardEnrollmentFields java object.
  ScopedJavaLocalRef<jstring> card_name = ConvertUTF16ToJavaString(
      env,
      virtual_card_enrollment_fields->credit_card.CardNameForAutofillDisplay());
  ScopedJavaLocalRef<jstring> card_number = ConvertUTF16ToJavaString(
      env, virtual_card_enrollment_fields->credit_card
               .ObfuscatedNumberWithVisibleLastFourDigits());
  int network_icon_id = ResourceMapper::MapToJavaDrawableId(
      GetIconResourceID(virtual_card_enrollment_fields->credit_card
                            .CardIconForAutofillSuggestion()));
  ScopedJavaLocalRef<jobject> card_art_url = url::GURLAndroid::FromNativeGURL(
      env, virtual_card_enrollment_fields->credit_card.card_art_url());
  ScopedJavaLocalRef<jobject> java_object =
      Java_VirtualCardEnrollmentFields_create(env, card_name, card_number,
                                              network_icon_id, card_art_url);
  // Add Google legal messages.
  for (const auto& legal_message_line :
       virtual_card_enrollment_fields->google_legal_message) {
    Java_VirtualCardEnrollmentFields_addGoogleLegalMessageLine(
        env, java_object,
        ConvertUTF16ToJavaString(env, legal_message_line.text()));
    for (const auto& link : legal_message_line.links()) {
      Java_VirtualCardEnrollmentFields_addLinkToLastGoogleLegalMessageLine(
          env, java_object, link.range.start(), link.range.end(),
          ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }
  // Add issuer legal messages.
  for (const auto& legal_message_line :
       virtual_card_enrollment_fields->issuer_legal_message) {
    Java_VirtualCardEnrollmentFields_addIssuerLegalMessageLine(
        env, java_object,
        ConvertUTF16ToJavaString(env, legal_message_line.text()));
    for (const auto& link : legal_message_line.links()) {
      Java_VirtualCardEnrollmentFields_addLinkToLastIssuerLegalMessageLine(
          env, java_object, link.range.start(), link.range.end(),
          ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }
  return java_object;
}

}  // namespace autofill
