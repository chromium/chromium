// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_LOGGER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_LOGGER_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"

namespace autofill {

// This class gets notified when autofill fields are filled, and can be used
// by the embedder to inject its logging mechanisms.
class AutofillLoggerAndroid {
 public:
  // Called when a field containing |autofilled_value| has been filled
  // with data from |profile_full_name|.
  static void DidFillOrPreviewField(const std::u16string& autofilled_value,
                                    const std::u16string& profile_full_name);

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillLoggerAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_LOGGER_ANDROID_H_
