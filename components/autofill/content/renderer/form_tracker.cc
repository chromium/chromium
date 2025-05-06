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
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/modules/autofill/web_form_element_observer.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "ui/base/page_transition_types.h"

using blink::WebDocumentLoader;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;

namespace autofill {

namespace {

constexpr char kSubmissionSourceHistogram[] =
    "Autofill.SubmissionDetectionSource.FormTracker";

bool ShouldReplaceElementsByRendererIds() {
  return base::FeatureList::IsEnabled(
      features::kAutofillReplaceCachedWebElementsByRendererIds);
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
                         AutofillAgent& agent)
    : content::RenderFrameObserver(render_frame),
      blink::WebLocalFrameObserver(render_frame->GetWebFrame()),
      agent_(agent) {
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
                       /*submitted_form_element=*/std::nullopt);
    return;
  }
  if (submission_triggering_events_.finished_same_document_navigation) {
    FireFormSubmission(mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION,
                       /*submitted_form_element=*/std::nullopt);
    return;
  }
  if (submission_triggering_events_.tracked_element_autofilled) {
    FireFormSubmission(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL,
                       /*submitted_form_element=*/std::nullopt);
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

void FormTracker::FormControlDidChangeImpl(FieldRendererId element_id,
                                           SaveFormReason change_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  CHECK_NE(change_source, SaveFormReason::kWillSendSubmitEvent);
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
  agent_->OnProvisionallySaveForm(form_element, element, change_source);
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
  if (!unsafe_render_frame()) {
    return;
  }
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
    FireFormSubmission(mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED,
                       /*submitted_form_element=*/std::nullopt);
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
                       /*submitted_form_element=*/std::nullopt);
  }
  // TODO(crbug.com/40281981): Figure out if this is still needed, and
  // document the reason, otherwise remove.
  ResetLastInteractedElements();
}

void FormTracker::WillSendSubmitEvent(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (base::FeatureList::IsEnabled(features::kAutofillOptimizeFormExtraction)) {
    CHECK(form);
    // TODO(crbug.com/40281981): Figure out if this is still needed, and
    // document the reason, otherwise remove.
    UpdateLastInteractedElement(form_util::GetFormRendererId(form));
  }
  agent_->OnProvisionallySaveForm(form, blink::WebFormControlElement(),
                                  SaveFormReason::kWillSendSubmitEvent);
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
  FireFormSubmission(mojom::SubmissionSource::FORM_SUBMISSION, form);
}

void FormTracker::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::FireFormSubmission(
    SubmissionSource source,
    std::optional<WebFormElement> submitted_form_element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!IsTracking() && source != mojom::SubmissionSource::FORM_SUBMISSION) {
    // If no form is being tracked, there's no need to inform the agent of
    // submission since no submitted form will be fetched. The only source
    // that's an exception for this is SubmissionSource::FORM_SUBMISSION since
    // it provides the submitted form element and therefore no tracking is
    // needed.
    return;
  }
  base::UmaHistogramEnumeration(kSubmissionSourceHistogram, source);
  agent_->OnFormSubmission(source, submitted_form_element);
  switch (source) {
    case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
    case mojom::SubmissionSource::FORM_SUBMISSION:
      if (!base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
        ResetLastInteractedElements();
      }
      break;
    case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
    case mojom::SubmissionSource::XHR_SUCCEEDED:
    case mojom::SubmissionSource::FRAME_DETACHED:
      // TODO(crbug.com/40281981): Figure out if this is still needed, and
      // document the reason, otherwise remove.
      ResetLastInteractedElements();
      break;
    case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
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
    FireFormSubmission(source, /*submitted_form_element=*/std::nullopt);
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

void FormTracker::UpdateLastInteractedElement(
    std::variant<FormRendererId, FieldRendererId> element_id) {
  ResetLastInteractedElements();

  // `document` is the WebDocument of `element_id`'s element. It is not
  // necessarily the same as the current frame's document.
  //
  // `form` is null if `element_id` is a FieldRendererId.
  auto [document, form_element] = std::visit(
      base::Overloaded{
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
      document, form_element, agent_->field_data_manager(),
      agent_->GetCallTimerState(
          CallTimerState::CallSite::kUpdateLastInteractedElement),
      agent_->button_titles_cache());
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
  FireFormSubmission(source, /*submitted_form_element=*/std::nullopt);
}

}  // namespace autofill
