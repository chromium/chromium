// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ANDROID_AUTOFILL_PROVIDER_ANDROID_H_
#define COMPONENTS_AUTOFILL_ANDROID_AUTOFILL_PROVIDER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_provider.h"

namespace content {
class WebContents;
}

namespace autofill {

class FormDataAndroid;

// Android implementation of AutofillProvider, it has one instance per
// WebContents, this class is native peer of AutofillProvider.java.
class AutofillProviderAndroid : public AutofillProvider {
 public:
  AutofillProviderAndroid(const base::android::JavaRef<jobject>& jcaller,
                          content::WebContents* web_contents);
  ~AutofillProviderAndroid() override;

  // AutofillProvider:
  void OnQueryFormFieldAutofill(
      AutofillHandlerProxy* handler,
      int32_t id,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      bool /*unused_autoselect_first_suggestion*/) override;
  void OnTextFieldDidChange(AutofillHandlerProxy* handler,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box,
                            const base::TimeTicks timestamp) override;
  void OnTextFieldDidScroll(AutofillHandlerProxy* handler,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box) override;
  void OnSelectControlDidChange(AutofillHandlerProxy* handler,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override;
  void OnFormSubmitted(AutofillHandlerProxy* handler,
                       const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource source) override;
  void OnFocusNoLongerOnForm(AutofillHandlerProxy* handler) override;
  void OnFocusOnFormField(AutofillHandlerProxy* handler,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;
  void OnDidFillAutofillFormData(AutofillHandlerProxy* handler,
                                 const FormData& form,
                                 base::TimeTicks timestamp) override;
  void OnFormsSeen(AutofillHandlerProxy* handler,
                   const std::vector<FormData>& forms,
                   const base::TimeTicks timestamp) override;

  void Reset(AutofillHandlerProxy* handler) override;

  // Methods called by Java.
  void OnAutofillAvailable(JNIEnv* env, jobject jcaller, jobject form_data);

 private:
  void FireSuccessfulSubmission(mojom::SubmissionSource source);
  void OnFocusChanged(bool focus_on_form,
                      size_t index,
                      const gfx::RectF& bounding_box);
  void FireFormFieldDidChanged(AutofillHandlerProxy* handler,
                               const FormData& form,
                               const FormFieldData& field,
                               const gfx::RectF& bounding_box);

  bool IsCurrentlyLinkedHandler(AutofillHandlerProxy* handler);

  bool IsCurrentlyLinkedForm(const FormData& form);

  gfx::RectF ToClientAreaBound(const gfx::RectF& bounding_box);

  bool ShouldStartNewSession(AutofillHandlerProxy* handler,
                             const FormData& form);

  void StartNewSession(AutofillHandlerProxy* handler,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& bounding_box);

  void Reset();

  int32_t id_;
  std::unique_ptr<FormDataAndroid> form_;
  base::WeakPtr<AutofillHandlerProxy> handler_;
  JavaObjectWeakGlobalRef java_ref_;
  content::WebContents* web_contents_;
  bool check_submission_;
  // Valid only if check_submission_ is true.
  mojom::SubmissionSource pending_submission_source_;

  base::WeakPtr<AutofillHandlerProxy> handler_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProviderAndroid);
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_ANDROID_AUTOFILL_PROVIDER_ANDROID_H_
