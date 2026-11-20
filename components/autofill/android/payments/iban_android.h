// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_IBAN_ANDROID_H_
#define COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_IBAN_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/payments_jni_headers/Iban_jni.h"

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::Iban>(
    JNIEnv* env,
    const autofill::Iban& iban) {
  switch (iban.record_type()) {
    case autofill::Iban::kLocalIban:
      return autofill::Java_Iban_createLocal(
          env, iban.guid(), iban.GetIdentifierStringForAutofillDisplay(),
          iban.nickname(), iban.value());
    case autofill::Iban::kServerIban:
      return autofill::Java_Iban_createServer(
          env, iban.instrument_id(),
          iban.GetIdentifierStringForAutofillDisplay(), iban.nickname(),
          iban.value());
    case autofill::Iban::kUnknown:
      return autofill::Java_Iban_createEphemeral(
          env, iban.GetIdentifierStringForAutofillDisplay(), iban.nickname(),
          iban.value());
  }
}

}  // namespace jni_zero

#endif  // COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_IBAN_ANDROID_H_
