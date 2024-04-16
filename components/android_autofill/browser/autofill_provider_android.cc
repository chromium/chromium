// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider_android.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/android_autofill_features.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/autofill_provider_android_bridge.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace {

using ::autofill::mojom::SubmissionSource;
using ::base::android::JavaRef;
using ::content::BrowserThread;
using ::password_manager::PasswordForm;
using FieldInfo = ::autofill::AutofillProviderAndroidBridge::FieldInfo;

constexpr int kMinimumSdkVersionForPrefillRequests =
    base::android::SdkVersion::SDK_VERSION_U;

constexpr base::TimeDelta kKeyboardSuppressionTimeout = base::Seconds(1);

std::unique_ptr<PasswordForm> ParseToPasswordForm(
    const FormStructure& form_structure) {
  // Transform the predictions data to a format the `FormDataParser` can handle
  // and parse the form.
  FormData form_data = form_structure.ToFormData();
  auto autofill_predictions =
      base::MakeFlatMap<FieldGlobalId, AutofillType::ServerPrediction>(
          form_structure, /*comp=*/{},
          /*proj=*/[](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(),
                                  AutofillType::ServerPrediction(*field));
          });
  password_manager::FormDataParser parser;
  // The driver id is irrelevant here because it would only be used by password
  // manager logic that handles the `PasswordForm` returned by the parser.
  // Therefore we pass a dummy a value.
  parser.set_predictions(password_manager::ConvertToFormPredictions(
      /*driver_id=*/0, form_data, autofill_predictions));
  // On Chrome, the parser can use stored usernames to identify a filled
  // username field by the value it contains. Since we do not have access to
  // credentials, we leave it empty.
  return parser.Parse(form_data,
                      password_manager::FormDataParser::Mode::kFilling,
                      /*stored_usernames=*/{});
}

// Returns whether we should attempt to cache provider responses for this form.
// Currently, that is the case iff we diagnose it to be a login form or a change
// password form.
bool ShouldCachePasswordForm(const PasswordForm& pw_form) {
  return pw_form.IsLikelyLoginForm() ||
         (pw_form.IsLikelyChangePasswordForm() &&
          base::FeatureList::IsEnabled(
              features::kAndroidAutofillPrefillRequestsForChangePassword));
}

// Extracts the underlying value of `check_result` and eliminates all other bits
// if the bit for `FormDataAndroid::SimilarityCheckComponent::kGlobalId` is set.
// Otherwise, it returns the value as is.
// The motivation behind this is to reduce the number of metrics entries: If the
// global ids differ, it is not interesting which other components differ as
// well.
auto ProjectSimilarityCheckResultToMetricsValue(
    FormDataAndroid::SimilarityCheckResult check_result) {
  static constexpr auto kGlobalId =
      base::to_underlying(FormDataAndroid::SimilarityCheckComponent::kGlobalId);
  return check_result.value() & kGlobalId ? kGlobalId : check_result.value();
}

constexpr base::TimeDelta kWasBottomSheetShownFlipTimeout =
    base::Milliseconds(50);

}  // namespace

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
  if (!IsLinkedForm(
          form, /*similarity_metric=*/kSimilarityCheckAskForValuesToFillUma)) {
    StartNewSession(manager, form, field, bounding_box);
  } else {
    last_focused_field_id_ = field.global_id();
  }

  if (field.datalist_options.empty()) {
    return;
  }
  bridge_->ShowDatalistPopup(field.datalist_options,
                             field.text_direction == base::i18n::RIGHT_TO_LEFT);
}

bool AutofillProviderAndroid::IsFormSimilarToCachedForm(
    const FormData& form,
    const FormStructure* form_structure) const {
  if (!cached_data_ || !cached_data_->cached_form) {
    return false;
  }
  if (form_structure &&
      base::FeatureList::IsEnabled(
          features::kAndroidAutofillUsePwmPredictionsForOverrides)) {
    CHECK_EQ(form.global_id(), form_structure->global_id());
    std::unique_ptr<PasswordForm> pw_form =
        ParseToPasswordForm(*form_structure);
    if (!pw_form || !ShouldCachePasswordForm(*pw_form)) {
      return false;
    }
    PasswordParserOverrides overrides =
        PasswordParserOverrides::FromLoginForm(*pw_form, *form_structure)
            .value_or(PasswordParserOverrides());
    return cached_data_->password_parser_overrides == overrides;
  }
  return cached_data_->cached_form->SimilarFormAs(form);
}

void AutofillProviderAndroid::StartNewSession(AndroidAutofillManager* manager,
                                              const FormData& form,
                                              const FormFieldData& field,
                                              const gfx::RectF& bounding_box) {
  FormStructure* form_structure = manager->FindCachedFormById(form.global_id());
  FormDataAndroid* cached_form =
      cached_data_ ? cached_data_->cached_form.get() : nullptr;
  const bool is_similar_to_cached_form =
      IsFormSimilarToCachedForm(form, form_structure);

  // The form is assigned the same session id form sent to the Android framework
  // in the prefill request iff all of the following conditions are met:
  // - There is a cached form.
  // - This is the first time we try to show the bottom sheet for the cached
  //   form (on their second interaction, the user should see the keyboard).
  // - The cached form is similar to the current form.
  const bool use_cached_form =
      is_similar_to_cached_form && !has_used_cached_form_;
  form_ = std::make_unique<FormDataAndroid>(
      form, use_cached_form ? cached_form->session_id() : CreateSessionId());
  FieldInfo field_info;
  if (!form_->GetFieldIndex(field, &field_info.index)) {
    Reset();
    return;
  }

  last_focused_field_id_ = field.global_id();
  field_type_group_ = manager->ComputeFieldTypeGroupForField(form, field);
  triggered_origin_ = field.origin;
  check_submission_ = false;
  manager_ = manager->GetWeakPtrToLeafClass();

  // Set the field type predictions in `form_`.
  if (form_structure) {
    form_->UpdateFieldTypes(*form_structure);
    // If there a non-trivial overrides from `FormDataParse` in the cached form,
    // apply them to the new form as well.
    if (use_cached_form &&
        cached_data_->password_parser_overrides != PasswordParserOverrides()) {
      form_->UpdateFieldTypes(
          cached_data_->password_parser_overrides.ToFieldTypeMap());
    }
  }
  field_info.bounds = ToClientAreaBound(bounding_box);

  [&] {
    // Metrics for prefill requests are only emitted if this is the first time
    // a cached form is focused - hence the use of `is_similar_to_cached_form`.
    if (!ArePrefillRequestsSupported() || is_similar_to_cached_form) {
      return;
    }

    // We sent a cache request for this form element, but the form (or its
    // members) have changed since then.
    if (cached_form && cached_form->form().global_id() == form.global_id()) {
      base::UmaHistogramEnumeration(
          kPrefillRequestStateUma,
          PrefillRequestState::kRequestSentFormChanged);
      if (!base::FeatureList::IsEnabled(
              features::kAndroidAutofillUsePwmPredictionsForOverrides)) {
        base::UmaHistogramExactLinear(
            kSimilarityCheckCacheRequestUma,
            ProjectSimilarityCheckResultToMetricsValue(
                cached_form->SimilarFormAsWithDiagnosis(form)),
            FormDataAndroid::kSimilaryCheckResultExclusiveMaximum);
      }
      return;
    }

    // Prefill request state metrics are for forms that we would have cached.
    if (!form_structure) {
      return;
    }
    std::unique_ptr<PasswordForm> pw_form =
        ParseToPasswordForm(*form_structure);
    if (!pw_form || !ShouldCachePasswordForm(*pw_form)) {
      return;
    }

    if (cached_form) {
      // We would have cached the form, but another cache request had already
      // been sent.
      base::UmaHistogramEnumeration(
          kPrefillRequestStateUma,
          PrefillRequestState::kRequestNotSentMaxNumberReached);
      return;
    }

    // If we reach this point, we know that a) we would have cached the form and
    // b) no other cache request has been sent. That means that we did not
    // receive the predictions for this form in time.
    base::UmaHistogramEnumeration(kPrefillRequestStateUma,
                                  PrefillRequestState::kRequestNotSentNoTime);
  }();

  has_used_cached_form_ = true;
  bridge_->StartAutofillSession(
      *form_, field_info, manager->has_server_prediction(form.global_id()));
}

void AutofillProviderAndroid::OnAutofillAvailable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  was_bottom_sheet_just_shown_ = false;
  if (manager_ && form_) {
    form_->UpdateFromJava();
    FillOrPreviewForm(manager_.get(), form_->form(), field_type_group_,
                      triggered_origin_);
  }
}

void AutofillProviderAndroid::OnAcceptDatalistSuggestion(
    const std::u16string& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager_) {
    RendererShouldAcceptDataListSuggestion(manager_.get(),
                                           last_focused_field_id_, value);
  }
}

void AutofillProviderAndroid::SetAnchorViewRect(
    const base::android::JavaRef<jobject>& anchor,
    const gfx::RectF& bounds) {
  if (ui::ViewAndroid* view_android = web_contents()->GetNativeView()) {
    view_android->SetAnchorRect(anchor, bounds);
  }
}

void AutofillProviderAndroid::OnShowBottomSheetResult(
    bool is_shown,
    bool provided_autofill_structure) {
  was_bottom_sheet_just_shown_ = is_shown;

  if (is_shown) {
    base::UmaHistogramEnumeration(
        kPrefillRequestStateUma,
        PrefillRequestState::kRequestSentStructureProvidedBottomSheetShown);
    return;
  }

  if (keyboard_suppressor_) {
    keyboard_suppressor_->Unsuppress();
  }

  // Note that in some cases this metric is not accurate: If, for example,
  // the bottom sheet is not shown because keyboard suppression did not work, it
  // might be that a later interaction triggers the bottom sheet. See
  // b/310634445.
  base::UmaHistogramEnumeration(
      kPrefillRequestStateUma,
      provided_autofill_structure
          ? PrefillRequestState::
                kRequestSentStructureProvidedBottomSheetNotShown
          : PrefillRequestState::kRequestSentStructureNotProvided);
  if (!provided_autofill_structure && cached_data_.has_value()) {
    base::UmaHistogramTimes(
        kPrefillRequestBottomsheetNoViewStructureDelayUma,
        base::TimeTicks::Now() - cached_data_->prefill_request_creation_time);
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
  if (!IsLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &field_info.index)) {
    return;
  }

  // TODO(crbug.com/1478934): Investigate whether the update of the value
  // is needed - why would it have changed?
  form_->OnFormFieldDidChange(field_info.index, field.value());

  field_info.bounds = ToClientAreaBound(bounding_box);
  bridge_->OnTextFieldDidScroll(field_info);
}

void AutofillProviderAndroid::OnSelectControlDidChange(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (!IsLinkedForm(form)) {
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
  if (!IsLinkedManager(manager)) {
    return;
  }

  // In the case of form submissions, we want to perform less strict form
  // comparisons than for other form events (focus change, scroll change, etc.):
  // Even if the page modifies the form between the user interaction and the
  // form submission, we want to inform `AutofillManager` about the submission.
  // Otherwise no saving prompt can be offered.
  if (!IsIdOfLinkedForm(form.global_id())) {
    return;
  }

  if (known_success || source == SubmissionSource::FORM_SUBMISSION ||
      base::FeatureList::IsEnabled(
          features::kAndroidAutofillDirectFormSubmission)) {
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
  if (!IsLinkedManager(manager)) {
    return;
  }

  bridge_->OnFocusChanged(std::nullopt);
}

void AutofillProviderAndroid::OnFocusOnFormField(
    AndroidAutofillManager* manager,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FieldInfo field_info;
  if (!IsLinkedForm(form,
                    /*similarity_metri=*/kSimilarityCheckFocusOnFormFieldUma) ||
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
  if (!IsLinkedForm(form) ||
      !form_->GetSimilarFieldIndex(field, &field_info.index)) {
    return;
  }
  // Propagate the changed values to Java.
  form_->OnFormFieldDidChange(field_info.index, field.value());
  field_info.bounds = ToClientAreaBound(bounding_box);
  bridge_->OnFormFieldDidChange(field_info);
}

void AutofillProviderAndroid::MaybeFireFormFieldVisibilitiesDidChange(
    AndroidAutofillManager* manager,
    const FormData& form) {
  if (!IsLinkedForm(form) ||
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
  if (manager != manager_.get() || !IsIdOfLinkedForm(form.global_id())) {
    return;
  }
  // TODO(crbug.com/40760916): Investigate passing the actually filled fields,
  // in case the passed fields to be filled are different from the fields that
  // were actually filled.
  bridge_->OnDidFillAutofillFormData();
}

void AutofillProviderAndroid::OnHidePopup(AndroidAutofillManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (manager == manager_.get()) {
    bridge_->HideDatalistPopup();
  }
}

void AutofillProviderAndroid::OnServerPredictionsAvailable(
    AndroidAutofillManager& manager,
    FormGlobalId form_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MaybeSendPrefillRequest(manager, form_id);

  if (!IsIdOfLinkedForm(form_id)) {
    return;
  }

  CHECK(manager_);
  const FormStructure* form_structure = manager_->FindCachedFormById(form_id);
  if (!form_structure) {
    return;
  }
  form_->UpdateFieldTypes(*form_structure);
  bridge_->OnServerPredictionsAvailable();
}

void AutofillProviderAndroid::OnManagerResetOrDestroyed(
    AndroidAutofillManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsLinkedManager(manager)) {
    return;
  }

  // If we previously received a notification from the renderer that the form
  // was likely submitted and no event caused a reset of state in the interim,
  // we consider this navigation to be resulting from the submission.
  if (check_submission_ && form_.get()) {
    FireSuccessfulSubmission(pending_submission_source_);
    return;
  }

  Reset();
}

bool AutofillProviderAndroid::GetCachedIsAutofilled(
    const FormFieldData& field) const {
  size_t field_index = 0u;
  return form_ && form_->GetFieldIndex(field, &field_index) &&
         form_->form().fields[field_index].is_autofilled;
}

bool AutofillProviderAndroid::IntendsToShowBottomSheet(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field,
    const FormData& form_data) const {
  return !has_used_cached_form_ && cached_data_ && cached_data_->cached_form &&
         form == cached_data_->cached_form->form().global_id();
}

bool AutofillProviderAndroid::WasBottomSheetJustShown(
    AutofillManager& manager) {
  // TODO(crbug.com/1490581) Remove the timer once a fix is landed on the
  // renderer side.
  was_shown_bottom_sheet_timer_.Start(
      FROM_HERE, kWasBottomSheetShownFlipTimeout, this,
      &AutofillProviderAndroid::SetBottomSheetShownOff);
  return was_bottom_sheet_just_shown_;
}

void AutofillProviderAndroid::SetBottomSheetShownOff() {
  was_bottom_sheet_just_shown_ = false;
}

void AutofillProviderAndroid::MaybeInitKeyboardSuppressor() {
  // Return early if prefill requests are not supported.
  if (!ArePrefillRequestsSupported()) {
    return;
  }
  keyboard_suppressor_ = std::make_unique<TouchToFillKeyboardSuppressor>(
      ContentAutofillClient::FromWebContents(web_contents()),
      base::BindRepeating(&AutofillProviderAndroid::WasBottomSheetJustShown,
                          base::Unretained(this)),
      base::BindRepeating(&AutofillProviderAndroid::IntendsToShowBottomSheet,
                          base::Unretained(this)),
      kKeyboardSuppressionTimeout);
}

bool AutofillProviderAndroid::IsLinkedManager(
    AndroidAutofillManager* manager) const {
  return manager == manager_.get();
}

bool AutofillProviderAndroid::IsIdOfLinkedForm(FormGlobalId form_id) const {
  return form_ && form_->form().global_id() == form_id;
}

bool AutofillProviderAndroid::IsLinkedForm(const FormData& form,
                                           const char* similarity_metric) {
  if (!form_) {
    return false;
  }

  if (!similarity_metric) {
    return form_->SimilarFormAs(form);
  }

  FormDataAndroid::SimilarityCheckResult similarity_result =
      form_->SimilarFormAsWithDiagnosis(form);
  base::UmaHistogramExactLinear(
      similarity_metric,
      ProjectSimilarityCheckResultToMetricsValue(similarity_result),
      FormDataAndroid::kSimilaryCheckResultExclusiveMaximum);
  return similarity_result == FormDataAndroid::kFormsAreSimilar;
}

gfx::RectF AutofillProviderAndroid::ToClientAreaBound(
    const gfx::RectF& bounding_box) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  return bounding_box + client_area.OffsetFromOrigin();
}

void AutofillProviderAndroid::Reset() {
  manager_ = nullptr;
  form_.reset();
  last_focused_field_id_ = {};
  field_type_group_ = FieldTypeGroup::kNoGroup;
  triggered_origin_ = {};
  check_submission_ = false;
  was_shown_bottom_sheet_timer_.Stop();
  was_bottom_sheet_just_shown_ = false;

  if (base::FeatureList::IsEnabled(
          features::kAndroidAutofillCancelSessionOnNavigation)) {
    CancelSession();
  }

  // Resets the Java instance and hides the datalist popup if there is one.
  bridge_->Reset();
  // TODO(crbug.com/1488233): Also send an unfocus event to make sure that the
  // Autofill session is truly terminated.
}

void AutofillProviderAndroid::CancelSession() {
  cached_data_ = std::nullopt;
  has_used_cached_form_ = false;
  was_bottom_sheet_just_shown_ = false;
  was_shown_bottom_sheet_timer_.Stop();
  bridge_->CancelSession();
}

SessionId AutofillProviderAndroid::CreateSessionId() {
  last_session_id_ = last_session_id_ == kMaximumSessionId
                         ? kMinimumSessionId
                         : SessionId(last_session_id_.value() + 1);
  return last_session_id_;
}

bool AutofillProviderAndroid::ArePrefillRequestsSupported() const {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
             kMinimumSdkVersionForPrefillRequests &&
         base::FeatureList::IsEnabled(
             features::kAndroidAutofillPrefillRequestsForLoginForms);
}

void AutofillProviderAndroid::MaybeSendPrefillRequest(
    const AndroidAutofillManager& manager,
    FormGlobalId form_id) {
  if (!ArePrefillRequestsSupported()) {
    return;
  }

  // Return if there has already been a cache request or if there is already an
  // ongoing Autofill session.
  if (cached_data_ || form_) {
    return;
  }

  const FormStructure* const form_structure =
      manager.FindCachedFormById(form_id);
  if (!form_structure) {
    return;
  }
  std::unique_ptr<PasswordForm> pw_form = ParseToPasswordForm(*form_structure);
  if (!pw_form || !ShouldCachePasswordForm(*pw_form)) {
    return;
  }

  cached_data_.emplace();
  cached_data_->prefill_request_creation_time = base::TimeTicks::Now();
  cached_data_->cached_form = std::make_unique<FormDataAndroid>(
      form_structure->ToFormData(), CreateSessionId());
  cached_data_->cached_form->UpdateFieldTypes(*form_structure);
  if (std::optional<PasswordParserOverrides> overrides =
          PasswordParserOverrides::FromLoginForm(*pw_form, *form_structure);
      overrides &&
      base::FeatureList::IsEnabled(
          features::kAndroidAutofillUsePwmPredictionsForOverrides)) {
    // If we manage to match the fields that the password form parser identified
    // as username and password fields, override their types.
    cached_data_->password_parser_overrides = *std::move(overrides);
    cached_data_->cached_form->UpdateFieldTypes(
        cached_data_->password_parser_overrides.ToFieldTypeMap());
  }
  bridge_->SendPrefillRequest(*cached_data_->cached_form);
}

base::flat_map<FieldGlobalId, AutofillType>
AutofillProviderAndroid::PasswordParserOverrides::ToFieldTypeMap() const {
  base::flat_map<FieldGlobalId, AutofillType> result;
  if (username_field_id) {
    result.emplace(*username_field_id, FieldType::USERNAME);
  }
  if (password_field_id) {
    result.emplace(*password_field_id, FieldType::PASSWORD);
  }
  return result;
}

// static
std::optional<AutofillProviderAndroid::PasswordParserOverrides>
AutofillProviderAndroid::PasswordParserOverrides::FromLoginForm(
    const PasswordForm& pw_form,
    const FormStructure& form_structure) {
  PasswordParserOverrides result;
  for (const std::unique_ptr<AutofillField>& field : form_structure) {
    if (field->renderer_id() == pw_form.username_element_renderer_id) {
      if (result.username_field_id) {
        return std::nullopt;
      }
      result.username_field_id = field->global_id();
    } else if (field->renderer_id() == pw_form.password_element_renderer_id) {
      if (result.password_field_id) {
        return std::nullopt;
      }
      result.password_field_id = field->global_id();
    }
  }
  // A login form must always have a username field and a password field.
  if (!result.username_field_id || !result.password_field_id) {
    // TODO(crbug.com/41496211): This should never be reachable. Remove once it
    // is clear how it can happen.
    SCOPED_CRASH_KEY_NUMBER("crbug1523259", "pw_form.username_id",
                            pw_form.username_element_renderer_id.value());
    SCOPED_CRASH_KEY_NUMBER("crbug1523259", "pw_form.password_id",
                            pw_form.password_element_renderer_id.value());
    SCOPED_CRASH_KEY_NUMBER("crbug1523259", "fs.fields.size",
                            form_structure.fields().size());
    SCOPED_CRASH_KEY_NUMBER("crbug1523259", "fs.form_signature",
                            form_structure.form_signature().value());
    SCOPED_CRASH_KEY_STRING1024("crbug1523259", "fs.fields.global_ids", [&] {
      std::ostringstream ss;
      for (size_t i = 0;
           i < std::min<size_t>(10u, form_structure.fields().size()); ++i) {
        ss << form_structure.fields()[i]->global_id() << " ";
      }
      return ss.str();
    }());
    base::debug::DumpWithoutCrashing();
    return std::nullopt;
  }
  return result;
}

AutofillProviderAndroid::CachedData::CachedData() = default;

AutofillProviderAndroid::CachedData::CachedData(CachedData&&) = default;

AutofillProviderAndroid::CachedData&
AutofillProviderAndroid::CachedData::operator=(CachedData&&) = default;

AutofillProviderAndroid::CachedData::~CachedData() = default;

}  // namespace autofill
