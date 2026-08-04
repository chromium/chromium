// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_submission_tracker.h"

#include <optional>
#include <utility>
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
using mojom::SubmissionSource;

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
      std::to_underlying(SubmittedFormTypeBySource::kMaxValue) + 1 ==
          3 * (std::to_underlying(mojom::SubmissionSource::kMaxValue) + 2),
      "SubmittedFormTypeBySource should have three values for each value of "
      "SubmissionSource in addition to three `Total` values");

  using underlying_type = std::underlying_type_t<SubmittedFormTypeBySource>;
  underlying_type source_bucket = std::to_underlying(source) * 3;
  underlying_type total_bucket =
      std::to_underlying(SubmittedFormTypeBySource::kTotal_Null);
  underlying_type offset = std::to_underlying(type);
  base::UmaHistogramEnumeration(
      "Autofill.SubmissionDetection.SubmittedFormType",
      static_cast<SubmittedFormTypeBySource>(source_bucket + offset));
  base::UmaHistogramEnumeration(
      "Autofill.SubmissionDetection.SubmittedFormType",
      static_cast<SubmittedFormTypeBySource>(total_bucket + offset));
}

}  // namespace

FormSubmissionTracker::FormSubmissionTracker(
    content::RenderFrame* render_frame,
    AutofillAgent& autofill_agent,
    PasswordAutofillAgent* password_autofill_agent)
    : content::RenderFrameObserver(render_frame),
      blink::WebLocalFrameObserver(render_frame->GetWebFrame()),
      autofill_agent_(autofill_agent),
      password_autofill_agent_(password_autofill_agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
}

FormSubmissionTracker::~FormSubmissionTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormSubmissionTracker::AjaxSucceeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  submission_triggering_events_.xhr_succeeded = true;
  FireSubmissionIfFormDisappear(SubmissionSource::XHR_SUCCEEDED);
}

void FormSubmissionTracker::ElementDisappeared(
    const blink::WebElement& element) {
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
      last_interacted_.form_id != form_util::GetFormRendererId(element)) {
    return;
  }
  // If tracking a field, any disappearance other than that field is not
  // interesting.
  if (element.DynamicTo<WebFormControlElement>() &&
      last_interacted_.formless_element_id !=
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

void FormSubmissionTracker::TrackAutofilledElement(FieldRendererId field_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  const WebFormControlElement element =
      form_util::GetFormControlByRendererId(field_id);
  if (!element) {
    return;
  }
  if (blink::WebFormElement form_element = element.GetOwningFormForAutofill()) {
    UpdateLastInteractedElement(form_element);
  } else {
    UpdateLastInteractedElement(element);
  }
  submission_triggering_events_.tracked_element_autofilled = true;
  TrackElement(mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
}

void FormSubmissionTracker::TrackAutofilledElement(
    const base::flat_map<FieldRendererId, FormRendererId>&
        filled_fields_and_forms) {
  auto field_is_owned = [](const std::pair<FieldRendererId, FormRendererId>&
                               filled_field_and_form) {
    return !form_util::GetFormByRendererId(filled_field_and_form.second)
                .IsNull();
  };
  if (auto it = std::ranges::find_if(filled_fields_and_forms, field_is_owned);
      it != filled_fields_and_forms.end()) {
    const auto& [filled_field_id, filled_form_id] = *it;
    if (base::FeatureList::IsEnabled(
            features::kAutofillAcceptDomMutationAfterAutofillSubmission)) {
      TrackAutofilledElement(filled_field_id);
    } else if (WebFormElement form =
                   form_util::GetFormByRendererId(filled_form_id)) {
      UpdateLastInteractedElement(form);
    } else {
      NOTREACHED();
    }
  } else {
    for (const auto& [filled_field_id, filled_form_id] :
         filled_fields_and_forms) {
      if (base::FeatureList::IsEnabled(
              features::kAutofillAcceptDomMutationAfterAutofillSubmission)) {
        TrackAutofilledElement(filled_field_id);
      } else if (WebFormControlElement control_element =
                     form_util::GetFormControlByRendererId(filled_field_id)) {
        UpdateLastInteractedElement(control_element);
      } else {
        NOTREACHED();
      }
    }
  }
}

void FormSubmissionTracker::OnJavaScriptChangedValue(
    const WebFormControlElement& element) {
  // The provisionally saved form must be updated on JS changes. However, it
  // should not be changed to another form, so that only the user can set the
  // tracked form and not JS. This call here is meant to keep the tracked form
  // up to date with the form's most recent version.
  if (provisionally_saved_form() &&
      form_util::GetFormRendererId(element.GetOwningFormForAutofill()) ==
          last_interacted_.form_id) {
    // Ideally, we re-extract the form at this moment, but to avoid performance
    // regression, we just update what JS updated on the Blink side.
    std::vector<FormFieldData> fields =
        provisionally_saved_form()->ExtractFields();
    if (auto it =
            std::ranges::find(fields, form_util::GetFieldRendererId(element),
                              &FormFieldData::renderer_id);
        it != fields.end()) {
      it->set_value(element.Value().Utf16().substr(0, kMaxStringLength));
      it->set_is_autofilled_according_to_renderer(element.IsAutofilled());
      form_util::MaybeUpdateUserInput(*it,
                                      form_util::GetFieldRendererId(element),
                                      autofill_agent_->field_data_manager());
    }
    provisionally_saved_form()->set_fields(std::move(fields));
  }

  const auto input_element = element.DynamicTo<WebInputElement>();
  if (password_autofill_agent_ && input_element &&
      input_element.IsTextField() && !element.Value().IsEmpty() &&
      (input_element.FormControlTypeForAutofill() ==
           blink::mojom::FormControlType::kInputPassword ||
       password_autofill_agent_->IsUsernameInputField(input_element))) {
    password_autofill_agent_->UpdatePasswordStateForTextChange(
        input_element,
        /*form_cache=*/{});
  }
}

void FormSubmissionTracker::FormControlDidChange(
    const WebFormControlElement& element,
    ElementDidChangeCallback callback) {
  if (!unsafe_render_frame()) {
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  unsafe_render_frame()
      ->GetWebFrame()
      ->GetTaskRunner(blink::TaskType::kInternalAutofill)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&FormSubmissionTracker::FormControlDidChangeImpl,
                         weak_ptr_factory_.GetWeakPtr(),
                         form_util::GetFieldRendererId(element),
                         std::move(callback)));
}

void FormSubmissionTracker::FormControlDidChangeImpl(
    FieldRendererId element_id,
    ElementDidChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  WebFormControlElement element =
      form_util::GetFormControlByRendererId(element_id);
  // This function may be called asynchronously, so a navigation may have
  // happened. Since this event isn't submission-related.
  if (!form_util::IsOwnedByFrame(element, unsafe_render_frame())) {
    return;
  }
  WebFormElement form = element.GetOwningFormForAutofill();
  if (form) {
    UpdateLastInteractedElement(form);
  } else {
    UpdateLastInteractedElement(element);
  }
  std::move(callback).Run(
      element, SynchronousFormCache(form_util::GetFormRendererId(form),
                                    provisionally_saved_form()));
}

void FormSubmissionTracker::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormSubmissionTracker::DidFinishSameDocumentNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  submission_triggering_events_.finished_same_document_navigation = true;
  FireSubmissionIfFormDisappear(SubmissionSource::SAME_DOCUMENT_NAVIGATION);
}

void FormSubmissionTracker::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  if (!unsafe_render_frame() ||
      !unsafe_render_frame()->GetWebFrame()->IsOutermostMainFrame()) {
    // Ony handle primary main frame as iframe navigations rarely mean
    // user-triggered form submissions.
    return;
  }

  if (!navigation_type) {
    // We are interested only in content-initiated navigations. Explicit browser
    // initiated navigations (e.g. via omnibox) do not have a navigation type
    // and are discarded here.
    return;
  }

  switch (*navigation_type) {
    // Standard link navigations are excluded as they do not usually signify a
    // form submission.
    case blink::kWebNavigationTypeLinkClicked:
    // These types represent restoring, reloading, or traversing history (not
    // content-initiated navigations). Since the form state for these pages has
    // already been processed or is simply being replayed by the browser, no
    // submission is fired in order not to introduce noise signals.
    case blink::kWebNavigationTypeBackForward:
    case blink::kWebNavigationTypeReload:
    case blink::kWebNavigationTypeFormResubmittedBackForward:
    case blink::kWebNavigationTypeFormResubmittedReload:
    case blink::kWebNavigationTypeRestore:
    // A standard <form> submission. This should already be caught by either
    // `FormSubmissionTracker::WillSubmitForm()` or
    // `FormSubmissionTracker::WilSendSubmitEvent()`, so submission is not fired
    // here in order to avoid duplicate signals.
    case blink::kWebNavigationTypeFormSubmitted:
      return;
    // Catch-all for other types. This includes JavaScript-initiated navigations
    // (e.g., setting window.location) which can simulate a submission.
    case blink::kWebNavigationTypeOther:
      break;
  }

  FireFormSubmission(mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED,
                     /*submitted_form_element=*/std::nullopt);
}

void FormSubmissionTracker::WillDetach(blink::DetachReason detach_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
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
}

void FormSubmissionTracker::WillSendSubmitEvent(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  CHECK(form);
  // TODO(crbug.com/40281981): Figure out if this is still needed, and document
  // the reason, otherwise remove.
  UpdateLastInteractedElement(form);
  // TODO(crbug.com/40281981): Figure out if this is still needed, and
  // document the reason, otherwise remove.
  if (password_autofill_agent_) {
    password_autofill_agent_->InformBrowserAboutUserInput(
        form, WebInputElement(),
        SynchronousFormCache(form_util::GetFormRendererId(form),
                             provisionally_saved_form()));
  }
  // Fire the form submission event to avoid missing submissions where websites
  // cancel the onsubmit event. This also gets the form before Javascript's
  // submit event handler could change it. We don't clear submitted_forms_
  // because OnFormSubmitted will normally be invoked afterwards and we don't
  // want to fire the same event twice.
  FireFormSubmission(mojom::SubmissionSource::FORM_SUBMISSION, form);
}

void FormSubmissionTracker::WillSubmitForm(const WebFormElement& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
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

void FormSubmissionTracker::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
  ResetLastInteractedElements();
}

void FormSubmissionTracker::FireFormSubmission(
    SubmissionSource source,
    std::optional<WebFormElement> submitted_form_element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(form_submission_tracker_sequence_checker_);
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
    if (password_autofill_agent_) {
      password_autofill_agent_->FireHostSubmitEvent(
          FormRendererId(), /*submitted_form=*/std::nullopt, source);
    }
  }

  std::optional<FormData> form_data =
      GetSubmittedForm(source, submitted_form_element);

  if (form_data) {
    FireHostSubmitEvents(*form_data, source);
  }

  if (form_data) {
    switch (source) {
      // Resetting here would hurt PasswordManager submissions because it
      // ignores FORM_SUBMISSION.
      case mojom::SubmissionSource::FORM_SUBMISSION:
      // Resetting here would hurt Autofill submissions because it ignores
      // DOM_MUTATION_AFTER_AUTOFILL.
      case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
        break;
      // Resetting here would hurt PasswordManager submissions because it
      // ignores PROBABLY_FORM_SUBMITTED.
      case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
        // TODO(crbug.com/40281981): Figure out if this is still needed, and
        // document the reason, otherwise remove.
        OnFormNoLongerSubmittable();
        break;
      case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      case mojom::SubmissionSource::XHR_SUCCEEDED:
      case mojom::SubmissionSource::FRAME_DETACHED:
        // TODO(crbug.com/40281981): Figure out if this is still needed, and
        // document the reason, otherwise remove.
        ResetLastInteractedElements();
        OnFormNoLongerSubmittable();
        break;
      case mojom::SubmissionSource::NONE:
        NOTREACHED();
    }
  }
}

void FormSubmissionTracker::FireSubmissionIfFormDisappear(
    SubmissionSource source) {
  if (CanInferFormSubmitted() ||
      (submission_triggering_events_.tracked_element_disappeared &&
       base::FeatureList::IsEnabled(
           features::kAutofillReplaceFormElementObserver))) {
    FireFormSubmission(source, /*submitted_form_element=*/std::nullopt);
    return;
  }
  TrackElement(source);
}

bool FormSubmissionTracker::CanInferFormSubmitted() {
  if (last_interacted_.form_id) {
    WebFormElement last_interacted_form =
        form_util::GetFormByRendererId(last_interacted_.form_id);
    // Infer submission if the form was removed or all its elements are hidden.
    return !last_interacted_form ||
           std::ranges::none_of(
               last_interacted_form.GetFormControlElements(),  // nocheck
               &WebElement::IsFocusable);
  }
  if (last_interacted_.formless_element_id) {
    WebFormControlElement last_interacted_formless_element =
        form_util::GetFormControlByRendererId(
            last_interacted_.formless_element_id);
    // Infer submission if the field was removed or it's hidden.
    return !last_interacted_formless_element ||
           !last_interacted_formless_element.IsFocusable();
  }
  return false;
}

void FormSubmissionTracker::TrackElement(mojom::SubmissionSource source) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
    // Do not use WebFormElementObserver. Instead, rely on the signal
    // `FormSubmissionTracker::ElementDisappeared` coming from blink.
    return;
  }
  // Already has observer for last interacted element.
  if (form_element_observer_) {
    return;
  }
  auto callback =
      base::BindOnce(&FormSubmissionTracker::ElementWasHiddenOrRemoved,
                     base::Unretained(this), source);

  if (WebFormElement last_interacted_form =
          form_util::GetFormByRendererId(last_interacted_.form_id)) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_form, std::move(callback));
  } else if (WebFormControlElement last_interacted_formless_element =
                 form_util::GetFormControlByRendererId(
                     last_interacted_.formless_element_id)) {
    form_element_observer_ = blink::WebFormElementObserver::Create(
        last_interacted_formless_element, std::move(callback));
  }
}

void FormSubmissionTracker::FireHostSubmitEvents(
    const FormData& form_data,
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

  if (password_autofill_agent_) {
    // This checks whether another source, that s relevant for PasswordManager,
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

std::optional<FormData> FormSubmissionTracker::GetSubmittedForm(
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
      submitted_form_element.has_value()
          ? *submitted_form_element
          : form_util::GetFormByRendererId(last_interacted_.form_id),
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

void FormSubmissionTracker::UpdateLastInteractedElement(
    std::variant<WebFormElement, WebFormControlElement> element) {
  ResetLastInteractedElements();

  // `document` is the WebDocument of `element`'s element. It is not
  // necessarily the same as the current frame's document.
  //
  // `form_element` is null if `element` is a FieldRendererId.
  auto [document, form_element] = std::visit(
      absl::Overload{
          [this](WebFormElement form) {
            CHECK(form);
            last_interacted_.form_id = form_util::GetFormRendererId(form);
            return std::pair(form.GetDocument(), form);
          },
          [this](WebFormControlElement form_control) {
            CHECK(form_control);
            last_interacted_.formless_element_id =
                form_util::GetFieldRendererId(form_control);
            return std::pair(form_control.GetDocument(), WebFormElement());
          },
      },
      element);
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

void FormSubmissionTracker::ResetLastInteractedElements() {
  last_interacted_ = {};
  submission_triggering_events_ = {};
  if (form_element_observer_) {
    form_element_observer_->Disconnect();
    form_element_observer_ = nullptr;
  }
}

bool FormSubmissionTracker::IsTracking() const {
  return last_interacted_.form_id || last_interacted_.formless_element_id ||
         last_interacted_.saved_state;
}

void FormSubmissionTracker::ElementWasHiddenOrRemoved(
    mojom::SubmissionSource source) {
  FireFormSubmission(source, /*submitted_form_element=*/std::nullopt);
}

WebDocument FormSubmissionTracker::GetDocument() const {
  return unsafe_render_frame()
             ? unsafe_render_frame()->GetWebFrame()->GetDocument()
             : WebDocument();
}

}  // namespace autofill
