// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/provider/autofill_provider_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/provider/form_data_android.h"
#include "components/autofill/android/provider/jni_headers/AutofillProvider_jni.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_handler_proxy.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using content::WebContents;
using gfx::RectF;

namespace autofill {

using mojom::SubmissionSource;

AutofillProviderAndroid::AutofillProviderAndroid(
    const JavaRef<jobject>& jcaller,
    content::WebContents* web_contents)
    : id_(kNoQueryId), web_contents_(web_contents), check_submission_(false) {
  OnJavaAutofillProviderChanged(AttachCurrentThread(), jcaller);
}

void AutofillProviderAndroid::OnJavaAutofillProviderChanged(
    JNIEnv* env,
    const JavaRef<jobject>& jcaller) {
  // If the current Java object isn't null (e.g., because it hasn't been
  // garbage-collected yet), clear its reference to this object.
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_AutofillProvider_setNativeAutofillProvider(env, obj, 0);
  }

  java_ref_ = JavaObjectWeakGlobalRef(env, jcaller);

  // If the new Java object isn't null, set its native object to |this|.
  obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_AutofillProvider_setNativeAutofillProvider(
        env, obj, reinterpret_cast<jlong>(this));
  }
}

AutofillProviderAndroid::~AutofillProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // Remove the reference to this object on the Java side.
  Java_AutofillProvider_setNativeAutofillProvider(env, obj, 0);
}

void AutofillProviderAndroid::OnQueryFormFieldAutofill(
    AutofillHandlerProxy* handler,
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool /*unused_autoselect_first_suggestion*/) {
  // The id isn't passed to Java side because Android API guarantees the
  // response is always for current session, so we just use the current id
  // in response, see OnAutofillAvailable.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  id_ = id;

  // Focus or field value change will also trigger the query, so it should be
  // ignored if the form is same.
  if (ShouldStartNewSession(handler, form))
    StartNewSession(handler, form, field, bounding_box);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  if (!field.datalist_values.empty()) {
    ScopedJavaLocalRef<jobjectArray> jdatalist_values =
        ToJavaArrayOfStrings(env, field.datalist_values);
    ScopedJavaLocalRef<jobjectArray> jdatalist_labels =
        ToJavaArrayOfStrings(env, field.datalist_labels);
    Java_AutofillProvider_showDatalistPopup(
        env, obj, jdatalist_values, jdatalist_labels,
        field.text_direction == base::i18n::RIGHT_TO_LEFT);
  }
}

bool AutofillProviderAndroid::ShouldStartNewSession(
    AutofillHandlerProxy* handler,
    const FormData& form) {
  // Only start a new session when form or handler is changed, the change of
  // handler indicates query from other frame and a new session is needed.
  return !IsCurrentlyLinkedForm(form) || !IsCurrentlyLinkedHandler(handler);
}

void AutofillProviderAndroid::StartNewSession(AutofillHandlerProxy* handler,
                                              const FormData& form,
                                              const FormFieldData& field,
                                              const gfx::RectF& bounding_box) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  form_ = std::make_unique<FormDataAndroid>(
      form, base::BindRepeating(
                &AutofillDriver::TransformBoundingBoxToViewportCoordinates,
                base::Unretained(handler->driver())));

  size_t index;
  if (!form_->GetFieldIndex(field, &index)) {
    form_.reset();
    return;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!handler->GetCachedFormAndField(form, field, &form_structure,
                                      &autofill_field)) {
    form_structure = nullptr;
  }
  gfx::RectF transformed_bounding = ToClientAreaBound(bounding_box);

  ScopedJavaLocalRef<jobject> form_obj = form_->GetJavaPeer(form_structure);
  handler_ = handler->GetWeakPtr();
  Java_AutofillProvider_startAutofillSession(
      env, obj, form_obj, index, transformed_bounding.x(),
      transformed_bounding.y(), transformed_bounding.width(),
      transformed_bounding.height());
}

void AutofillProviderAndroid::OnAutofillAvailable(JNIEnv* env,
                                                  jobject jcaller,
                                                  jobject formData) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler_ && form_) {
    const FormData& form = form_->GetAutofillValues();
    SendFormDataToRenderer(handler_.get(), id_, form);
  }
}

void AutofillProviderAndroid::OnAcceptDataListSuggestion(JNIEnv* env,
                                                         jobject jcaller,
                                                         jstring value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (auto* handler = handler_.get()) {
    RendererShouldAcceptDataListSuggestion(
        handler, ConvertJavaStringToUTF16(env, value));
  }
}

void AutofillProviderAndroid::SetAnchorViewRect(JNIEnv* env,
                                                jobject jcaller,
                                                jobject anchor_view,
                                                jfloat x,
                                                jfloat y,
                                                jfloat width,
                                                jfloat height) {
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  if (!view_android)
    return;

  view_android->SetAnchorRect(ScopedJavaLocalRef<jobject>(env, anchor_view),
                              gfx::RectF(x, y, width, height));
}

void AutofillProviderAndroid::OnTextFieldDidChange(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const base::TimeTicks timestamp) {
  FireFormFieldDidChanged(handler, form, field, bounding_box);
}

void AutofillProviderAndroid::OnTextFieldDidScroll(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t index;
  if (!IsCurrentlyLinkedHandler(handler) || !IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &index))
    return;

  form_->OnFormFieldDidChange(index, field.value);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  gfx::RectF transformed_bounding = ToClientAreaBound(bounding_box);
  Java_AutofillProvider_onTextFieldDidScroll(
      env, obj, index, transformed_bounding.x(), transformed_bounding.y(),
      transformed_bounding.width(), transformed_bounding.height());
}

void AutofillProviderAndroid::OnSelectControlDidChange(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (ShouldStartNewSession(handler, form))
    StartNewSession(handler, form, field, bounding_box);
  FireFormFieldDidChanged(handler, form, field, bounding_box);
}

void AutofillProviderAndroid::FireSuccessfulSubmission(
    SubmissionSource source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AutofillProvider_onFormSubmitted(env, obj, (int)source);
  Reset();
}

void AutofillProviderAndroid::OnFormSubmitted(AutofillHandlerProxy* handler,
                                              const FormData& form,
                                              bool known_success,
                                              SubmissionSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsCurrentlyLinkedHandler(handler) || !IsCurrentlyLinkedForm(form))
    return;

  if (known_success || source == SubmissionSource::FORM_SUBMISSION) {
    FireSuccessfulSubmission(source);
    return;
  }

  check_submission_ = true;
  pending_submission_source_ = source;
}

void AutofillProviderAndroid::OnFocusNoLongerOnForm(
    AutofillHandlerProxy* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsCurrentlyLinkedHandler(handler))
    return;

  OnFocusChanged(false, 0, RectF());
}

void AutofillProviderAndroid::OnFocusOnFormField(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  size_t index;
  if (!IsCurrentlyLinkedHandler(handler) || !IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &index))
    return;

  // Because this will trigger a suggestion query, set request id to browser
  // initiated request.
  id_ = kNoQueryId;

  OnFocusChanged(true, index, ToClientAreaBound(bounding_box));
}

void AutofillProviderAndroid::OnFocusChanged(bool focus_on_form,
                                             size_t index,
                                             const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AutofillProvider_onFocusChanged(
      env, obj, focus_on_form, index, bounding_box.x(), bounding_box.y(),
      bounding_box.width(), bounding_box.height());
}

void AutofillProviderAndroid::FireFormFieldDidChanged(
    AutofillHandlerProxy* handler,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t index;
  if (!IsCurrentlyLinkedHandler(handler) || !IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &index))
    return;

  form_->OnFormFieldDidChange(index, field.value);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  gfx::RectF transformed_bounding = ToClientAreaBound(bounding_box);
  Java_AutofillProvider_onFormFieldDidChange(
      env, obj, index, transformed_bounding.x(), transformed_bounding.y(),
      transformed_bounding.width(), transformed_bounding.height());
}

void AutofillProviderAndroid::OnDidFillAutofillFormData(
    AutofillHandlerProxy* handler,
    const FormData& form,
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler != handler_.get() || !IsCurrentlyLinkedForm(form))
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AutofillProvider_onDidFillAutofillFormData(env, obj);
}

void AutofillProviderAndroid::OnFormsSeen(AutofillHandlerProxy* handler,
                                          const std::vector<FormData>& forms,
                                          const base::TimeTicks timestamp) {}

void AutofillProviderAndroid::OnHidePopup(AutofillHandlerProxy* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler == handler_.get()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return;

    Java_AutofillProvider_hidePopup(env, obj);
  }
}

void AutofillProviderAndroid::Reset(AutofillHandlerProxy* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler == handler_.get()) {
    // If we previously received a notification from the renderer that the form
    // was likely submitted and no event caused a reset of state in the interim,
    // we consider this navigation to be resulting from the submission.
    if (check_submission_ && form_.get())
      FireSuccessfulSubmission(pending_submission_source_);

    Reset();

    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return;

    Java_AutofillProvider_reset(env, obj);
  }
}

bool AutofillProviderAndroid::IsCurrentlyLinkedHandler(
    AutofillHandlerProxy* handler) {
  return handler == handler_.get();
}

bool AutofillProviderAndroid::IsCurrentlyLinkedForm(const FormData& form) {
  return form_ && form_->SimilarFormAs(form);
}

gfx::RectF AutofillProviderAndroid::ToClientAreaBound(
    const gfx::RectF& bounding_box) {
  gfx::Rect client_area = web_contents_->GetContainerBounds();
  return bounding_box + client_area.OffsetFromOrigin();
}

void AutofillProviderAndroid::Reset() {
  form_.reset(nullptr);
  id_ = kNoQueryId;
  check_submission_ = false;
}

}  // namespace autofill
