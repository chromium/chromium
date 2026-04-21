// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}

namespace autofill {

class AtMemoryBottomSheetDelegate;

// Bridge class owned by `ChromeAutofillClient` providing an entry point
// to trigger the @memory bottom sheet on Android.
class AtMemoryBottomSheetBridge {
 public:
  explicit AtMemoryBottomSheetBridge(content::WebContents* web_contents);

  AtMemoryBottomSheetBridge(const AtMemoryBottomSheetBridge&) = delete;
  AtMemoryBottomSheetBridge& operator=(const AtMemoryBottomSheetBridge&) =
      delete;

  virtual ~AtMemoryBottomSheetBridge();

  // Requests to show the bottom sheet.
  void RequestShowContent(
      std::unique_ptr<AtMemoryBottomSheetDelegate> delegate);

  // -- JNI calls bridged to AtMemoryBottomSheetDelegate --
  void OnDismissed(JNIEnv* env);

 private:
  void ResetDelegate();

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AtMemoryBottomSheetDelegate> delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_BRIDGE_H_
