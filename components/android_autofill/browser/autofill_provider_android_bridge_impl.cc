// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider_android_bridge_impl.h"

#include <string>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "components/android_autofill/browser/autofill_provider_android.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/jni_headers/AutofillProvider_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;

void JNI_AutofillProvider_Init(JNIEnv* env,
                               const JavaParamRef<jobject>& jcaller,
                               const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  AutofillProviderAndroid::CreateForWebContents(web_contents);
  AutofillProviderAndroid::FromWebContents(web_contents)
      ->AttachToJavaAutofillProvider(env, jcaller);
}

AutofillProviderAndroidBridgeImpl::AutofillProviderAndroidBridgeImpl(
    Delegate* delegate)
    : delegate_(*delegate) {}

AutofillProviderAndroidBridgeImpl::~AutofillProviderAndroidBridgeImpl() {
  JNIEnv* env = AttachCurrentThread();
  if (ScopedJavaLocalRef<jobject> obj = java_ref_.get(env); !obj.is_null()) {
    Java_AutofillProvider_setNativeAutofillProvider(env, obj, 0);
  }
}

void AutofillProviderAndroidBridgeImpl::AttachToJavaAutofillProvider(
    JNIEnv* env,
    const JavaRef<jobject>& jcaller) {
  DCHECK(java_ref_.get(env).is_null());
  java_ref_ = JavaObjectWeakGlobalRef(env, jcaller);

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_AutofillProvider_setNativeAutofillProvider(
      env, obj, reinterpret_cast<intptr_t>(this));
}

void AutofillProviderAndroidBridgeImpl::SendPrefillRequest(
    FormDataAndroid& form) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_AutofillProvider_sendPrefillRequest(env, obj, form.GetJavaPeer());
}

void AutofillProviderAndroidBridgeImpl::StartAutofillSession(
    FormDataAndroid& form,
    const FieldInfo& field,
    bool has_server_predictions) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_AutofillProvider_startAutofillSession(
      env, obj, form.GetJavaPeer(), field.index, field.bounds.x(),
      field.bounds.y(), field.bounds.width(), field.bounds.height(),
      has_server_predictions);
}

void AutofillProviderAndroidBridgeImpl::OnServerPredictionQueryDone(
    bool success) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_AutofillProvider_onServerPredictionQueryDone(env, obj, success);
}

void AutofillProviderAndroidBridgeImpl::OnFocusChanged(
    const absl::optional<FieldInfo>& field) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  if (field) {
    Java_AutofillProvider_onFocusChanged(
        env, obj, /*focusOnForm=*/true, field->index, field->bounds.x(),
        field->bounds.y(), field->bounds.width(), field->bounds.height());
  } else {
    Java_AutofillProvider_onFocusChanged(env, obj, /*focusOnForm=*/false, 0, 0,
                                         0, 0, 0);
  }
}

void AutofillProviderAndroidBridgeImpl::ShowDatalistPopup(
    base::span<const SelectOption> options,
    bool is_rtl) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  std::vector<std::u16string> values;
  std::vector<std::u16string> labels;
  values.reserve(options.size());
  labels.reserve(options.size());
  for (const SelectOption& option : options) {
    values.push_back(option.value);
    labels.push_back(option.content);
  }

  Java_AutofillProvider_showDatalistPopup(
      env, obj, ToJavaArrayOfStrings(env, values),
      ToJavaArrayOfStrings(env, labels), is_rtl);
}

void AutofillProviderAndroidBridgeImpl::HideDatalistPopup() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_AutofillProvider_hideDatalistPopup(env, obj);
}

void AutofillProviderAndroidBridgeImpl::OnTextFieldDidScroll(
    const FieldInfo& field) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_AutofillProvider_onTextFieldDidScroll(
      env, obj, field.index, field.bounds.x(), field.bounds.y(),
      field.bounds.width(), field.bounds.height());
}

void AutofillProviderAndroidBridgeImpl::OnFormFieldDidChange(
    const FieldInfo& field) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_AutofillProvider_onFormFieldDidChange(
      env, obj, field.index, field.bounds.x(), field.bounds.y(),
      field.bounds.width(), field.bounds.height());
}

void AutofillProviderAndroidBridgeImpl::OnFormFieldVisibilitiesDidChange(
    base::span<const int> indices) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_AutofillProvider_onFormFieldVisibilitiesDidChange(
      env, obj, ToJavaIntArray(env, indices));
}

void AutofillProviderAndroidBridgeImpl::OnFormSubmitted(
    mojom::SubmissionSource submission_source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_AutofillProvider_onFormSubmitted(env, obj,
                                        static_cast<int>(submission_source));
}

void AutofillProviderAndroidBridgeImpl::OnDidFillAutofillFormData() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_AutofillProvider_onDidFillAutofillFormData(env, obj);
}

void AutofillProviderAndroidBridgeImpl::Reset() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_AutofillProvider_reset(env, obj);
}

void AutofillProviderAndroidBridgeImpl::DetachFromJavaAutofillProvider(
    JNIEnv* env) {
  java_ref_.reset();
}

void AutofillProviderAndroidBridgeImpl::OnAutofillAvailable(JNIEnv* env,
                                                            jobject jcaller) {
  delegate_->OnAutofillAvailable();
}

void AutofillProviderAndroidBridgeImpl::OnAcceptDataListSuggestion(
    JNIEnv* env,
    jobject jcaller,
    jstring value) {
  delegate_->OnAcceptDatalistSuggestion(ConvertJavaStringToUTF16(env, value));
}

void AutofillProviderAndroidBridgeImpl::SetAnchorViewRect(JNIEnv* env,
                                                          jobject jcaller,
                                                          jobject anchor_view,
                                                          jfloat x,
                                                          jfloat y,
                                                          jfloat width,
                                                          jfloat height) {
  delegate_->SetAnchorViewRect(ScopedJavaLocalRef<jobject>(env, anchor_view),
                               gfx::RectF(x, y, width, height));
}

void AutofillProviderAndroidBridgeImpl::OnShowBottomSheetResult(
    JNIEnv* env,
    jboolean is_shown,
    jboolean provided_autofill_structure) {
  delegate_->OnShowBottomSheetResult(is_shown, provided_autofill_structure);
}
}  // namespace autofill
