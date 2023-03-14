// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/jni_headers/AutofillProvider_jni.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using content::WebContents;
using gfx::RectF;

namespace autofill {

using mojom::SubmissionSource;

static jlong JNI_AutofillProvider_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  auto* provider = AutofillProvider::FromWebContents(web_contents);
  if (provider) {
    static_cast<AutofillProviderAndroid*>(provider)
        ->AttachToJavaAutofillProvider(env, jcaller);
    return reinterpret_cast<intptr_t>(provider);
  }
  return reinterpret_cast<intptr_t>(
      AutofillProviderAndroid::Create(env, jcaller, web_contents));
}

// Static
AutofillProviderAndroid* AutofillProviderAndroid::Create(
    JNIEnv* env,
    const JavaRef<jobject>& jcaller,
    content::WebContents* web_contents) {
  DCHECK(!FromWebContents(web_contents));
  // This object is owned by WebContents.
  return new AutofillProviderAndroid(env, jcaller, web_contents);
}

AutofillProviderAndroid* AutofillProviderAndroid::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<AutofillProviderAndroid*>(
      AutofillProvider::FromWebContents(web_contents));
}

AutofillProviderAndroid::AutofillProviderAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& jcaller,
    content::WebContents* web_contents)
    : AutofillProvider(web_contents),
      java_ref_(JavaObjectWeakGlobalRef(env, jcaller)),
      check_submission_(false) {}

AutofillProviderAndroid::~AutofillProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // Remove the reference to this object on the Java side.
  Java_AutofillProvider_setNativeAutofillProvider(env, obj, 0);
}

void AutofillProviderAndroid::AttachToJavaAutofillProvider(
    JNIEnv* env,
    const JavaRef<jobject>& jcaller) {
  DCHECK(java_ref_.get(env).is_null());
  java_ref_ = JavaObjectWeakGlobalRef(env, jcaller);
}

void AutofillProviderAndroid::DetachFromJavaAutofillProvider(JNIEnv* env) {
  // Reset the reference to Java peer.
  java_ref_.reset();
}

void AutofillProviderAndroid::OnAskForValuesToFill(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutoselectFirstSuggestion /*unused_autoselect_first_suggestion*/,
    FormElementWasClicked /*unused_form_element_was_clicked*/) {
  // The id isn't passed to Java side because Android API guarantees the
  // response is always for current session, so we just use the current id
  // in response, see OnAutofillAvailable.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Focus or field value change will also trigger the query, so it should be
  // ignored if the form is same.
  if (ShouldStartNewSession(manager, form))
    StartNewSession(manager, form, field, bounding_box);

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
    AndroidAutofillManager* manager,
    const FormData& form) {
  // Only start a new session when form or manager is changed, the change of
  // manager indicates query from other frame and a new session is needed.
  return !IsCurrentlyLinkedForm(form) || !IsCurrentlyLinkedManager(manager);
}

void AutofillProviderAndroid::StartNewSession(AndroidAutofillManager* manager,
                                              const FormData& form,
                                              const FormFieldData& field,
                                              const gfx::RectF& bounding_box) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  form_ = std::make_unique<FormDataAndroid>(form);
  field_id_ = field.global_id();
  field_type_group_ = manager->ComputeFieldTypeGroupForField(form, field);
  triggered_origin_ = field.origin;

  size_t index;
  if (!form_->GetFieldIndex(field, &index)) {
    form_.reset();
    field_id_ = {};
    field_type_group_ = FieldTypeGroup::kNoGroup;
    triggered_origin_ = {};
    return;
  }

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!manager->GetCachedFormAndField(form, field, &form_structure,
                                      &autofill_field)) {
    form_structure = nullptr;
  }
  gfx::RectF transformed_bounding = ToClientAreaBound(bounding_box);

  ScopedJavaLocalRef<jobject> form_obj = form_->GetJavaPeer(form_structure);
  manager_ = manager->GetWeakPtrToLeafClass();
  Java_AutofillProvider_startAutofillSession(
      env, obj, form_obj, index, transformed_bounding.x(),
      transformed_bounding.y(), transformed_bounding.width(),
      transformed_bounding.height(), manager->has_server_prediction());
}

void AutofillProviderAndroid::OnAutofillAvailable(JNIEnv* env,
                                                  jobject jcaller,
                                                  jobject formData) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager_ && form_) {
    form_->UpdateFromJava();
    const FormData& form = form_->form();

    FillOrPreviewForm(manager_.get(), form, field_type_group_,
                      triggered_origin_);
  }
}

void AutofillProviderAndroid::OnAcceptDataListSuggestion(JNIEnv* env,
                                                         jobject jcaller,
                                                         jstring value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (auto* manager = manager_.get()) {
    RendererShouldAcceptDataListSuggestion(
        manager, field_id_, ConvertJavaStringToUTF16(env, value));
  }
}

void AutofillProviderAndroid::SetAnchorViewRect(JNIEnv* env,
                                                jobject jcaller,
                                                jobject anchor_view,
                                                jfloat x,
                                                jfloat y,
                                                jfloat width,
                                                jfloat height) {
  ui::ViewAndroid* view_android = web_contents()->GetNativeView();
  if (!view_android)
    return;

  view_android->SetAnchorRect(ScopedJavaLocalRef<jobject>(env, anchor_view),
                              gfx::RectF(x, y, width, height));
}

void AutofillProviderAndroid::OnTextFieldDidChange(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const base::TimeTicks timestamp) {
  FireFormFieldDidChanged(manager, form, field, bounding_box);
}

void AutofillProviderAndroid::OnTextFieldDidScroll(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t index;
  if (!IsCurrentlyLinkedManager(manager) || !IsCurrentlyLinkedForm(form) ||
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
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (ShouldStartNewSession(manager, form))
    StartNewSession(manager, form, field, bounding_box);
  FireFormFieldDidChanged(manager, form, field, bounding_box);
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

void AutofillProviderAndroid::OnFormSubmitted(AndroidAutofillManager* manager,
                                              const FormData& form,
                                              bool known_success,
                                              SubmissionSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsCurrentlyLinkedManager(manager) || !IsCurrentlyLinkedForm(form))
    return;

  if (known_success || source == SubmissionSource::FORM_SUBMISSION) {
    FireSuccessfulSubmission(source);
    return;
  }

  check_submission_ = true;
  pending_submission_source_ = source;
}

void AutofillProviderAndroid::OnFocusNoLongerOnForm(
    AndroidAutofillManager* manager,
    bool had_interacted_form) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsCurrentlyLinkedManager(manager))
    return;

  OnFocusChanged(false, 0, RectF());
}

void AutofillProviderAndroid::OnFocusOnFormField(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  size_t index;
  if (!IsCurrentlyLinkedManager(manager) || !IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &index))
    return;

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
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t index;
  if (!IsCurrentlyLinkedManager(manager) || !IsCurrentlyLinkedForm(form) ||
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
    AndroidAutofillManager* manager,
    const FormData& form,
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager != manager_.get() || !IsCurrentlyLinkedForm(form))
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AutofillProvider_onDidFillAutofillFormData(env, obj);
}

void AutofillProviderAndroid::OnHidePopup(AndroidAutofillManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager == manager_.get()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return;

    Java_AutofillProvider_hidePopup(env, obj);
  }
}

void AutofillProviderAndroid::OnServerPredictionsAvailable(
    AndroidAutofillManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager != manager_.get() || !form_.get())
    return;

  if (auto* form_structure =
          manager_->FindCachedFormById(form_->form().global_id())) {
    form_->UpdateFieldTypes(*form_structure);

    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return;

    Java_AutofillProvider_onQueryDone(env, obj, /*success=*/true);
  }
}

void AutofillProviderAndroid::OnServerQueryRequestError(
    AndroidAutofillManager* manager,
    FormSignature form_signature) {
  if (!IsCurrentlyLinkedManager(manager) || !form_.get())
    return;

  if (auto* form_structure =
          manager_->FindCachedFormById(form_->form().global_id())) {
    if (form_structure->form_signature() != form_signature)
      return;

    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return;

    Java_AutofillProvider_onQueryDone(env, obj, /*success=*/false);
  }
}

void AutofillProviderAndroid::Reset(AndroidAutofillManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager == manager_.get()) {
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

bool AutofillProviderAndroid::GetCachedIsAutofilled(
    const FormFieldData& field) const {
  size_t field_index = 0u;
  return form_ && form_->GetFieldIndex(field, &field_index) &&
         form_->form().fields[field_index].is_autofilled;
}

bool AutofillProviderAndroid::IsCurrentlyLinkedManager(
    AndroidAutofillManager* manager) {
  return manager == manager_.get();
}

bool AutofillProviderAndroid::IsCurrentlyLinkedForm(const FormData& form) {
  return form_ && form_->SimilarFormAs(form);
}

gfx::RectF AutofillProviderAndroid::ToClientAreaBound(
    const gfx::RectF& bounding_box) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  return bounding_box + client_area.OffsetFromOrigin();
}

void AutofillProviderAndroid::Reset() {
  form_.reset(nullptr);
  field_id_ = {};
  field_type_group_ = FieldTypeGroup::kNoGroup;
  triggered_origin_ = {};
  check_submission_ = false;
}

}  // namespace autofill
