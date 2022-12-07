// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/autofill/core/common/unique_ids.h"

namespace content {
class WebContents;
}

namespace autofill {

class FormDataAndroid;

// Android implementation of AutofillProvider, it has one instance per
// WebContents, this class is native peer of AutofillProvider.java.
// This class is always instantialized by AutofillProvider Java object.
class AutofillProviderAndroid : public AutofillProvider {
 public:
  static AutofillProviderAndroid* Create(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcaller,
      content::WebContents* web_contents);

  static AutofillProviderAndroid* FromWebContents(
      content::WebContents* web_contents);

  ~AutofillProviderAndroid() override;

  AutofillProviderAndroid(const AutofillProviderAndroid&) = delete;
  AutofillProviderAndroid& operator=(const AutofillProviderAndroid&) = delete;

  // Attach this detached object to |jcaller|.
  void AttachToJavaAutofillProvider(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcaller);

  // Invoked when the WebContents that associates with Java AutofillProvider
  // is changed or Java AutofillProvider is destroyed, it indicates this
  // AutofillProviderAndroid object shall not talk to its Java peer anymore.
  void DetachFromJavaAutofillProvider(JNIEnv* env);

  // AutofillProvider:
  void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutoselectFirstSuggestion /*unused_autoselect_first_suggestion*/,
      FormElementWasClicked /*unused_form_element_was_clicked*/) override;
  void OnTextFieldDidChange(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box,
                            const base::TimeTicks timestamp) override;
  void OnTextFieldDidScroll(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box) override;
  void OnSelectControlDidChange(AndroidAutofillManager* manager,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override;
  void OnFormSubmitted(AndroidAutofillManager* manager,
                       const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource source) override;
  void OnFocusNoLongerOnForm(AndroidAutofillManager* manager,
                             bool had_interacted_form) override;
  void OnFocusOnFormField(AndroidAutofillManager* manager,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;
  void OnDidFillAutofillFormData(AndroidAutofillManager* manager,
                                 const FormData& form,
                                 base::TimeTicks timestamp) override;
  void OnFormsSeen(AndroidAutofillManager* manager,
                   const std::vector<FormData>& forms) override;
  void OnHidePopup(AndroidAutofillManager* manager) override;
  void OnServerPredictionsAvailable(AndroidAutofillManager* manager) override;
  void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                 FormSignature form_signature) override;

  void Reset(AndroidAutofillManager* manager) override;

  // Methods called by Java.
  void OnAutofillAvailable(JNIEnv* env, jobject jcaller, jobject form_data);
  void OnAcceptDataListSuggestion(JNIEnv* env, jobject jcaller, jstring value);

  void SetAnchorViewRect(JNIEnv* env,
                         jobject jcaller,
                         jobject anchor_view,
                         jfloat x,
                         jfloat y,
                         jfloat width,
                         jfloat height);

 private:
  AutofillProviderAndroid(JNIEnv* env,
                          const base::android::JavaRef<jobject>& jcaller,
                          content::WebContents* web_contents);

  void FireSuccessfulSubmission(mojom::SubmissionSource source);
  void OnFocusChanged(bool focus_on_form,
                      size_t index,
                      const gfx::RectF& bounding_box);
  void FireFormFieldDidChanged(AndroidAutofillManager* manager,
                               const FormData& form,
                               const FormFieldData& field,
                               const gfx::RectF& bounding_box);

  bool IsCurrentlyLinkedManager(AndroidAutofillManager* manager);

  bool IsCurrentlyLinkedForm(const FormData& form);

  gfx::RectF ToClientAreaBound(const gfx::RectF& bounding_box);

  bool ShouldStartNewSession(AndroidAutofillManager* manager,
                             const FormData& form);

  void StartNewSession(AndroidAutofillManager* manager,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& bounding_box);

  void Reset();

  // The form of the current session (queried input or changed select box).
  std::unique_ptr<FormDataAndroid> form_;
  // The field of the current session (queried input or changed select box).
  FieldGlobalId field_id_;
  // The origin of the field of the current session (cf. `field_id_`). This is
  // determines which fields are safe to be filled in cross-frame forms.
  url::Origin triggered_origin_;
  base::WeakPtr<AndroidAutofillManager> manager_;
  JavaObjectWeakGlobalRef java_ref_;
  bool check_submission_;
  // Valid only if check_submission_ is true.
  mojom::SubmissionSource pending_submission_source_;
};
}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_
