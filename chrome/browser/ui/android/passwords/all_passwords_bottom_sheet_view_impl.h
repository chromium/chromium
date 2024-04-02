// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_IMPL_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"

class AllPasswordsBottomSheetController;

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// This class communicates via JNI with its AllPasswordsBottomSheetBridge
// Java counterpart.
class AllPasswordsBottomSheetViewImpl : public AllPasswordsBottomSheetView {
 public:
  explicit AllPasswordsBottomSheetViewImpl(
      AllPasswordsBottomSheetController* controller);
  AllPasswordsBottomSheetViewImpl(const AllPasswordsBottomSheetViewImpl&) =
      delete;
  AllPasswordsBottomSheetViewImpl& operator=(
      const AllPasswordsBottomSheetViewImpl&) = delete;
  ~AllPasswordsBottomSheetViewImpl() override;

  // AllPasswordsBottomSheetView:
  void Show(const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
                credentials,
            autofill::mojom::FocusedFieldType focused_field_type) override;

  // Invoked in case the user chooses an entry from the credential list
  // presented to them.
  void OnCredentialSelected(JNIEnv* env,
                            std::u16string& username,
                            std::u16string& password,
                            jboolean requests_to_fill_password);

  // Called from Java bridge when user dismisses the BottomSheet.
  // Redirects the call to the controller.
  void OnDismiss(JNIEnv* env);

 private:
  // Returns either the fully initialized java counterpart of this bridge or
  // a is_null() reference if the creation failed. By using this method, the
  // bridge will try to recreate the java object if it failed previously (e.g.
  // because there was no native window available).
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject();

  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;
  raw_ptr<AllPasswordsBottomSheetController> controller_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_IMPL_H_
