// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_LEGAL_MESSAGE_LINE_ANDROID_H_
#define COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_LEGAL_MESSAGE_LINE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "jni.h"

namespace autofill {

class LegalMessageLineAndroid {
 public:
  static base::android::ScopedJavaLocalRef<jobject> ConvertToJavaObject(
      const LegalMessageLine& legal_message_line);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_LEGAL_MESSAGE_LINE_ANDROID_H_
