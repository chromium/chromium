// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ACCESSIBILITY_UTILS_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ACCESSIBILITY_UTILS_H_

#include <string>

namespace autofill {

// Helper class for autofill accessibility functionality on Android.
class AutofillAccessibilityHelper {
 public:
  AutofillAccessibilityHelper() = default;
  virtual ~AutofillAccessibilityHelper() = default;
  virtual void AnnounceTextForA11y(const std::u16string& message);
  static AutofillAccessibilityHelper* GetInstance();
  static void SetInstanceForTesting(AutofillAccessibilityHelper* instance);

 private:
  static AutofillAccessibilityHelper* default_instance_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ACCESSIBILITY_UTILS_H_
