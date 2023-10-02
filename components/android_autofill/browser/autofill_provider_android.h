// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/autofill_provider_android_bridge.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace autofill {

class FormDataAndroid;

// Android implementation of AutofillProvider, it has one instance per
// WebContents, this class is native peer of AutofillProvider.java.
// This class is always instantialized by AutofillProvider Java object.
class AutofillProviderAndroid : public AutofillProvider,
                                public AutofillProviderAndroidBridge::Delegate,
                                public content::WebContentsObserver {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  static AutofillProviderAndroid* FromWebContents(
      content::WebContents* web_contents);

  AutofillProviderAndroid(const AutofillProviderAndroid&) = delete;
  AutofillProviderAndroid& operator=(const AutofillProviderAndroid&) = delete;
  ~AutofillProviderAndroid() override;

  // Attach this detached object to `jcaller`.
  void AttachToJavaAutofillProvider(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcaller);

  // AutofillProvider:
  void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource /*unused_trigger_source*/) override;
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
  void OnHidePopup(AndroidAutofillManager* manager) override;
  // TODO(crbug.com/1479006): Remove the `manager_for_debugging` parameter.
  void OnServerPredictionsAvailable(
      AndroidAutofillManager* manager_for_debugging,
      FormGlobalId form) override;
  void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                 FormSignature form_signature) override;

  void OnManagerResetOrDestroyed(AndroidAutofillManager* manager) override;

  bool GetCachedIsAutofilled(const FormFieldData& field) const override;

 private:
  friend class AutofillProviderAndroidTestApi;

  explicit AutofillProviderAndroid(content::WebContents* web_contents);

  // AndroidAutofillProviderBridge::Delegate:
  void OnAutofillAvailable() override;
  void OnAcceptDatalistSuggestion(const std::u16string& value) override;
  void SetAnchorViewRect(const base::android::JavaRef<jobject>& anchor,
                         const gfx::RectF& bounds) override;

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  void FireSuccessfulSubmission(mojom::SubmissionSource source);

  // Calls `OnFormFieldDidChange` in the bridge if there is an ongoing Autofill
  // session for this `form`.
  void MaybeFireFormFieldDidChange(AndroidAutofillManager* manager,
                                   const FormData& form,
                                   const FormFieldData& field,
                                   const gfx::RectF& bounding_box);

  // Propagates visibility changes for fields in `form` and notifies the bridge
  // in case any of the fields had a visibility change.
  void MaybeFireFormFieldVisibilitiesDidChange(AndroidAutofillManager* manager,
                                             const FormData& form);

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

  // Properties of the field of the current session (queried input or changed
  // select box).
  FieldGlobalId field_id_;
  FieldTypeGroup field_type_group_{FieldTypeGroup::kNoGroup};

  // The frame of the field for which the last OnAskForValuesToFill() happened.
  //
  // It is not necessarily the same frame as the current session's
  // `field_id_.host_frame` because the session may survive
  // OnAskForValuesToFill().
  //
  // It's not necessarily the same frame as `manager_`'s for the same reason as
  // `field_id_`, and also because `manager_` may refer to an ancestor frame of
  // the queried field.
  content::GlobalRenderFrameHostId last_queried_field_rfh_id_;

  // The origin of the field of the current session (cf. `field_id_`). This is
  // determines which fields are safe to be filled in cross-frame forms.
  url::Origin triggered_origin_;
  base::WeakPtr<AndroidAutofillManager> manager_;
  bool check_submission_ = false;
  // Valid only if check_submission_ is true.
  mojom::SubmissionSource pending_submission_source_;

  // The bridge for C++ <-> Java communication.
  std::unique_ptr<AutofillProviderAndroidBridge> bridge_;
};
}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_
