// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/android_autofill_features.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/autofill_provider_android_bridge.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

using base::android::JavaRef;
using content::BrowserThread;
using mojom::SubmissionSource;
using FieldInfo = AutofillProviderAndroidBridge::FieldInfo;

// static
void AutofillProviderAndroid::CreateForWebContents(
    content::WebContents* web_contents) {
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new AutofillProviderAndroid(web_contents)));
  }
}

AutofillProviderAndroid* AutofillProviderAndroid::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<AutofillProviderAndroid*>(
      AutofillProvider::FromWebContents(web_contents));
}

AutofillProviderAndroid::AutofillProviderAndroid(
    content::WebContents* web_contents)
    : AutofillProvider(web_contents),
      content::WebContentsObserver(web_contents),
      bridge_(AndroidAutofillBridgeFactory::GetInstance()
                  .CreateAutofillProviderAndroidBridge(/*delegate=*/this)) {}

AutofillProviderAndroid::~AutofillProviderAndroid() = default;

void AutofillProviderAndroid::AttachToJavaAutofillProvider(
    JNIEnv* env,
    const JavaRef<jobject>& jcaller) {
  bridge_->AttachToJavaAutofillProvider(env, jcaller);
}

void AutofillProviderAndroid::RenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  // If the popup menu has been triggered from within an iframe and that frame
  // is deleted, hide the popup. This is necessary because the popup may
  // actually be shown by the AutofillExternalDelegate of an ancestor frame,
  // which is not notified about `rfh`'s destruction and therefore won't close
  // the popup.
  if (manager_ && last_queried_field_rfh_id_ == rfh->GetGlobalId()) {
    OnHidePopup(manager_.get());
    last_queried_field_rfh_id_ = {};
  }
}

void AutofillProviderAndroid::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (manager_ &&
      last_queried_field_rfh_id_ ==
          navigation_handle->GetPreviousRenderFrameHostId() &&
      !navigation_handle->IsSameDocument()) {
    OnHidePopup(manager_.get());
    last_queried_field_rfh_id_ = {};
  }
}

void AutofillProviderAndroid::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN && manager_) {
    OnHidePopup(manager_.get());
  }
}

void AutofillProviderAndroid::OnAskForValuesToFill(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutofillSuggestionTriggerSource /*unused_trigger_source*/) {
  // The id isn't passed to Java side because Android API guarantees the
  // response is always for current session, so we just use the current id
  // in response, see OnAutofillAvailable.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static_cast<ContentAutofillDriver&>(manager->driver())
      .render_frame_host()
      ->ForEachRenderFrameHost([this, &field](content::RenderFrameHost* rfh) {
        LocalFrameToken frame_token(rfh->GetFrameToken().value());
        if (frame_token == field.host_frame) {
          last_queried_field_rfh_id_ = rfh->GetGlobalId();
        }
      });

  // Focus or field value change will also trigger the query, so it should be
  // ignored if the form is same.
  if (!IsCurrentlyLinkedForm(form)) {
    StartNewSession(manager, form, field, bounding_box);
  }

  if (field.datalist_options.empty()) {
    return;
  }
  bridge_->ShowDatalistPopup(field.datalist_options,
                             field.text_direction == base::i18n::RIGHT_TO_LEFT);
}

void AutofillProviderAndroid::StartNewSession(AndroidAutofillManager* manager,
                                              const FormData& form,
                                              const FormFieldData& field,
                                              const gfx::RectF& bounding_box) {
  form_ = std::make_unique<FormDataAndroid>(form);
  FieldInfo field_info;
  if (!form_->GetFieldIndex(field, &field_info.index)) {
    Reset();
    return;
  }

  field_id_ = field.global_id();
  field_type_group_ = manager->ComputeFieldTypeGroupForField(form, field);
  triggered_origin_ = field.origin;
  check_submission_ = false;
  manager_ = manager->GetWeakPtrToLeafClass();

  // Set the field type predictions in `form_`.
  if (FormStructure* form_structure =
          manager->FindCachedFormById(form.global_id())) {
    form_->UpdateFieldTypes(*form_structure);
  }
  field_info.bounds = ToClientAreaBound(bounding_box);
  bridge_->StartAutofillSession(
      *form_, field_info, manager->has_server_prediction(form.global_id()));
}

void AutofillProviderAndroid::OnAutofillAvailable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager_ && form_) {
    form_->UpdateFromJava();
    FillOrPreviewForm(manager_.get(), form_->form(), field_type_group_,
                      triggered_origin_);
  }
}

void AutofillProviderAndroid::OnAcceptDatalistSuggestion(
    const std::u16string& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (auto* manager = manager_.get()) {
    RendererShouldAcceptDataListSuggestion(manager, field_id_, value);
  }
}

void AutofillProviderAndroid::SetAnchorViewRect(
    const base::android::JavaRef<jobject>& anchor,
    const gfx::RectF& bounds) {
  if (ui::ViewAndroid* view_android = web_contents()->GetNativeView()) {
    view_android->SetAnchorRect(anchor, bounds);
  }
}

void AutofillProviderAndroid::OnTextFieldDidChange(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const base::TimeTicks timestamp) {
  MaybeFireFormFieldDidChange(manager, form, field, bounding_box);
}

void AutofillProviderAndroid::OnTextFieldDidScroll(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FieldInfo field_info;
  if (!IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &field_info.index)) {
    return;
  }

  // TODO(crbug.com/1478934): Investigate whether the update of the value
  // is needed - why would it have changed?
  form_->OnFormFieldDidChange(field_info.index, field.value);

  field_info.bounds = ToClientAreaBound(bounding_box);
  bridge_->OnTextFieldDidScroll(field_info);
}

void AutofillProviderAndroid::OnSelectControlDidChange(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (!IsCurrentlyLinkedForm(form)) {
    StartNewSession(manager, form, field, bounding_box);
    // TODO(crbug.com/1478934): Return early at this point?
  }
  MaybeFireFormFieldDidChange(manager, form, field, bounding_box);
}

void AutofillProviderAndroid::FireSuccessfulSubmission(
    SubmissionSource source) {
  bridge_->OnFormSubmitted(source);
  Reset();
}

void AutofillProviderAndroid::OnFormSubmitted(AndroidAutofillManager* manager,
                                              const FormData& form,
                                              bool known_success,
                                              SubmissionSource source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsCurrentlyLinkedManager(manager) || !form_) {
    return;
  }

  // In the case of form submissions, we want to perform less strict form
  // comparisons than for other form events (focus change, scroll change, etc.):
  // Even if the page modifies the form between the user interaction and the
  // form submission, we want to inform `AutofillManager` about the submission.
  // Otherwise no saving prompt can be offered.
  if (base::FeatureList::IsEnabled(
          features::kAndroidAutofillFormSubmissionCheckById)) {
    if (form_->form().global_id() != form.global_id()) {
      return;
    }
  } else {
    if (!form_->SimilarFormAs(form)) {
      return;
    }
  }

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
  if (!IsCurrentlyLinkedManager(manager)) {
    return;
  }

  bridge_->OnFocusChanged(absl::nullopt);
}

void AutofillProviderAndroid::OnFocusOnFormField(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FieldInfo field_info;
  if (!IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &field_info.index)) {
    return;
  }

  field_info.bounds = ToClientAreaBound(bounding_box);
  MaybeFireFormFieldVisibilitiesDidChange(manager, form);
  bridge_->OnFocusChanged(field_info);
}

void AutofillProviderAndroid::MaybeFireFormFieldDidChange(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FieldInfo field_info;
  if (!IsCurrentlyLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &field_info.index)) {
    return;
  }
  // Propagate the changed values to Java.
  form_->OnFormFieldDidChange(field_info.index, field.value);
  field_info.bounds = ToClientAreaBound(bounding_box);
  bridge_->OnFormFieldDidChange(field_info);
}

void AutofillProviderAndroid::MaybeFireFormFieldVisibilitiesDidChange(
    AndroidAutofillManager* manager,
    const FormData& form) {
  if (!IsCurrentlyLinkedForm(form) ||
      !base::FeatureList::IsEnabled(
          features::kAndroidAutofillSupportVisibilityChanges)) {
    return;
  }

  std::vector<int> field_indices_with_change =
      form_->UpdateFieldVisibilities(form);
  if (field_indices_with_change.empty()) {
    return;
  }
  bridge_->OnFormFieldVisibilitiesDidChange(field_indices_with_change);
}

void AutofillProviderAndroid::OnDidFillAutofillFormData(
    AndroidAutofillManager* manager,
    const FormData& form,
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager != manager_.get() || !IsCurrentlyLinkedForm(form)) {
    return;
  }
  bridge_->OnDidFillAutofillFormData();
}

void AutofillProviderAndroid::OnHidePopup(AndroidAutofillManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager == manager_.get()) {
    bridge_->HideDatalistPopup();
  }
}

void AutofillProviderAndroid::OnServerPredictionsAvailable(
    AndroidAutofillManager* manager_for_debugging,
    FormGlobalId form) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!form_ || form_->form().global_id() != form) {
    return;
  }

  if (!manager_) {
    // TODO(crbug.com/1479006): This should never be reachable. Remove once it
    // is clear how it can happen.
    SCOPED_CRASH_KEY_STRING32("crbug1479006", "form_ token",
                              form_->form().global_id().frame_token.ToString());
    SCOPED_CRASH_KEY_STRING32("crbug1479006", "form token",
                              form.frame_token.ToString());
    SCOPED_CRASH_KEY_STRING32(
        "crbug1479006", "manager token",
        manager_for_debugging->driver().GetFrameToken().ToString());
    base::debug::DumpWithoutCrashing();
    return;
  }

  const FormStructure* form_structure = manager_->FindCachedFormById(form);
  if (!form_structure) {
    return;
  }
  form_->UpdateFieldTypes(*form_structure);
  bridge_->OnServerPredictionQueryDone(/*success=*/true);
}

void AutofillProviderAndroid::OnServerQueryRequestError(
    AndroidAutofillManager* manager,
    FormSignature form_signature) {
  if (!IsCurrentlyLinkedManager(manager) || !form_.get())
    return;

  if (auto* form_structure =
          manager_->FindCachedFormById(form_->form().global_id())) {
    if (form_structure->form_signature() != form_signature) {
      return;
    }

    bridge_->OnServerPredictionQueryDone(/*success=*/false);
  }
}

void AutofillProviderAndroid::OnManagerResetOrDestroyed(
    AndroidAutofillManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsCurrentlyLinkedManager(manager)) {
    return;
  }

  // If we previously received a notification from the renderer that the form
  // was likely submitted and no event caused a reset of state in the interim,
  // we consider this navigation to be resulting from the submission.
  if (check_submission_ && form_.get()) {
    FireSuccessfulSubmission(pending_submission_source_);
  }

  Reset();
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
  manager_ = nullptr;
  form_.reset();
  field_id_ = {};
  field_type_group_ = FieldTypeGroup::kNoGroup;
  triggered_origin_ = {};
  check_submission_ = false;

  // This is a no-op if there is no datalist popup.
  bridge_->HideDatalistPopup();
  // TODO(crbug.com/1488233): Also send an unfocus event to make sure that the
  // Autofill session is truly terminated.
}

}  // namespace autofill
