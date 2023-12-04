// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/barrier_callback.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/bad_message.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver_router.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "url/origin.h"

namespace autofill {

namespace {

// TODO(crbug.com/1117028): Remove once FormData objects aren't stored
// globally anymore.
const FormData& WithNewVersion(const FormData& form) {
  static FormVersion version_counter;
  ++*version_counter;
  const_cast<FormData&>(form).version = version_counter;
  return form;
}

// TODO(crbug.com/1117028): Remove once FormData objects aren't stored
// globally anymore.
const std::optional<FormData>& WithNewVersion(
    const std::optional<FormData>& form) {
  if (form) {
    WithNewVersion(*form);
  }
  return form;
}

// TODO(crbug.com/1117028): Remove once FormData objects aren't stored
// globally anymore.
const std::vector<FormData>& WithNewVersion(
    const std::vector<FormData>& forms) {
  for (const FormData& form : forms) {
    WithNewVersion(form);
  }
  return forms;
}

// ContentAutofillDriver::router() must only route among ContentAutofillDrivers.
// Therefore, we can safely cast AutofillDriverRouter's AutofillDrivers to
// ContentAutofillDrivers.
ContentAutofillDriver* cast(autofill::AutofillDriver* driver) {
  return static_cast<ContentAutofillDriver*>(driver);
}

}  // namespace

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    ContentAutofillDriverFactory* owner)
    : render_frame_host_(*render_frame_host), owner_(*owner) {}

ContentAutofillDriver::~ContentAutofillDriver() {
  owner_->router().UnregisterDriver(this,
                                    /*driver_is_dying=*/true);
}

void ContentAutofillDriver::TriggerFormExtractionInDriverFrame() {
  GetAutofillAgent()->TriggerFormExtraction();
}

void ContentAutofillDriver::TriggerFormExtractionInAllFrames(
    base::OnceCallback<void(bool success)> form_extraction_finished_callback) {
  std::vector<ContentAutofillDriver*> drivers;
  render_frame_host()->GetMainFrame()->ForEachRenderFrameHost(
      [&drivers](content::RenderFrameHost* rfh) {
        if (rfh->IsActive()) {
          if (auto* driver = GetForRenderFrameHost(rfh)) {
            drivers.push_back(driver);
          }
        }
      });
  auto barrier_callback = base::BarrierCallback<bool>(
      drivers.size(),
      base::BindOnce(
          [](base::OnceCallback<void(bool success)>
                 form_extraction_finished_callback,
             const std::vector<bool>& successes) {
            std::move(form_extraction_finished_callback)
                .Run(base::ranges::all_of(successes, std::identity()));
          },
          std::move(form_extraction_finished_callback)));
  for (ContentAutofillDriver* driver : drivers) {
    driver->GetAutofillAgent()->TriggerFormExtractionWithResponse(
        barrier_callback);
  }
}

void ContentAutofillDriver::GetFourDigitCombinationsFromDOM(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  GetAutofillAgent()->GetPotentialLastFourCombinationsForStandaloneCvc(
      std::move(potential_matches));
}

// static
ContentAutofillDriver* ContentAutofillDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->DriverForFrame(render_frame_host) : nullptr;
}

void ContentAutofillDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

LocalFrameToken ContentAutofillDriver::GetFrameToken() const {
  return LocalFrameToken(render_frame_host_->GetFrameToken().value());
}

ContentAutofillDriver* ContentAutofillDriver::GetParent() {
  content::RenderFrameHost* parent_rfh = render_frame_host_->GetParent();
  if (!parent_rfh) {
    return nullptr;
  }
  return owner_->DriverForFrame(parent_rfh);
}

AutofillManager& ContentAutofillDriver::GetAutofillManager() {
  return *autofill_manager_;
}

absl::optional<LocalFrameToken> ContentAutofillDriver::Resolve(
    FrameToken query) {
  if (absl::holds_alternative<LocalFrameToken>(query)) {
    return absl::get<LocalFrameToken>(query);
  }
  DCHECK(absl::holds_alternative<RemoteFrameToken>(query));
  content::RenderProcessHost* rph = render_frame_host_->GetProcess();
  blink::RemoteFrameToken blink_remote_token(
      absl::get<RemoteFrameToken>(query).value());
  content::RenderFrameHost* remote_rfh =
      content::RenderFrameHost::FromPlaceholderToken(rph->GetID(),
                                                     blink_remote_token);
  if (!remote_rfh) {
    return absl::nullopt;
  }
  return LocalFrameToken(remote_rfh->GetFrameToken().value());
}

bool ContentAutofillDriver::IsInActiveFrame() const {
  return render_frame_host_->IsActive();
}

bool ContentAutofillDriver::IsInAnyMainFrame() const {
  return render_frame_host_->GetMainFrame() == render_frame_host();
}

bool ContentAutofillDriver::IsPrerendering() const {
  return render_frame_host_->IsInLifecycleState(
      content::RenderFrameHost::LifecycleState::kPrerendering);
}

bool ContentAutofillDriver::HasSharedAutofillPermission() const {
  return render_frame_host_->IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature::kSharedAutofill);
}

bool ContentAutofillDriver::CanShowAutofillUi() const {
  // Don't show AutofillUi for inactive RenderFrameHost. Here it is safe to
  // ignore the calls from inactive RFH as the renderer is not expecting a reply
  // and it doesn't lead to browser-renderer consistency issues.
  return render_frame_host_->IsActive();
}

void ContentAutofillDriver::PopupHidden() {
  // If the unmask prompt is shown, keep showing the preview. The preview
  // will be cleared when the prompt closes.
  if (autofill_manager_->ShouldClearPreviewedForm()) {
    RendererShouldClearPreviewedForm();
  }
}

gfx::RectF ContentAutofillDriver::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) const {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return bounding_box;

  gfx::PointF orig_point(bounding_box.x(), bounding_box.y());
  gfx::PointF transformed_point =
      view->TransformPointToRootCoordSpaceF(orig_point);
  return gfx::RectF(transformed_point.x(), transformed_point.y(),
                    bounding_box.width(), bounding_box.height());
}

net::IsolationInfo ContentAutofillDriver::IsolationInfo() {
  return render_frame_host_->GetIsolationInfoForSubresources();
}

std::vector<FieldGlobalId> ContentAutofillDriver::ApplyFormAction(
    mojom::ActionType action_type,
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) {
  return router().ApplyFormAction(
      this, action_type, action_persistence, form, triggered_origin,
      field_type_map,
      [](autofill::AutofillDriver* target, mojom::ActionType action_type,
         mojom::ActionPersistence action_persistence, const FormData& form) {
        cast(target)->GetAutofillAgent()->ApplyFormAction(
            action_type, action_persistence, form);
      });
}

void ContentAutofillDriver::ApplyFieldAction(
    mojom::ActionPersistence action_persistence,
    mojom::TextReplacement text_replacement,
    const FieldGlobalId& field,
    const std::u16string& value) {
  router().ApplyFieldAction(
      this, action_persistence, text_replacement, field, value,
      [](autofill::AutofillDriver* target,
         mojom::ActionPersistence action_persistence,
         mojom::TextReplacement text_replacement, const FieldRendererId& field,
         const std::u16string& value) {
        cast(target)->GetAutofillAgent()->ApplyFieldAction(
            action_persistence, text_replacement, field, value);
      });
}

void ContentAutofillDriver::ExtractForm(FormGlobalId form_id,
                                        BrowserFormHandler final_handler) {
  // Caution: nested callbacks ahead.
  //
  // There are three drivers involved:
  //   `this` --> `request_target` == `response_source` --> `response_target`.
  // The hops between these drivers happen in AutofillDriverRouter, which
  // continues the control flow through synchronous callbacks. The response of
  // the renderer is handled by an asynchronous callback.
  //
  // The chronological order of events is this:
  //
  // (1) AutofillDriverRouter::ExtractForm() finds the right AutofillDriver.
  // (2) make_request() sends the request to the right AutofillAgent.
  // (3) set_meta_data() sets the meta data upon receival of the renderer form.
  // (4) flatten_form() converts the received renderer form to a browser form.
  // (5) set_version() increments FormData::version.
  // (6) final_handler() processes the browser form.
  //
  // Step (N+1) is called by Step (N).
  // Step (3) set_meta_data() is the only asynchronous call.
  //
  // These steps occur in reverse order in the code below due to the callbacks.
  // Reading it from the bottom up is recommended.
  //
  // Perhaps most confusing is that flatten_form() becomes available in (2) but
  // is called only in (3). The reason is that
  // - we must set the meta data before flattening the form, and
  // - since the response to make_request() is asynchronous, the only way to
  //   squeeze that in is through a separate callback.
  //
  // TODO(crbug.com/1331312): Make ExtractForm() trigger a FormsSeen() event and
  // await that event in the browser process, instead of having the response
  // callback.

  auto set_version = base::BindOnce(
      [](BrowserFormHandler final_handler,
         autofill::AutofillDriver* response_target,
         const std::optional<FormData>& browser_form) {
        // (6) Process the browser form (with meta data and version).
        std::move(final_handler)
            .Run(response_target,
                 // (5) Increment the form's version as very last step.
                 WithNewVersion(browser_form));
      },
      std::move(final_handler));

  using RendererFormHandler =
      base::OnceCallback<void(const absl::optional<::autofill::FormData>&)>;
  // Called on the autofill driver that hosts the form with `form_id`.
  auto make_request = [](autofill::AutofillDriver* request_target,
                         const FormRendererId& form_id,
                         RendererFormHandler flatten_form) {
    // Called asynchronously. Binding `response_target` is safe because
    // destroying `*response_target` also destroys any pending callback.
    auto set_meta_data = base::BindOnce(
        [](RendererFormHandler flatten_form,
           raw_ref<ContentAutofillDriver> response_target,
           const std::optional<FormData>& raw_form) {
          // (4) Convert the received renderer form to a browser form.
          std::move(flatten_form)
              .Run(
                  // (3) Set the meta data on receival of the renderer form.
                  response_target->GetFormWithFrameAndFormMetaData(raw_form));
        },
        std::move(flatten_form), raw_ref(*cast(request_target)));
    // (2) Send the mojo message to the right AutofillAgent.
    cast(request_target)
        ->GetAutofillAgent()
        ->ExtractForm(form_id, std::move(set_meta_data));
  };

  // (1) Route the request to the right AutofillDriver.
  router().ExtractForm(this, form_id, std::move(set_version), make_request);
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  // TODO(crbug.com/1185232) Send the FormDataPredictions object only if the
  // debugging flag is enabled.
  router().SendAutofillTypePredictionsToRenderer(
      this, type_predictions,
      [](autofill::AutofillDriver* target,
         const std::vector<FormDataPredictions>& type_predictions) {
        cast(target)->GetAutofillAgent()->FieldTypePredictionsAvailable(
            type_predictions);
      });
}

void ContentAutofillDriver::SendFieldsEligibleForManualFillingToRenderer(
    const std::vector<FieldGlobalId>& fields) {
  router().SendFieldsEligibleForManualFillingToRenderer(
      this, fields,
      [](autofill::AutofillDriver* target,
         const std::vector<FieldRendererId>& fields) {
        cast(target)->GetAutofillAgent()->SetFieldsEligibleForManualFilling(
            fields);
      });
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field,
    const std::u16string& value) {
  router().RendererShouldAcceptDataListSuggestion(
      this, field, value,
      [](autofill::AutofillDriver* target, const FieldRendererId& field,
         const std::u16string& value) {
        cast(target)->GetAutofillAgent()->AcceptDataListSuggestion(field,
                                                                   value);
      });
}

void ContentAutofillDriver::RendererShouldClearFilledSection() {
  router().RendererShouldClearFilledSection(
      this, [](autofill::AutofillDriver* target) {
        cast(target)->GetAutofillAgent()->ClearSection();
      });
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  router().RendererShouldClearPreviewedForm(
      this, [](autofill::AutofillDriver* target) {
        cast(target)->GetAutofillAgent()->ClearPreviewedForm();
      });
}

void ContentAutofillDriver::RendererShouldTriggerSuggestions(
    const FieldGlobalId& field,
    AutofillSuggestionTriggerSource trigger_source) {
  router().RendererShouldTriggerSuggestions(
      this, field, trigger_source,
      [](autofill::AutofillDriver* target, const FieldRendererId& field,
         AutofillSuggestionTriggerSource trigger_source) {
        cast(target)->GetAutofillAgent()->TriggerSuggestions(field,
                                                             trigger_source);
      });
}

void ContentAutofillDriver::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field,
    mojom::AutofillSuggestionAvailability suggestion_availability) {
  router().RendererShouldSetSuggestionAvailability(
      this, field, suggestion_availability,
      [](autofill::AutofillDriver* target, const FieldRendererId& field,
         mojom::AutofillSuggestionAvailability suggestion_availability) {
        cast(target)->GetAutofillAgent()->SetSuggestionAvailability(
            field, suggestion_availability);
      });
}

void ContentAutofillDriver::ProbablyFormSubmitted(
    base::PassKey<ContentAutofillDriverFactory>) {
  // TODO(crbug.com/1117451): This currently misbehaves in frame-transcending
  // forms: SetFormToBeProbablySubmitted() is routed, but this event is not.
  // We should probably direct the event to the top-most frame, perhaps to the
  // top-most frame that has a `potentially_submitted_form_`.
  if (potentially_submitted_form_) {
    FormSubmitted(*potentially_submitted_form_, false,
                  mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
}

void ContentAutofillDriver::SetFormToBeProbablySubmitted(
    const absl::optional<FormData>& form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  router().SetFormToBeProbablySubmitted(
      this, GetFormWithFrameAndFormMetaData(form),
      [](autofill::AutofillDriver* target, const FormData* optional_form) {
        cast(target)->potentially_submitted_form_ =
            base::OptionalFromPtr(optional_form);
      });
}

void ContentAutofillDriver::FormsSeen(
    const std::vector<FormData>& raw_updated_forms,
    const std::vector<FormRendererId>& raw_removed_forms) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  std::vector<FormData> updated_forms = raw_updated_forms;
  for (FormData& form : updated_forms)
    SetFrameAndFormMetaData(form, nullptr);

  std::vector<FormGlobalId> removed_forms;
  removed_forms.reserve(raw_removed_forms.size());
  for (FormRendererId form_id : raw_removed_forms)
    removed_forms.push_back({GetFrameToken(), form_id});

  router().FormsSeen(this, std::move(updated_forms), removed_forms,
                     [](autofill::AutofillDriver* target,
                        const std::vector<FormData>& updated_forms,
                        const std::vector<FormGlobalId>& removed_forms) {
                       target->GetAutofillManager().OnFormsSeen(
                           WithNewVersion(updated_forms), removed_forms);
                     });
}

void ContentAutofillDriver::FormSubmitted(
    const FormData& raw_form,
    bool known_success,
    mojom::SubmissionSource submission_source) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  router().FormSubmitted(
      this, GetFormWithFrameAndFormMetaData(raw_form), known_success,
      submission_source,
      [](autofill::AutofillDriver* target, const FormData& form,
         bool known_success, mojom::SubmissionSource submission_source) {
        // Omit duplicate form submissions. It may be reasonable to take
        // |source| into account here as well.
        // TODO(crbug/1117451): Clean up experiment code.
        if (base::FeatureList::IsEnabled(
                features::kAutofillProbableFormSubmissionInBrowser) &&
            !base::FeatureList::IsEnabled(
                features::kAutofillAllowDuplicateFormSubmissions) &&
            !cast(target)->submitted_forms_.insert(form.global_id()).second) {
          return;
        }
        target->GetAutofillManager().OnFormSubmitted(
            WithNewVersion(form), known_success, submission_source);
      });
}

void ContentAutofillDriver::TextFieldDidChange(const FormData& raw_form,
                                               const FormFieldData& raw_field,
                                               const gfx::RectF& bounding_box,
                                               base::TimeTicks timestamp) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  router().TextFieldDidChange(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box), timestamp,
      [](autofill::AutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box,
         base::TimeTicks timestamp) {
        target->GetAutofillManager().OnTextFieldDidChange(
            WithNewVersion(form), field, bounding_box, timestamp);
      });
}

void ContentAutofillDriver::TextFieldDidScroll(const FormData& raw_form,
                                               const FormFieldData& raw_field,
                                               const gfx::RectF& bounding_box) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  router().TextFieldDidScroll(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      [](autofill::AutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box) {
        target->GetAutofillManager().OnTextFieldDidScroll(WithNewVersion(form),
                                                          field, bounding_box);
      });
}

void ContentAutofillDriver::SelectControlDidChange(
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const gfx::RectF& bounding_box) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  router().SelectControlDidChange(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      [](autofill::AutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box) {
        target->GetAutofillManager().OnSelectControlDidChange(
            WithNewVersion(form), field, bounding_box);
      });
}

void ContentAutofillDriver::AskForValuesToFill(
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const gfx::RectF& bounding_box,
    AutofillSuggestionTriggerSource trigger_source) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  router().AskForValuesToFill(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box), trigger_source,
      [](autofill::AutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box,
         AutofillSuggestionTriggerSource trigger_source) {
        target->GetAutofillManager().OnAskForValuesToFill(
            WithNewVersion(form), field, bounding_box, trigger_source);
      });
}

void ContentAutofillDriver::HidePopup() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  router().HidePopup(this, [](autofill::AutofillDriver* target) {
    DCHECK(!target->IsPrerendering())
        << "We should never affect UI while prerendering";
    target->GetAutofillManager().OnHidePopup();
  });
}

void ContentAutofillDriver::FocusNoLongerOnForm(bool had_interacted_form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  router().FocusNoLongerOnForm(
      this, had_interacted_form,
      [](autofill::AutofillDriver* target, bool had_interacted_form) {
        target->GetAutofillManager().OnFocusNoLongerOnForm(had_interacted_form);
      });
}

void ContentAutofillDriver::FocusOnFormField(const FormData& raw_form,
                                             const FormFieldData& raw_field,
                                             const gfx::RectF& bounding_box) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  router().FocusOnFormField(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      [](autofill::AutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box) {
        target->GetAutofillManager().OnFocusOnFormField(WithNewVersion(form),
                                                        field, bounding_box);
      },
      [](autofill::AutofillDriver* target) {
        target->GetAutofillManager().OnFocusNoLongerOnForm(true);
      });
}

void ContentAutofillDriver::DidFillAutofillFormData(const FormData& raw_form,
                                                    base::TimeTicks timestamp) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  router().DidFillAutofillFormData(
      this, GetFormWithFrameAndFormMetaData(raw_form), timestamp,
      [](autofill::AutofillDriver* target, const FormData& form,
         base::TimeTicks timestamp) {
        target->GetAutofillManager().OnDidFillAutofillFormData(
            WithNewVersion(form), timestamp);
      });
}

void ContentAutofillDriver::DidEndTextFieldEditing() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  router().DidEndTextFieldEditing(this, [](autofill::AutofillDriver* target) {
    target->GetAutofillManager().OnDidEndTextFieldEditing();
  });
}

void ContentAutofillDriver::SelectOrSelectListFieldOptionsDidChange(
    const FormData& raw_form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  router().SelectOrSelectListFieldOptionsDidChange(
      this, GetFormWithFrameAndFormMetaData(raw_form),
      [](autofill::AutofillDriver* target, const FormData& form) {
        cast(target)
            ->GetAutofillManager()
            .OnSelectOrSelectListFieldOptionsDidChange(WithNewVersion(form));
      });
}

void ContentAutofillDriver::JavaScriptChangedAutofilledValue(
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const std::u16string& old_value) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  router().JavaScriptChangedAutofilledValue(
      this, std::move(form), field, old_value,
      [](autofill::AutofillDriver* target, const FormData& form,
         const FormFieldData& field, const std::u16string& old_value) {
        target->GetAutofillManager().OnJavaScriptChangedAutofilledValue(
            WithNewVersion(form), field, old_value);
      });
}

void ContentAutofillDriver::OnContextMenuShownInField(
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id) {
  router().OnContextMenuShownInField(
      this, form_global_id, field_global_id,
      [](autofill::AutofillDriver* target, const FormGlobalId& form_global_id,
         const FieldGlobalId& field_global_id) {
        target->GetAutofillManager().OnContextMenuShownInField(form_global_id,
                                                               field_global_id);
      });
}

void ContentAutofillDriver::Reset() {
  // The driver's RenderFrameHost may be used for the page we're navigating to.
  // Therefore, we need to forget all forms of the page we're navigating from.
  submitted_forms_.clear();
  owner_->router().UnregisterDriver(this,
                                    /*driver_is_dying=*/false);
  autofill_manager_->Reset();
}

const mojo::AssociatedRemote<mojom::AutofillAgent>&
ContentAutofillDriver::GetAutofillAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_agent_);
  }
  return autofill_agent_;
}

void ContentAutofillDriver::SetFrameAndFormMetaData(
    FormData& form,
    FormFieldData* optional_field) const {
  form.host_frame = GetFrameToken();

  // GetLastCommittedURL doesn't include URL updates due to document.open() and
  // so it might be about:blank or about:srcdoc. In this case fallback to
  // GetLastCommittedOrigin. See http://crbug.com/1209270 for more details.
  GURL url = render_frame_host_->GetLastCommittedURL();
  if (url.SchemeIs(url::kAboutScheme))
    url = render_frame_host_->GetLastCommittedOrigin().GetURL();
  form.url = StripAuthAndParams(url);

  if (auto* main_rfh = render_frame_host_->GetMainFrame())
    form.main_frame_origin = main_rfh->GetLastCommittedOrigin();
  else
    form.main_frame_origin = url::Origin();

  // The form signature must not be calculated before setting FormData::url.
  FormSignature form_signature = CalculateFormSignature(form);

  auto SetFieldMetaData = [&](FormFieldData& field) {
    field.host_frame = form.host_frame;
    field.host_form_id = form.unique_renderer_id;
    field.origin = render_frame_host_->GetLastCommittedOrigin();
    field.host_form_signature = form_signature;
    field.bounds = TransformBoundingBoxToViewportCoordinates(field.bounds);
  };

  for (FormFieldData& field : form.fields)
    SetFieldMetaData(field);
  if (optional_field)
    SetFieldMetaData(*optional_field);
}

FormData ContentAutofillDriver::GetFormWithFrameAndFormMetaData(
    FormData form) const {
  SetFrameAndFormMetaData(form, nullptr);
  return form;
}

std::optional<FormData> ContentAutofillDriver::GetFormWithFrameAndFormMetaData(
    base::optional_ref<const FormData> form) const {
  if (!form.has_value()) {
    return std::nullopt;
  }
  return GetFormWithFrameAndFormMetaData(*form);
}

AutofillDriverRouter& ContentAutofillDriver::router() {
  DCHECK(!IsPrerendering());
  return owner_->router();
}

}  // namespace autofill
