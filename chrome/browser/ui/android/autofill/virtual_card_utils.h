// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_VIRTUAL_CARD_UTILS_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_VIRTUAL_CARD_UTILS_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

// Converts VirtualCardEnrollmentFields native object to it's Java counterpart.
base::android::ScopedJavaLocalRef<jobject>
CreateVirtualCardEnrollmentFieldsJavaObject(
    autofill::VirtualCardEnrollmentFields* virtual_card_enrollment_fields);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_VIRTUAL_CARD_UTILS_H_
