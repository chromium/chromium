// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_tracker.h"

#include <optional>
#include <variant>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-data-view.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/modules/autofill/web_form_element_observer.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "ui/base/page_transition_types.h"

using blink::WebDocument;
using blink::WebDocumentLoader;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;

namespace autofill {

namespace {

using enum CallTimerState::CallSite;

// Used for metrics. Do not renumber.
// This enum is supposed to identify what is being returned by
// `AutofillAgent::GetSubmittedForm`: Either no form (null) which means that
// fetching the submitted form failed, or a form that was extracted at the time
// of calling the function, or a form that was extracted before and cached until
// submission time.
enum class SubmittedFormType { kNull = 0, kExtracted = 1, kCached = 2 };

constexpr char kAutofillAgentSubmissionSourceHistogram[] =
    "Autofill.SubmissionDetectionSource.AutofillAgent";
constexpr char kFormTrackerSubmissionSourceHistogram[] =
    "Autofill.SubmissionDetectionSource.FormTracker";

bool ShouldReplaceElementsByRendererIds() {
  return base::FeatureList::IsEnabled(
      features::kAutofillReplaceCachedWebElementsByRendererIds);
}

void LogSubmittedFormMetric(mojom::SubmissionSource source,
                            SubmittedFormType type) {
  // Used for metrics. Do not renumber.
  enum class SubmittedFormTypeBySource {
    kNone_Null = 0,
    kNone_Extracted = 1,
    kNone_Cached = 2,
    kSameDocumentNavigation_Null = 3,
    kSameDocumentNavigation_Extracted = 4,
    kSameDocumentNavigation_Cached = 5,
    kXhrSucceeded_Null = 6,
    kXhrSucceeded_Extracted = 7,
    kXhrSucceeded_Cached = 8,
    kFrameDetached_Null = 9,
    kFrameDetached_Extracted = 10,
    kFrameDetached_Cached = 11,
    kProbableFormSubmission_Null = 12,
    kProbableFormSubmission_Extracted = 13,
    kProbableFormSubmission_Cached = 14,
    kFormSubmission_Null = 15,
    kFormSubmission_Extracted = 16,
    kFormSubmission_Cached = 17,
    kDomMutationAfterAutofill_Null = 18,
    kDomMutationAfterAutofill_Extracted = 19,
    kDomMutationAfterAutofill_Cached = 20,
    kTotal_Null = 21,
    kTotal_Extracted = 22,
    kTotal_Cached = 23,
    kMaxValue = kTotal_Cached
  };
  static_assert(
      base::to_underlying(SubmittedFormTypeBySource::kMaxValue) + 1 ==
          3 * (base::to_underlying(mojom::SubmissionSource::kMaxValue) + 2),
      "SubmittedFormTypeBySource should have three values for each value of "
      "SubmissionSource in addition to three `Total` values");

  using underlying_type = std::underlying_type_t<SubmittedFormTypeBySource>;
  underlying_type source_bucket = base::to_underlying(source) * 3;
  underlying_type total_bucket =
      base::to_underlying(SubmittedFormTypeBySource::kTotal_Null);
  underlying_type offset = base::to_underlying(type);
  base::UmaHistogramEnumeration(
      "Autofill.SubmissionDetection.SubmittedFormType",
      static_cast<SubmittedFormTypeBySource>(source_bucket + offset));
  base::UmaHistogramEnumeration(
      "Autofill.SubmissionDetection.SubmittedFormType",
      static_cast<SubmittedFormTypeBySource>(total_bucket + offset));
}

}  // namespace

using mojom::SubmissionSource;

FormRef::FormRef(blink::WebFormElement form)
    : form_renderer_id_(form_util::GetFormRendererId(form)) {
  if (!ShouldReplaceElementsByRendererIds()) {
    form_ = form;
  }
}

blink::WebFormElement FormRef::GetForm() const {
  return ShouldReplaceElementsByRendererIds()
             ? form_util::GetFormByRendererId(form_renderer_id_)
             : form_;
}

FormRendererId FormRef::GetId() const {
  return ShouldReplaceElementsByRendererIds()
             ? form_renderer_id_
             : form_util::GetFormRendererId(form_);
}

FieldRef::FieldRef(blink::WebFormControlElement form_control)
    : field_renderer_id_(form_util::GetFieldRendererId(form_control)) {
  CHECK(form_control);
  if (!ShouldReplaceElementsByRendererIds()) {
    field_ = form_control;
  }
}

FieldRef::FieldRef(blink::WebElement content_editable)
    : field_renderer_id_(content_editable.GetDomNodeId()) {
  CHECK(content_editable);
  CHECK(content_editable.IsContentEditable());
  if (!ShouldReplaceElementsByRendererIds()) {
    field_ = content_editable;
  }
}

bool operator<(const FieldRef& lhs, const FieldRef& rhs) {
  return lhs.field_renderer_id_ < rhs.field_renderer_id_;
}

blink::WebFormControlElement FieldRef::GetField() const {
  return ShouldReplaceElementsByRendererIds()
             ? form_util::GetFormControlByRendererId(field_renderer_id_)
             : field_.DynamicTo<WebFormControlElement>();
}

blink::WebElement FieldRef::GetContentEditable() const {
  blink::WebElement content_editable =
      ShouldReplaceElementsByRendererIds()
          ? form_util::GetContentEditableByRendererId(field_renderer_id_)
          : field_;
  return content_editable && content_editable.IsContentEditable()
             ? content_editable
             : blink::WebElement();
}

FieldRendererId FieldRef::GetId() const {
  return ShouldReplaceElementsByRendererIds() ? field_renderer_id_
         : field_ ? form_util::GetFieldRendererId(field_)
                  : FieldRendererId();
}

FormTracker::FormTracker(content::RenderFrame* render_frame,
                         AutofillAgent& autofill_agent,
                         PasswordAutofillAgent& password_autofill_agent)
    : content::RenderFrameObserver(render_frame),
      blink::WebLocalFrameObserver(render_frame->GetWebFrame()),
      autofill_agent_(autofill_agent),
      password_autofill_agent_(password_autofill_agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
}

FormTracker::~FormTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::AjaxSucceeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  submission_triggering_events_.xhr_succeeded = true;
  FireSubmissionIfFormDisappear(SubmissionSource::XHR_SUCCEEDED);
}

void FormTracker::TextFieldValueChanged(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  DCHECK(element.DynamicTo<WebInputElement>() ||
         form_util::IsTextAreaElement(element));
  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.Focused()) {
    return;
  }

  if (!unsafe_render_frame()) {
    return;
  }
  // Disregard text changes that aren't caused by user gestures or pastes. Note
  // that pastes aren't necessarily user gestures because Blink's conception of
  // user gestures is centered around creating new windows/tabs.
  if (user_gesture_required_ &&
      !unsafe_render_frame()->GetWebFrame()->HasTransientUserActivation() &&
      !unsafe_render_frame()->IsPasting()) {
    return;
  }
  // We post a task for doing the Autofill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  weak_ptr_factory_.InvalidateWeakPtrs();
  unsafe_render_frame()
      ->GetWebFrame()
      ->GetTaskRunner(blink::TaskType::kInternalAutofill)
      ->PostTask(FROM_HERE,
                 base::BindRepeating(&FormTracker::FormControlDidChangeImpl,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     form_util::GetFieldRendererId(element),
                                     SaveFormReason::kTextFieldChanged));
}

void FormTracker::SelectControlSelectionChanged(
    const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!unsafe_render_frame()) {
    return;
  }
  // Post a task to avoid processing select control change while it is changing.
  weak_ptr_factory_.InvalidateWeakPtrs();
  unsafe_render_frame()
      ->GetWebFrame()
      ->GetTaskRunner(blink::TaskType::kInternalAutofill)
      ->PostTask(FROM_HERE,
                 base::BindRepeating(&FormTracker::FormControlDidChangeImpl,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     form_util::GetFieldRendererId(element),
                                     SaveFormReason::kSelectChanged));
}

void FormTracker::ElementDisappeared(const blink::WebElement& element) {
  // Signal is discarded altogether when the feature is disabled.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
    return;
  }
  if (!element.DynamicTo<WebFormElement>() &&
      !element.DynamicTo<WebFormControlElement>()) {
    return;
  }
  // If tracking a form, any disappearance other than that form is not
  // interesting.
  if (element.DynamicTo<WebFormElement>() &&
      last_interacted_.form.GetId() != form_util::GetFormRendererId(element)) {
    return;
  }
  // If tracking a field, any disappearance other than that field is not
  // interesting.
  if (element.DynamicTo<WebFormControlElement>() &&
      last_interacted_.formless_element.GetId() !=
          form_util::GetFieldRendererId(element)) {
    return;
  }
  if (submission_triggering_events_.xhr_succeeded) {
    FireFormSubmission(mojom::SubmissionSource::XHR_SUCCEEDED,
                       /*submitted_form_element=*/std::nullopt,
                       // TODO(crbug.com/40281981): Figure out if this is still
                       // needed, and document the reason, otherwise remove.
                       /*reset_last_interacted_elements=*/true);
    return;
  }
  if (submission_triggering_events_.finished_same_document_navigation) {
    FireFormSubmission(mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION,
                       /*submitted_form_element=*/std::nullopt,
                       // TODO(crbug.com/40281981): Figure out if this is still
                       // needed, and document the reason, otherwise remove.
                       /*reset_last_interacted_elements=*/true);
    return;
  }
  if (submission_triggering_events_.tracked_element_autofilled) {
    FireFormSubmission(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL,
                       /*submitted_form_element=*/std::nullopt,
                       /*reset_last_interacted_elements=*/false);
    return;
  }
  submission_triggering_events_.tracked_element_disappeared = true;
}

void FormTracker::TrackAutofilledElement(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!form_util::GetFormControlByRendererId(
          form_util::GetFieldRendererId(element))) {
    return;
  }
  blink::WebFormElement form_element = element.GetOwningFormForAutofill();
  if (form_element) {
    UpdateLastInteractedElement(form_util::GetFormRendererId(form_element));
  } else {
    UpdateLastInteractedElement(form_util::GetFieldRendererId(element));
  }
  submission_triggering_events_.tracked_element_autofilled = true;
  TrackElement(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
}

void FormTracker::TrackAutofilledElement(
    const base::flat_map<FieldRendererId, FormRendererId>&
        filled_fields_and_forms) {
  auto field_is_owned =
      [](const std::pair<FieldRendererId, FormRendererId>&
             filled_field_and_form) {
        return !form_util::GetFormByRendererId(filled_field_and_form.second)
                    .IsNull();
      };
  if (auto it = std::ranges::find_if(filled_fields_and_forms, field_is_owned);
      it != filled_fields_and_forms.end()) {
    const auto& [filled_field_id, filled_form_id] = *it;
    if (base::FeatureList::IsEnabled(
            features::kAutofillAcceptDomMutationAfterAutofillSubmission)) {
      TrackAutofilledElement(
          form_util::GetFormControlByRendererId(filled_field_id));
    } else {
      UpdateLastInteractedElement(filled_form_id);
    }
  } else {
    for (const auto& [filled_field_id, filled_form_id] :
         filled_fields_and_forms) {
      WebFormControlElement control_element =
          form_util::GetFormControlByRendererId(filled_field_id);
      CHECK(control_element);
      if (base::FeatureList::IsEnabled(
              features::kAutofillAcceptDomMutationAfterAutofillSubmission)) {
        TrackAutofilledElement(control_element);
      } else {
        UpdateLastInteractedElement(
            form_util::GetFieldRendererId(control_element));
      }
    }
  }
}

void FormTracker::OnJavaScriptChangedValue(
    const WebFormControlElement& element) {
  // The provisionally saved form must be updated on JS changes. However, it
  // should not be changed to another form, so that only the user can set the
  // tracked form and not JS. This call here is meant to keep the tracked form
  // up to date with the form's most recent version.
  if (provisionally_saved_form() &&
      form_util::GetFormRendererId(element.GetOwningFormForAutofill()) ==
          last_interacted_form().GetId()) {
    // Ideally, we re-extract the form at this moment, but to avoid performance
    // regression, we just update what JS updated on the Blink side.
    std::vector<FormFieldData> fields =
        provisionally_saved_form()->ExtractFields();
    if (auto it =
            std::ranges::find(fields, form_util::GetFieldRendererId(element),
                              &FormFieldData::renderer_id);
        it != fields.end()) {
      it->set_value(element.Value().Utf16().substr(0, kMaxStringLength));
      it->set_is_autofilled(element.IsAutofilled());
      form_util::MaybeUpdateUserInput(*it,
                                      form_util::GetFieldRendererId(element),
                                      autofill_agent_->field_data_manager());
    }
    provisionally_saved_form()->set_fields(std::move(fields));
  }

  const auto input_element = element.DynamicTo<WebInputElement>();
  if (input_element && input_element.IsTextField() &&
      !element.Value().IsEmpty() &&
      (input_element.FormControlTypeForAutofill() ==
           blink::mojom::FormControlType::kInputPassword ||
       password_autofill_agent_->IsUsernameInputField(input_element))) {
    password_autofill_agent_->UpdatePasswordStateForTextChange(
        input_element,
        /*form_cache=*/{});
  }
}

void FormTracker::FormControlDidChangeImpl(FieldRendererId element_id,
                                           SaveFormReason change_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  WebFormControlElement element =
      form_util::GetFormControlByRendererId(element_id);
  // This function may be called asynchronously, so a navigation may have
  // happened. Since this event isn't submission-related.
  if (!form_util::IsOwnedByFrame(element, unsafe_render_frame())) {
    return;
  }
  blink::WebFormElement form_element = element.GetOwningFormForAutofill();
  if (form_element) {
    UpdateLastInteractedElement(form_util::GetFormRendererId(form_element));
  } else {
    UpdateLastInteractedElement(form_util::GetFieldRendererId(element));
  }
  switch (change_source) {
    case SaveFormReason::kTextFieldChanged:
      autofill_agent_->OnTextFieldValueChanged(
          element,
          SynchronousFormCache(form_util::GetFormRendererId(form_element),
                               provisionally_saved_form()));
      break;
    case SaveFormReason::kSelectChanged:
      autofill_agent_->OnSelectControlSelectionChanged(
          element,
          SynchronousFormCache(form_util::GetFormRendererId(form_element),
                               provisionally_saved_form()));
      break;
  }
}

void FormTracker::DidCommitProvisionalLoad(ui::PageTransition transition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::DidFinishSameDocumentNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  submission_triggering_events_.finished_same_document_navigation = true;
  FireSubmissionIfFormDisappear(SubmissionSource::SAME_DOCUMENT_NAVIGATION);
}

void FormTracker::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  // Ony handle primary main frame.
  if (!unsafe_render_frame() ||
      !unsafe_render_frame()->GetWebFrame()->IsOutermostMainFrame()) {
    return;
  }

  // We are interested only in content-initiated navigations. Explicit browser
  // initiated navigations (e.g. via omnibox) don't have a navigation type
  // and are discarded here.
  if (navigation_type.has_value() &&
      navigation_type.value() != blink::kWebNavigationTypeLinkClicked) {
    FireFormSubmission(
        mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED,
        /*submitted_form_element=*/std::nullopt,
        /*reset_last_interacted_elements=*/
        !base::FeatureList::IsEnabled(features::kAutofillFixFormTracking));
  }
}

void FormTracker::WillDetach(blink::DetachReason detach_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!unsafe_render_frame()) {
    return;
  }
  if (detach_reason == blink::DetachReason::kFrameDeletion &&
      !unsafe_render_frame()->GetWebFrame()->IsOutermostMainFrame()) {
    // Exclude cases where the previous RenderFrame gets deleted only to be
    // replaced by a new RenderFrame, which happens on navigations. This is so
    // that we only trigger inferred form submission if the actual frame
    // (<iframe> element etc) gets detached.
    FireFormSubmission(SubmissionSource::FRAME_DETACHED,
                       /*submitted_form_element=*/std::nullopt,
                       // TODO(crbug.com/40281981): Figure out if this is still
                       // needed, and document the reason, otherwise remove.
                       /*reset_last_interacted_elements=*/true);
  }
  // TODO(crbug.com/40281981): Figure out if this is still needed, and
  // document the reason, otherwise remove.
  ResetLastInteractedElements();
}

void FormTracker::WillSendSubmitEvent(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  CHECK(form);
  // TODO(crbug.com/40281981): Figure out if this is still needed, and document
  // the reason, otherwise remove.
  UpdateLastInteractedElement(form_util::GetFormRendererId(form));
  // TODO(crbug.com/40281981): Figure out if this is still needed, and
  // document the reason, otherwise remove.
  password_autofill_agent_->InformBrowserAboutUserInput(
      form, WebInputElement(),
      SynchronousFormCache(form_util::GetFormRendererId(form),
                           provisionally_saved_form()));
  // Fire the form submission event to avoid missing submissions where websites
  // cancel the onsubmit event. This also gets the form before Javascript's
  // submit event handler could change it. We don't clear submitted_forms_
  // because OnFormSubmitted will normally be invoked afterwards and we don't
  // want to fire the same event twice.
  FireFormSubmission(mojom::SubmissionSource::FORM_SUBMISSION, form,
                     /*reset_last_interacted_elements=*/false);
}

void FormTracker::WillSubmitForm(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  // A form submission may target a frame other than the frame that owns |form|.
  // The WillSubmitForm() event is only fired on the target frame's FormTracker
  // (provided that both have the same origin). In such a case, we ignore the
  // form submission event. If we didn't, we would send |form| to an
  // AutofillAgent and then to a ContentAutofillDriver etc. which haven't seen
  // this form before. See crbug.com/1240247#c13 for details.
  if (!form_util::IsOwnedByFrame(form, unsafe_render_frame())) {
    return;
  }
  FireFormSubmission(
      mojom::SubmissionSource::FORM_SUBMISSION, form,
      /*reset_last_interacted_elements=*/
      !base::FeatureList::IsEnabled(features::kAutofillFixFormTracking));
}

void FormTracker::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::FireFormSubmission(
    SubmissionSource source,
    std::optional<WebFormElement> submitted_form_element,
    bool reset_last_interacted_elements) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!IsTracking() && source != mojom::SubmissionSource::FORM_SUBMISSION) {
    // If no form is being tracked, there's no need to inform the agent of
    // submission since no submitted form will be fetched. The only source
    // that's an exception for this is SubmissionSource::FORM_SUBMISSION since
    // it provides the submitted form element and therefore no tracking is
    // needed.
    return;
  }
  base::UmaHistogramEnumeration(kFormTrackerSubmissionSourceHistogram, source);

  if (source == mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL) {
    // TODO(crbug.com/40281981): Investigate removing this and relying on the
    // call conditioned on the submitted form.
    password_autofill_agent_->FireHostSubmitEvent(
        FormRendererId(), /*submitted_form=*/std::nullopt, source);
  }
  if (std::optional<FormData> form_data =
          GetSubmittedForm(source, submitted_form_element)) {
    FireHostSubmitEvents(*form_data, source);
  }

  if (reset_last_interacted_elements) {
    ResetLastInteractedElements();
  }
  switch (source) {
    case mojom::SubmissionSource::FORM_SUBMISSION:
    case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      break;
    case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
    case mojom::SubmissionSource::XHR_SUCCEEDED:
    case mojom::SubmissionSource::FRAME_DETACHED:
    case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
      // TODO(crbug.com/40281981): Figure out if this is still needed, and
      // document the reason, otherwise remove.
      OnFormNoLongerSubmittable();
      break;
    case mojom::SubmissionSource::NONE:
      NOTREACHED();
  }
}

void FormTracker::FireSubmissionIfFormDisappear(SubmissionSource source) {
  if (CanInferFormSubmitted() ||
      (submission_triggering_events_.tracked_element_disappeared &&
       base::FeatureList::IsEnabled(
           features::kAutofillReplaceFormElementObserver))) {
    FireFormSubmission(source, /*submitted_form_element=*/std::nullopt,
                       // TODO(crbug.com/40281981): Figure out if this is still
                       // needed, and document the reason, otherwise remove.
                       /*reset_last_interacted_elements=*/true);
    return;
  }
  TrackElement(source);
}

bool FormTracker::CanInferFormSubmitted() {
  if (last_interacted_.form.GetId()) {
    WebFormElement last_interacted_form = last_interacted_.form.GetForm();
    // Infer submission if the form was removed or all its elements are hidden.
    return !last_interacted_form ||
           std::ranges::none_of(
               last_interacted_form.GetFormControlElements(),  // nocheck
               &WebElement::IsFocusable);
  }
  if (last_interacted_.formless_element.GetId()) {
    WebFormControlElement last_interacted_formless_element =
        last_interacted_.formless_element.GetField();
    // Infer submission if the field was removed or it's hidden.
    return !last_interacted_formless_element ||
           !last_interacted_formless_element.IsFocusable();
  }
  return false;
}

void FormTracker::TrackElement(mojom::SubmissionSource source) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
    // Do not use WebFormElementObserver. Instead, rely on the signal
    // `FormTracker::ElementDisappeared` coming from blink.
    return;
  }
  // Already has observer for last interacted element.
  if (form_element_observer_) {
    return;
  }
  auto callback = base::BindOnce(&FormTracker::ElementWasHiddenOrRemoved,
                                 base::Unretained(this), source);

  if (WebFormElement last_interacted_form = last_interacted_.form.GetForm()) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_form, std::move(callback));
  } else if (WebFormControlElement last_interacted_formless_element =
                 last_interacted_.formless_element.GetField()) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_formless_element, std::move(callback));
  }
}

void FormTracker::FireHostSubmitEvents(const FormData& form_data,
                                       mojom::SubmissionSource source) {
  if (source == mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL &&
      !base::FeatureList::IsEnabled(
          features::kAutofillAcceptDomMutationAfterAutofillSubmission)) {
    return;
  }
  DenseSet<mojom::SubmissionSource>& sources =
      submitted_forms_[form_data.renderer_id()];
  if (!sources.insert(source).second) {
    // The form (identified by its renderer id) was already submitted with the
    // same submission source. This should not be reported multiple times.
    return;
  }
  // This is the first time the form was submitted with the given source. It is
  // still possible, however, that another submission with another source was
  // recorded, making this one obsolete. (More details below)

  // This checks whether another source, that is relevant for Autofill, already
  // reported the submission of `form_data`.
  const bool is_duplicate_submission_for_autofill = [&] {
    DenseSet<mojom::SubmissionSource> af_sources = sources;
    // Autofill ignores DOM_MUTATION_AFTER_AUTOFILL on non-WebView platforms.
    // For this reason, the presence of DOM_MUTATION_AFTER_AUTOFILL in the
    // submission history is not sufficient to skip reporting `source`. On
    // WebView, no duplicate filtering is required since the provider is reset
    // on submission, meaning that subsequent submission signals will just be
    // ignored.
    af_sources.erase(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
    return af_sources.size() > 1;
  }();

  // This checks whether another source, that is relevant for PasswordManager,
  // already reported the submission of `form_data`.
  const bool is_duplicate_submission_for_password_manager = [&] {
    DenseSet<mojom::SubmissionSource> pwm_sources = sources;
    // PasswordManager doesn't consider FORM_SUBMISSION as a sufficient
    // condition for "successful" submission.
    pwm_sources.erase(mojom::SubmissionSource::FORM_SUBMISSION);
    // PasswordManager completely ignores PROBABLY_FORM_SUBMITTED.
    pwm_sources.erase(mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
    return pwm_sources.size() > 1;
  }();

  if (!is_duplicate_submission_for_password_manager) {
    password_autofill_agent_->FireHostSubmitEvent(form_data.renderer_id(),
                                                  form_data, source);
  }
  if (!is_duplicate_submission_for_autofill) {
    base::UmaHistogramEnumeration(kAutofillAgentSubmissionSourceHistogram,
                                  source);
    autofill_agent_->FireHostSubmitEvents(form_data, source);
  }
  // Bound the size of `submitted_forms_` to avoid possible memory leaks.
  if (submitted_forms_.size() > 200) {
    submitted_forms_.erase(--submitted_forms_.end());
  }
}

std::optional<FormData> FormTracker::GetSubmittedForm(
    mojom::SubmissionSource source,
    std::optional<WebFormElement> submitted_form_element) {
  std::optional<FormData> cached_form = provisionally_saved_form();
  const bool cache_matches_submitted_form_element =
      !submitted_form_element.has_value() || !cached_form ||
      cached_form->renderer_id() ==
          form_util::GetFormRendererId(*submitted_form_element);

  // Behavior when `AutofillReplaceFormElementObserver` is enabled:
  // - Never try to extract and unconditionally look at the provisionally saved
  //   form. The reason is that some form extraction could happen during style
  //   recalc, meaning that querying field focusability would crash.
  if (base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
    LogSubmittedFormMetric(source, cached_form ? SubmittedFormType::kCached
                                               : SubmittedFormType::kNull);
    return cached_form;
  }

  // Behavior when the submission is a result of a detached iframe:
  // - Look at the cached form and don't try extracting the form from the frame
  //   since the frame became disconnected.
  // TODO(crbug.com/40281981): Investigate following the default behavior for
  // this source (i.e. trying to extract anyways).
  if (source == mojom::SubmissionSource::FRAME_DETACHED) {
    LogSubmittedFormMetric(source, cached_form ? SubmittedFormType::kCached
                                               : SubmittedFormType::kNull);
    return cached_form;
  }

  WebDocument document = GetDocument();
  std::optional<FormData> extracted_form = form_util::ExtractFormData(
      document,
      submitted_form_element.has_value() ? *submitted_form_element
                                         : last_interacted_form().GetForm(),
      autofill_agent_->field_data_manager(),
      autofill_agent_->GetCallTimerState(kGetSubmittedForm),
      autofill_agent_->button_titles_cache());

  // - Return null if there was no interaction so far and no `form_element` is
  //   provided.
  // - Primarily look at the provisionally saved form.
  // - In case there isn't one try extracting the form (either
  //   `last_interacted_form()` or `form_element` if provided).
  if (cached_form && cache_matches_submitted_form_element) {
    LogSubmittedFormMetric(source, SubmittedFormType::kCached);
    return cached_form;
  }
  LogSubmittedFormMetric(source, extracted_form ? SubmittedFormType::kExtracted
                                                : SubmittedFormType::kNull);
  return extracted_form;
}

void FormTracker::UpdateLastInteractedElement(
    std::variant<FormRendererId, FieldRendererId> element_id) {
  ResetLastInteractedElements();

  // `document` is the WebDocument of `element_id`'s element. It is not
  // necessarily the same as the current frame's document.
  //
  // `form` is null if `element_id` is a FieldRendererId.
  auto [document, form_element] = std::visit(
      absl::Overload{
          [this](FormRendererId form_id) {
            CHECK(form_id);
            WebFormElement form = form_util::GetFormByRendererId(form_id);
            last_interacted_.form =
                FormRef(form_util::GetFormByRendererId(form_id));
            return std::pair(form.GetDocument(), form);
          },
          [this](FieldRendererId field_id) {
            CHECK(field_id);
            WebFormControlElement form_control =
                form_util::GetFormControlByRendererId(field_id);
            last_interacted_.formless_element = FieldRef(form_control);
            return std::pair(form_control.GetDocument(), WebFormElement());
          },
      },
      element_id);
  CHECK(document);

  // We use the element's `document`, not the current frame's document, because
  // `element_id` may refer to an element that is not in the current frame's
  // document.
  last_interacted_.saved_state = form_util::ExtractFormData(
      document, form_element, autofill_agent_->field_data_manager(),
      autofill_agent_->GetCallTimerState(
          CallTimerState::CallSite::kUpdateLastInteractedElement),
      autofill_agent_->button_titles_cache());
}

void FormTracker::ResetLastInteractedElements() {
  last_interacted_ = {};
  submission_triggering_events_ = {};
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
}

void FormTracker::SetUserGestureRequired(
    UserGestureRequired user_gesture_required) {
  user_gesture_required_ = user_gesture_required;
}

bool FormTracker::IsTracking() const {
  return last_interacted_.form.GetId() ||
         last_interacted_.formless_element.GetId() ||
         last_interacted_.saved_state;
}

void FormTracker::ElementWasHiddenOrRemoved(mojom::SubmissionSource source) {
  const bool reset_last_interacted_elements = [&] {
    switch (source) {
      case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      case mojom::SubmissionSource::XHR_SUCCEEDED:
        // TODO(crbug.com/40281981): Figure out if this is still needed, and
        // document the reason, otherwise remove.
        return true;
      case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
        return false;
      case mojom::SubmissionSource::FRAME_DETACHED:
      case mojom::SubmissionSource::NONE:
      case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
      case mojom::SubmissionSource::FORM_SUBMISSION:
        NOTREACHED();
    }
  }();
  FireFormSubmission(source, /*submitted_form_element=*/std::nullopt,
                     reset_last_interacted_elements);
}

WebDocument FormTracker::GetDocument() const {
  return unsafe_render_frame()
             ? unsafe_render_frame()->GetWebFrame()->GetDocument()
             : WebDocument();
}

}  // namespace autofill
