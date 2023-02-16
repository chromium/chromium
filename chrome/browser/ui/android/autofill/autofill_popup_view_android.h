// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/core/common/aliases.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/android/view_android.h"

namespace autofill {

class AutofillPopupController;

class AutofillPopupViewAndroid : public AutofillPopupView {
 public:
  explicit AutofillPopupViewAndroid(
      base::WeakPtr<AutofillPopupController> controller);

  AutofillPopupViewAndroid(const AutofillPopupViewAndroid&) = delete;
  AutofillPopupViewAndroid& operator=(const AutofillPopupViewAndroid&) = delete;

  ~AutofillPopupViewAndroid() override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  // Called when an autofill item was selected.
  void SuggestionSelected(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint list_index);

  void DeletionRequested(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint list_index);

  void DeletionConfirmed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void PopupDismissed(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);

 protected:
  // AutofillPopupView:
  void Show(AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void Hide() override;
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void OnSuggestionsChanged() override;
  void AxAnnounce(const std::u16string& text) override;
  absl::optional<int32_t> GetAxUniqueId() override;

 private:
  friend class AutofillPopupView;
  // Creates the AutofillPopupBridge Java object.
  bool Init();
  // Returns whether the dropdown was suppressed (mainly due to not enough
  // screen space available).
  bool WasSuppressed();

  base::WeakPtr<AutofillPopupController> controller_;  // weak.

  // The index of the last item the user long-pressed (they will be shown a
  // confirmation dialog).
  int deleting_index_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Popup view
  ui::ViewAndroid::ScopedAnchorView popup_view_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_
