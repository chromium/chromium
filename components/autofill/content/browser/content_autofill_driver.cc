// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/bad_message.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
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
const std::vector<FormData>& WithNewVersion(
    const std::vector<FormData>& forms) {
  for (const FormData& form : forms) {
    WithNewVersion(form);
  }
  return forms;
}

}  // namespace

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    ContentAutofillDriverFactory* owner)
    : render_frame_host_(*render_frame_host),
      owner_(*owner),
      suppress_showing_ime_callback_(base::BindRepeating(
          [](const ContentAutofillDriver* driver) {
            return driver->should_suppress_keyboard_;
          },
          base::Unretained(this))) {
  render_frame_host_->GetRenderWidgetHost()->AddSuppressShowingImeCallback(
      suppress_showing_ime_callback_);
}

ContentAutofillDriver::~ContentAutofillDriver() {
  owner_->autofill_router().UnregisterDriver(this,
                                             /*driver_is_dying=*/true);
  render_frame_host_->GetRenderWidgetHost()->RemoveSuppressShowingImeCallback(
      suppress_showing_ime_callback_);
}

void ContentAutofillDriver::TriggerReparse() {
  GetAutofillAgent()->TriggerReparse();
}

void ContentAutofillDriver::TriggerReparseInAllFrames(
    base::OnceCallback<void(bool success)> trigger_reparse_finished_callback) {
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
                 trigger_reparse_finished_callback,
             const std::vector<bool>& successes) {
            std::move(trigger_reparse_finished_callback)
                .Run(base::ranges::all_of(successes, base::identity()));
          },
          std::move(trigger_reparse_finished_callback)));
  for (ContentAutofillDriver* driver : drivers) {
    driver->GetAutofillAgent()->TriggerReparseWithResponse(barrier_callback);
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

bool ContentAutofillDriver::IsInFencedFrameRoot() const {
  return render_frame_host_->IsFencedFrameRoot();
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

ui::AXTreeID ContentAutofillDriver::GetAxTreeId() const {
  return render_frame_host_->GetAXTreeID();
}

bool ContentAutofillDriver::RendererIsAvailable() {
  return render_frame_host_->GetRenderViewHost() != nullptr;
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

std::vector<FieldGlobalId> ContentAutofillDriver::FillOrPreviewForm(
    mojom::RendererFormDataAction action,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) {
  return autofill_router().FillOrPreviewForm(
      this, action, data, triggered_origin, field_type_map,
      [](ContentAutofillDriver* target, mojom::RendererFormDataAction action,
         const FormData& data) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->FillOrPreviewForm(data, action);
      });
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  // TODO(crbug.com/1185232) Send the FormDataPredictions object only if the
  // debugging flag is enabled.
  autofill_router().SendAutofillTypePredictionsToRenderer(
      this, type_predictions,
      [](ContentAutofillDriver* target,
         const std::vector<FormDataPredictions>& type_predictions) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->FieldTypePredictionsAvailable(
            type_predictions);
      });
}

void ContentAutofillDriver::SendFieldsEligibleForManualFillingToRenderer(
    const std::vector<FieldGlobalId>& fields) {
  autofill_router().SendFieldsEligibleForManualFillingToRenderer(
      this, fields,
      [](ContentAutofillDriver* target,
         const std::vector<FieldRendererId>& fields) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->SetFieldsEligibleForManualFilling(fields);
      });
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field,
    const std::u16string& value) {
  autofill_router().RendererShouldAcceptDataListSuggestion(
      this, field, value,
      [](ContentAutofillDriver* target, const FieldRendererId& field,
         const std::u16string& value) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->AcceptDataListSuggestion(field, value);
      });
}

void ContentAutofillDriver::RendererShouldClearFilledSection() {
  autofill_router().RendererShouldClearFilledSection(
      this, [](ContentAutofillDriver* target) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->ClearSection();
      });
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  autofill_router().RendererShouldClearPreviewedForm(
      this, [](ContentAutofillDriver* target) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->ClearPreviewedForm();
      });
}

void ContentAutofillDriver::RendererShouldFillFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {
  autofill_router().RendererShouldFillFieldWithValue(
      this, field, value,
      [](ContentAutofillDriver* target, const FieldRendererId& field,
         const std::u16string& value) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->FillFieldWithValue(field, value);
      });
}

void ContentAutofillDriver::RendererShouldPreviewFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {
  autofill_router().RendererShouldPreviewFieldWithValue(
      this, field, value,
      [](ContentAutofillDriver* target, const FieldRendererId& field,
         const std::u16string& value) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->PreviewFieldWithValue(field, value);
      });
}

void ContentAutofillDriver::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field,
    const mojom::AutofillState state) {
  autofill_router().RendererShouldSetSuggestionAvailability(
      this, field, state,
      [](ContentAutofillDriver* target, const FieldRendererId& field,
         const mojom::AutofillState state) {
        if (!target->RendererIsAvailable())
          return;
        target->GetAutofillAgent()->SetSuggestionAvailability(field, state);
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
  autofill_router().SetFormToBeProbablySubmitted(
      this,
      form ? absl::make_optional<FormData>(
                 GetFormWithFrameAndFormMetaData(*form))
           : absl::nullopt,
      [](ContentAutofillDriver* target, const FormData* optional_form) {
        target->potentially_submitted_form_ =
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

  autofill_router().FormsSeen(
      this, std::move(updated_forms), removed_forms,
      [](ContentAutofillDriver* target,
         const std::vector<FormData>& updated_forms,
         const std::vector<FormGlobalId>& removed_forms) {
        target->autofill_manager_->OnFormsSeen(WithNewVersion(updated_forms),
                                               removed_forms);
      });
}

void ContentAutofillDriver::FormSubmitted(
    const FormData& raw_form,
    bool known_success,
    mojom::SubmissionSource submission_source) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  autofill_router().FormSubmitted(
      this, GetFormWithFrameAndFormMetaData(raw_form), known_success,
      submission_source,
      [](ContentAutofillDriver* target, const FormData& form,
         bool known_success, mojom::SubmissionSource submission_source) {
        // Omit duplicate form submissions. It may be reasonable to take
        // |source| into account here as well.
        // TODO(crbug/1117451): Clean up experiment code.
        if (base::FeatureList::IsEnabled(
                features::kAutofillProbableFormSubmissionInBrowser) &&
            !base::FeatureList::IsEnabled(
                features::kAutofillAllowDuplicateFormSubmissions) &&
            !target->submitted_forms_.insert(form.global_id()).second) {
          return;
        }
        target->autofill_manager_->OnFormSubmitted(
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
  autofill_router().TextFieldDidChange(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box), timestamp,
      [](ContentAutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box,
         base::TimeTicks timestamp) {
        target->autofill_manager_->OnTextFieldDidChange(
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
  autofill_router().TextFieldDidScroll(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      [](ContentAutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box) {
        target->autofill_manager_->OnTextFieldDidScroll(WithNewVersion(form),
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
  autofill_router().SelectControlDidChange(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      [](ContentAutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box) {
        target->autofill_manager_->OnSelectControlDidChange(
            WithNewVersion(form), field, bounding_box);
      });
}

void ContentAutofillDriver::AskForValuesToFill(
    const FormData& raw_form,
    const FormFieldData& raw_field,
    const gfx::RectF& bounding_box,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    FormElementWasClicked form_element_was_clicked) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  FormData form = raw_form;
  FormFieldData field = raw_field;
  SetFrameAndFormMetaData(form, &field);
  autofill_router().AskForValuesToFill(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      autoselect_first_suggestion, form_element_was_clicked,
      [](ContentAutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box,
         AutoselectFirstSuggestion autoselect_first_suggestion,
         FormElementWasClicked form_element_was_clicked) {
        target->autofill_manager_->OnAskForValuesToFill(
            WithNewVersion(form), field, bounding_box,
            autoselect_first_suggestion, form_element_was_clicked);
      });
}

void ContentAutofillDriver::HidePopup() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  autofill_router().HidePopup(this, [](ContentAutofillDriver* target) {
    DCHECK(!target->IsPrerendering())
        << "We should never affect UI while prerendering";
    target->autofill_manager_->OnHidePopup();
  });
}

void ContentAutofillDriver::FocusNoLongerOnFormCallback(
    bool had_interacted_form) {
  autofill_manager_->OnFocusNoLongerOnForm(had_interacted_form);
}

void ContentAutofillDriver::FocusNoLongerOnForm(bool had_interacted_form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  autofill_router().FocusNoLongerOnForm(
      this, had_interacted_form,
      [](ContentAutofillDriver* target, bool had_interacted_form) {
        target->FocusNoLongerOnFormCallback(had_interacted_form);
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
  autofill_router().FocusOnFormField(
      this, std::move(form), field,
      TransformBoundingBoxToViewportCoordinates(bounding_box),
      [](ContentAutofillDriver* target, const FormData& form,
         const FormFieldData& field, const gfx::RectF& bounding_box) {
        target->autofill_manager_->OnFocusOnFormField(WithNewVersion(form),
                                                      field, bounding_box);
      });
}

void ContentAutofillDriver::DidFillAutofillFormData(const FormData& raw_form,
                                                    base::TimeTicks timestamp) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  autofill_router().DidFillAutofillFormData(
      this, GetFormWithFrameAndFormMetaData(raw_form), timestamp,
      [](ContentAutofillDriver* target, const FormData& form,
         base::TimeTicks timestamp) {
        target->autofill_manager_->OnDidFillAutofillFormData(
            WithNewVersion(form), timestamp);
      });
}

void ContentAutofillDriver::DidPreviewAutofillFormData() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  autofill_router().DidPreviewAutofillFormData(
      this, [](ContentAutofillDriver* target) {
        target->autofill_manager_->OnDidPreviewAutofillFormData();
      });
}

void ContentAutofillDriver::DidEndTextFieldEditing() {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  autofill_router().DidEndTextFieldEditing(
      this, [](ContentAutofillDriver* target) {
        target->autofill_manager_->OnDidEndTextFieldEditing();
      });
}

void ContentAutofillDriver::SelectFieldOptionsDidChange(
    const FormData& raw_form) {
  if (!bad_message::CheckFrameNotPrerendering(render_frame_host())) {
    return;
  }
  autofill_router().SelectFieldOptionsDidChange(
      this, GetFormWithFrameAndFormMetaData(raw_form),
      [](ContentAutofillDriver* target, const FormData& form) {
        target->autofill_manager_->OnSelectFieldOptionsDidChange(
            WithNewVersion(form));
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
  autofill_router().JavaScriptChangedAutofilledValue(
      this, std::move(form), field, old_value,
      [](ContentAutofillDriver* target, const FormData& form,
         const FormFieldData& field, const std::u16string& old_value) {
        target->autofill_manager_->OnJavaScriptChangedAutofilledValue(
            WithNewVersion(form), field, old_value);
      });
}

void ContentAutofillDriver::OnContextMenuShownInFieldCallback(
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id) {
  autofill_manager_->OnContextMenuShownInField(form_global_id, field_global_id);
}

void ContentAutofillDriver::OnContextMenuShownInField(
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id) {
  autofill_router().OnContextMenuShownInField(
      this, form_global_id, field_global_id,
      [](ContentAutofillDriver* target, const FormGlobalId& form_global_id,
         const FieldGlobalId& field_global_id) {
        target->OnContextMenuShownInFieldCallback(form_global_id,
                                                  field_global_id);
      });
}

void ContentAutofillDriver::Reset() {
  // The driver's RenderFrameHost may be used for the page we're navigating to.
  // Therefore, we need to forget all forms of the page we're navigating from.
  submitted_forms_.clear();
  owner_->autofill_router().UnregisterDriver(this,
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

void ContentAutofillDriver::UnsetKeyPressHandlerCallback() {
  if (key_press_handler_.is_null())
    return;
  render_frame_host_->GetRenderWidgetHost()->RemoveKeyPressEventCallback(
      key_press_handler_);
  key_press_handler_.Reset();
}

void ContentAutofillDriver::SetShouldSuppressKeyboardCallback(bool suppress) {
  should_suppress_keyboard_ = suppress;
}

void ContentAutofillDriver::SetKeyPressHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  autofill_router().SetKeyPressHandler(
      this, handler,
      [](ContentAutofillDriver* target,
         const content::RenderWidgetHost::KeyPressEventCallback& handler) {
        target->UnsetKeyPressHandlerCallback();
        target->render_frame_host_->GetRenderWidgetHost()
            ->AddKeyPressEventCallback(handler);
        target->key_press_handler_ = handler;
      });
}

void ContentAutofillDriver::UnsetKeyPressHandler() {
  autofill_router().UnsetKeyPressHandler(
      this, [](ContentAutofillDriver* target) {
        target->UnsetKeyPressHandlerCallback();
      });
}

void ContentAutofillDriver::SetShouldSuppressKeyboard(bool suppress) {
  autofill_router().SetShouldSuppressKeyboard(
      this, suppress, [](ContentAutofillDriver* target, bool suppress) {
        target->SetShouldSuppressKeyboardCallback(suppress);
      });
}

void ContentAutofillDriver::SetFrameAndFormMetaData(
    FormData& form,
    FormFieldData* optional_field) const {
  form.host_frame =
      LocalFrameToken(render_frame_host_->GetFrameToken().value());

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

ContentAutofillRouter& ContentAutofillDriver::autofill_router() {
  DCHECK(!IsPrerendering());
  return owner_->autofill_router();
}

}  // namespace autofill
