// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/authenticator_selection_dialog_view_android.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/autofill/internal/jni_headers/AuthenticatorSelectionDialogBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace autofill {

AuthenticatorSelectionDialogViewAndroid::
    AuthenticatorSelectionDialogViewAndroid(
        CardUnmaskAuthenticationSelectionDialogController* controller)
    : controller_(controller) {
  DCHECK(controller);
}

AuthenticatorSelectionDialogViewAndroid::
    ~AuthenticatorSelectionDialogViewAndroid() = default;

void AuthenticatorSelectionDialogViewAndroid::Dismiss(bool user_closed_dialog,
                                                      bool server_success) {
  if (java_object_) {
    // Multiple calls to `AuthenticatorSelectionDialogViewAndroid::Dismiss`
    // should result in only 1 call to
    // `Java_AuthenticatorSelectionDialogBridge_dismiss`.
    Java_AuthenticatorSelectionDialogBridge_dismiss(
        base::android::AttachCurrentThread(), java_object_);
    java_object_.Reset();
  }
  if (controller_) {
    // `OnDialogClosed` destroys this view, no member access or method calls
    // should happen afterwards.
    controller_->OnDialogClosed(user_closed_dialog, server_success);
  }
}

void AuthenticatorSelectionDialogViewAndroid::UpdateContent() {}

void AuthenticatorSelectionDialogViewAndroid::OnOptionSelected(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& authenticator_option_identifier) {
  std::string card_unmask_challenge_option_id =
      base::android::ConvertJavaStringToUTF8(env,
                                             authenticator_option_identifier);
  controller_->SetSelectedChallengeOptionId(
      CardUnmaskChallengeOption::ChallengeOptionId(
          card_unmask_challenge_option_id));
  controller_->OnOkButtonClicked();
}

void AuthenticatorSelectionDialogViewAndroid::OnDismissed(JNIEnv* env) {
  if (controller_) {
    // `OnDialogClosed` destroys this view, no member access or method calls
    // should happen afterwards.
    controller_->OnDialogClosed(/*user_closed_dialog=*/true,
                                /*server_success=*/false);
  }
}

bool AuthenticatorSelectionDialogViewAndroid::ShowDialog(
    ui::WindowAndroid* window_android) {
  // Don't show the dialog twice. This should be impossible as long as
  // `ShowDialog` is called only from
  // `CreateAndShowCardUnmaskAuthenticationSelectionDialog`.
  CHECK(!java_object_, base::NotFatalUntil::M150);
  if (java_object_) {
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(window_android);

  std::vector<CardUnmaskChallengeOption> options =
      controller_->GetChallengeOptions();
  base::android::ScopedJavaLocalRef<jobject> authOptions =
      CreateJavaAuthenticatorOptions(env, options);

  java_object_.Reset(Java_AuthenticatorSelectionDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));

  if (java_object_.is_null()) {
    return false;
  }

  Java_AuthenticatorSelectionDialogBridge_show(env, java_object_, authOptions);
  return true;
}

base::android::ScopedJavaLocalRef<jobject>
AuthenticatorSelectionDialogViewAndroid::CreateJavaAuthenticatorOptions(
    JNIEnv* env,
    const std::vector<CardUnmaskChallengeOption>& options) {
  base::android::ScopedJavaLocalRef<jobject> jlist =
      Java_AuthenticatorSelectionDialogBridge_createAuthenticatorOptionList(
          env);

  for (const auto& option : options) {
    CreateJavaAuthenticatorOptionAndAddToList(env, jlist, option);
  }

  return jlist;
}

void AuthenticatorSelectionDialogViewAndroid::
    CreateJavaAuthenticatorOptionAndAddToList(
        JNIEnv* env,
        base::android::ScopedJavaLocalRef<jobject> jlist,
        const CardUnmaskChallengeOption& option) {
  std::u16string title = controller_->GetAuthenticationModeLabel(option);
  Java_AuthenticatorSelectionDialogBridge_createAuthenticatorOptionAndAddToList(
      env, jlist, ConvertUTF16ToJavaString(env, title),
      ConvertUTF8ToJavaString(env, option.id.value()),
      ConvertUTF16ToJavaString(env, option.challenge_info),
      static_cast<int>(option.type));
}

CardUnmaskAuthenticationSelectionDialog*
CreateAndShowCardUnmaskAuthenticationSelectionDialog(
    content::WebContents* web_contents,
    CardUnmaskAuthenticationSelectionDialogController* controller) {
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return nullptr;
  }
  AuthenticatorSelectionDialogViewAndroid* dialog_view =
      new AuthenticatorSelectionDialogViewAndroid(controller);
  if (!dialog_view->ShowDialog(window_android)) {
    delete dialog_view;
    return nullptr;
  }
  return dialog_view;
}

}  // namespace autofill

DEFINE_JNI(AuthenticatorSelectionDialogBridge)
