// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_tracker.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
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
                         UserGestureRequired user_gesture_required,
                         AutofillAgent& agent)
    : content::RenderFrameObserver(render_frame),
      blink::WebLocalFrameObserver(render_frame->GetWebFrame()),
      user_gesture_required_(user_gesture_required),
      agent_(agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
}

FormTracker::~FormTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::AddObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  observers_.AddObserver(observer);
}

void FormTracker::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  observers_.RemoveObserver(observer);
}

void FormTracker::AjaxSucceeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  submission_triggering_events_.xhr_succeeded = true;
  FireSubmissionIfFormDisappear(SubmissionSource::XHR_SUCCEEDED);
}

void FormTracker::TextFieldDidChange(const WebFormControlElement& element) {
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
      ->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
      ->PostTask(FROM_HERE, base::BindRepeating(
                                &FormTracker::FormControlDidChangeImpl,
                                weak_ptr_factory_.GetWeakPtr(),
                                form_util::GetFieldRendererId(element),
                                Observer::SaveFormReason::kTextFieldChanged));
}

void FormTracker::SelectControlDidChange(const WebFormControlElement& element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!unsafe_render_frame()) {
    return;
  }
  // Post a task to avoid processing select control change while it is changing.
  weak_ptr_factory_.InvalidateWeakPtrs();
  unsafe_render_frame()
      ->GetWebFrame()
      ->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
      ->PostTask(FROM_HERE,
                 base::BindRepeating(&FormTracker::FormControlDidChangeImpl,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     form_util::GetFieldRendererId(element),
                                     Observer::SaveFormReason::kSelectChanged));
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
    FireInferredFormSubmission(mojom::SubmissionSource::XHR_SUCCEEDED);
    return;
  }
  if (submission_triggering_events_.finished_same_document_navigation) {
    FireInferredFormSubmission(
        mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION);
    return;
  }
  if (submission_triggering_events_.tracked_element_autofilled) {
    FireInferredFormSubmission(
        mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
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
  blink::WebFormElement form_element = form_util::GetOwningForm(element);
  if (base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    if (form_element) {
      UpdateLastInteractedElement(form_util::GetFormRendererId(form_element));
    } else {
      UpdateLastInteractedElement(form_util::GetFieldRendererId(element));
    }
  } else {
    ResetLastInteractedElements();
    if (form_element) {
      last_interacted_.form = FormRef(form_element);
    } else {
      last_interacted_.formless_element = FieldRef(element);
    }
  }
  submission_triggering_events_.tracked_element_autofilled = true;
  TrackElement(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
}

void FormTracker::FormControlDidChangeImpl(
    FieldRendererId element_id,
    Observer::SaveFormReason change_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  WebFormControlElement element =
      form_util::GetFormControlByRendererId(element_id);
  // The frame or document or element could be null because this function is
  // called asynchronously.
  if (!unsafe_render_frame() || !element || !element.GetDocument() ||
      !element.GetDocument().GetFrame()) {
    return;
  }
  blink::WebFormElement form_element = form_util::GetOwningForm(element);
  if (base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    if (form_element) {
      UpdateLastInteractedElement(form_util::GetFormRendererId(form_element));
    } else {
      UpdateLastInteractedElement(form_util::GetFieldRendererId(element));
    }
  } else {
    if (form_element) {
      last_interacted_.form = FormRef(form_element);
    } else {
      last_interacted_.formless_element = FieldRef(element);
    }
  }
  for (auto& observer : observers_) {
    observer.OnProvisionallySaveForm(form_element, element, change_source);
  }
}

void FormTracker::DidCommitProvisionalLoad(ui::PageTransition transition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
    ResetLastInteractedElements();
  }
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
    FireProbablyFormSubmitted();
  }
}

void FormTracker::WillDetach(blink::DetachReason detach_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!unsafe_render_frame()) {
    return;
  }
  if (detach_reason == blink::DetachReason::kFrameDeletion &&
      (!unsafe_render_frame()->GetWebFrame()->IsOutermostMainFrame() ||
       !base::FeatureList::IsEnabled(
           features::kAutofillUnifyAndFixFormTracking))) {
    // Exclude cases where the previous RenderFrame gets deleted only to be
    // replaced by a new RenderFrame, which happens on navigations. This is so
    // that we only trigger inferred form submission if the actual frame
    // (<iframe> element etc) gets detached.
    FireInferredFormSubmission(SubmissionSource::FRAME_DETACHED);
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking) &&
      !base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
    // TODO(crbug.com/40281981): Figure out if this is still needed, and
    // document the reason, otherwise remove.
    ResetLastInteractedElements();
  }
}

void FormTracker::WillSendSubmitEvent(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    ResetLastInteractedElements();
    last_interacted_.form = FormRef(form);
  }
  for (auto& observer : observers_) {
    observer.OnProvisionallySaveForm(
        form, blink::WebFormControlElement(),
        Observer::SaveFormReason::kWillSendSubmitEvent);
  }
}

void FormTracker::WillSubmitForm(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  // A form submission may target a frame other than the frame that owns |form|.
  // The WillSubmitForm() event is only fired on the target frame's FormTracker
  // (provided that both have the same origin). In such a case, we ignore the
  // form submission event. If we didn't, we would send |form| to an
  // AutofillAgent and then to a ContentAutofillDriver etc. which haven't seen
  // this form before. See crbug.com/1240247#c13 for details.
  if (!unsafe_render_frame() ||
      !form_util::IsOwnedByFrame(form, unsafe_render_frame())) {
    return;
  }
  FireFormSubmitted(form);
}

void FormTracker::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormTracker::OnFrameDetached() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    ResetLastInteractedElements();
  }
}

void FormTracker::FireFormSubmitted(const blink::WebFormElement& form) {
  base::UmaHistogramEnumeration(kSubmissionSourceHistogram,
                                SubmissionSource::FORM_SUBMISSION);
  for (auto& observer : observers_)
    observer.OnFormSubmitted(form);
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
    ResetLastInteractedElements();
  }
}

void FormTracker::FireProbablyFormSubmitted() {
  base::UmaHistogramEnumeration(kSubmissionSourceHistogram,
                                SubmissionSource::PROBABLY_FORM_SUBMITTED);
  for (auto& observer : observers_)
    observer.OnProbablyFormSubmitted();
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
    ResetLastInteractedElements();
  }
}

void FormTracker::FireInferredFormSubmission(SubmissionSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_tracker_sequence_checker_);
  base::UmaHistogramEnumeration(kSubmissionSourceHistogram, source);
  for (auto& observer : observers_)
    observer.OnInferredFormSubmission(source);
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFormTracking) &&
      (source != SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL ||
       !base::FeatureList::IsEnabled(
           features::kAutofillAcceptDomMutationAfterAutofillSubmission))) {
    ResetLastInteractedElements();
  }
}

void FormTracker::FireSubmissionIfFormDisappear(SubmissionSource source) {
  if (CanInferFormSubmitted() ||
      (submission_triggering_events_.tracked_element_disappeared &&
       base::FeatureList::IsEnabled(
           features::kAutofillReplaceFormElementObserver))) {
    FireInferredFormSubmission(source);
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
               &form_util::IsWebElementFocusableForAutofill);
  }
  if (last_interacted_.formless_element.GetId()) {
    WebFormControlElement last_interacted_formless_element =
        last_interacted_.formless_element.GetField();
    // Infer submission if the field was removed or it's hidden.
    return !last_interacted_formless_element ||
           !form_util::IsWebElementFocusableForAutofill(
               last_interacted_formless_element);
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
    absl::variant<FormRendererId, FieldRendererId> element_id) {
  CHECK(
      base::FeatureList::IsEnabled(features::kAutofillUnifyAndFixFormTracking));
  ResetLastInteractedElements();
  if (absl::holds_alternative<FormRendererId>(element_id)) {
    FormRendererId form_id = absl::get<FormRendererId>(element_id);
    CHECK(form_id);
    last_interacted_.form = FormRef(form_util::GetFormByRendererId(form_id));
  } else {
    FieldRendererId field_id = absl::get<FieldRendererId>(element_id);
    CHECK(field_id);
    last_interacted_.formless_element =
        FieldRef(form_util::GetFormControlByRendererId(field_id));
  }
  last_interacted_.saved_state =
      unsafe_render_frame()
          ? form_util::ExtractFormData(
                unsafe_render_frame()->GetWebFrame()->GetDocument(),
                last_interacted_.form.GetForm(), agent_->field_data_manager(),
                agent_->GetCallTimerState(
                    CallTimerState::CallSite::kUpdateLastInteractedElement))
          : std::nullopt;
}

void FormTracker::ResetLastInteractedElements() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    last_interacted_ = {};
  } else {
    last_interacted_.form = {};
    last_interacted_.formless_element = {};
  }
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
  submission_triggering_events_ = {};
}

void FormTracker::ElementWasHiddenOrRemoved(mojom::SubmissionSource source) {
  FireInferredFormSubmission(source);
}

}  // namespace autofill
