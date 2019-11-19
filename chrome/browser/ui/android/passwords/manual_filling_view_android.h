// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_MANUAL_FILLING_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_MANUAL_FILLING_VIEW_ANDROID_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"

namespace gfx {
class Image;
}

class ManualFillingController;

// This Android-specific implementation of the |ManualFillingViewInterface|
// is the native counterpart of the |PasswordAccessoryViewBridge| java class.
// It's owned by a ManualFillingController which is bound to an activity.
class ManualFillingViewAndroid : public ManualFillingViewInterface {
 public:
  // Builds the UI for the |controller|.
  explicit ManualFillingViewAndroid(ManualFillingController* controller);
  ~ManualFillingViewAndroid() override;

  // ManualFillingViewInterface:
  void OnItemsAvailable(const autofill::AccessorySheetData& data) override;
  void OnAutomaticGenerationStatusChanged(bool available) override;
  void CloseAccessorySheet() override;
  void SwapSheetWithKeyboard() override;
  void ShowWhenKeyboardIsVisible() override;
  void Hide() override;

  // Called from Java via JNI:
  void OnFaviconRequested(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_origin,
      jint desired_size_in_px,
      const base::android::JavaParamRef<jobject>& j_callback);
  void OnFillingTriggered(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint tab_type,
      const base::android::JavaParamRef<jobject>& j_user_info_field);
  void OnOptionSelected(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint selected_action);

 private:
  void OnImageFetched(base::android::ScopedJavaGlobalRef<jstring> j_origin,
                      base::android::ScopedJavaGlobalRef<jobject> j_callback,
                      const gfx::Image& image);

  base::android::ScopedJavaLocalRef<jobject>
  ConvertAccessorySheetDataToJavaObject(
      JNIEnv* env,
      const autofill::AccessorySheetData& tab_data);

  autofill::UserInfo::Field ConvertJavaUserInfoField(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_field_to_convert);

  // The controller provides data for this view and owns it.
  ManualFillingController* controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(ManualFillingViewAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_MANUAL_FILLING_VIEW_ANDROID_H_
