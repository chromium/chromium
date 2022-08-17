// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stddef.h>

#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/cxx20_erase_set.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_assistant_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "net/cert/cert_status_flags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebAutofillClient;
using blink::WebAutofillState;
using blink::WebAXObject;
using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

using form_util::ExtractMask;
using form_util::FindFormAndFieldForFormControlElement;
using form_util::IsOwnedByFrame;
using mojom::SubmissionSource;
using ShowAll = PasswordAutofillAgent::ShowAll;
using GenerationShowing = PasswordAutofillAgent::GenerationShowing;

namespace {

// Time to wait in ms to ensure that only a single select or datalist change
// will be acted upon, instead of multiple in close succession (debounce time).
size_t kWaitTimeForOptionsChangesMs = 50;

// Helper function to return EXTRACT_DATALIST if kAutofillExtractAllDatalist is
// enabled, otherwise EXTRACT_NONE is returned.
ExtractMask GetExtractDatalistMask() {
  return base::FeatureList::IsEnabled(features::kAutofillExtractAllDatalists)
             ? form_util::EXTRACT_DATALIST
             : form_util::EXTRACT_NONE;
}

}  // namespace

// During prerendering, we do not want the renderer to send messages to the
// corresponding driver. Since we use a channel associated interface, we still
// need to set up the mojo connection as before (i.e., we can't defer binding
// the interface). Instead, we enqueue our messages here as post-activation
// tasks. See post-prerendering activation steps here:
// https://wicg.github.io/nav-speculation/prerendering.html#prerendering-bcs-subsection
class AutofillAgent::DeferringAutofillDriver : public mojom::AutofillDriver {
 public:
  explicit DeferringAutofillDriver(AutofillAgent* agent) : agent_(agent) {}
  ~DeferringAutofillDriver() override = default;

 private:
  template <typename F, typename... Args>
  void SendMsg(F fn, Args&&... args) {
    DCHECK(!agent_->IsPrerendering());
    mojom::AutofillDriver& autofill_driver = agent_->GetAutofillDriver();
    DCHECK_NE(&autofill_driver, this);
    (autofill_driver.*fn)(std::forward<Args>(args)...);
  }
  template <typename F, typename... Args>
  void DeferMsg(F fn, Args... args) {
    DCHECK(agent_->IsPrerendering());
    agent_->render_frame()
        ->GetWebFrame()
        ->GetDocument()
        .AddPostPrerenderingActivationStep(base::BindOnce(
            &DeferringAutofillDriver::SendMsg<F, Args...>,
            weak_ptr_factory_.GetWeakPtr(), fn, std::forward<Args>(args)...));
  }
  void SetFormToBeProbablySubmitted(
      const absl::optional<FormData>& form) override {
    DeferMsg(&mojom::AutofillDriver::SetFormToBeProbablySubmitted, form);
  }
  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override {
    DeferMsg(&mojom::AutofillDriver::FormsSeen, updated_forms, removed_forms);
  }
  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource source) override {
    DeferMsg(&mojom::AutofillDriver::FormSubmitted, form, known_success,
             source);
  }
  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldDidChange, form, field,
             bounding_box, timestamp);
  }
  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldDidScroll, form, field,
             bounding_box);
  }
  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override {
    DeferMsg(&mojom::AutofillDriver::SelectControlDidChange, form, field,
             bounding_box);
  }
  void SelectFieldOptionsDidChange(const FormData& form) override {
    DeferMsg(&mojom::AutofillDriver::SelectFieldOptionsDidChange, form);
  }
  void AskForValuesToFill(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          int32_t query_id,
                          bool autoselect_first_suggestion,
                          TouchToFillEligible touch_to_fill_eligible) override {
    DeferMsg(&mojom::AutofillDriver::AskForValuesToFill, form, field,
             bounding_box, query_id, autoselect_first_suggestion,
             touch_to_fill_eligible);
  }
  void HidePopup() override { DeferMsg(&mojom::AutofillDriver::HidePopup); }
  void FocusNoLongerOnForm(bool had_interacted_form) override {
    DeferMsg(&mojom::AutofillDriver::FocusNoLongerOnForm, had_interacted_form);
  }
  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override {
    DeferMsg(&mojom::AutofillDriver::FocusOnFormField, form, field,
             bounding_box);
  }
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {
    DeferMsg(&mojom::AutofillDriver::DidFillAutofillFormData, form, timestamp);
  }
  void DidPreviewAutofillFormData() override {
    DeferMsg(&mojom::AutofillDriver::DidPreviewAutofillFormData);
  }
  void DidEndTextFieldEditing() override {
    DeferMsg(&mojom::AutofillDriver::DidEndTextFieldEditing);
  }
  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override {
    DeferMsg(&mojom::AutofillDriver::JavaScriptChangedAutofilledValue, form,
             field, old_value);
  }

  AutofillAgent* agent_ = nullptr;
  base::WeakPtrFactory<DeferringAutofillDriver> weak_ptr_factory_{this};
};

AutofillAgent::AutofillAgent(content::RenderFrame* render_frame,
                             PasswordAutofillAgent* password_autofill_agent,
                             PasswordGenerationAgent* password_generation_agent,
                             AutofillAssistantAgent* autofill_assistant_agent,
                             blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      form_cache_(render_frame->GetWebFrame()),
      password_autofill_agent_(password_autofill_agent),
      password_generation_agent_(password_generation_agent),
      autofill_assistant_agent_(autofill_assistant_agent),
      autofill_query_id_(0),
      query_node_autofill_state_(WebAutofillState::kNotFilled),
      is_popup_possibly_visible_(false),
      is_generation_popup_possibly_visible_(false),
      is_user_gesture_required_(true),
      is_secure_context_required_(false),
      form_tracker_(render_frame),
      field_data_manager_(password_autofill_agent->GetFieldDataManager()) {
  render_frame->GetWebFrame()->SetAutofillClient(this);
  password_autofill_agent->SetAutofillAgent(this);
  AddFormObserver(this);
  registry->AddInterface<mojom::AutofillAgent>(base::BindRepeating(
      &AutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

// The destructor is not guaranteed to be called. Destruction happens (only)
// through the OnDestruct() event, which posts a task to delete this object.
// The process may be killed before this deletion can happen.
AutofillAgent::~AutofillAgent() {
  RemoveFormObserver(this);
}

void AutofillAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void AutofillAgent::DidCommitProvisionalLoad(ui::PageTransition transition) {
  // Navigation to a new page or a page refresh.
  element_.Reset();

  form_cache_.Reset();
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::DidDispatchDOMContentLoadedEvent() {
  ProcessForms();
}

void AutofillAgent::DidChangeScrollOffset() {
  if (element_.IsNull())
    return;

  if (!focus_requires_scroll_) {
    // Post a task here since scroll offset may change during layout.
    // (https://crbug.com/804886)
    weak_ptr_factory_.InvalidateWeakPtrs();
    render_frame()
        ->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&AutofillAgent::DidChangeScrollOffsetImpl,
                                  weak_ptr_factory_.GetWeakPtr(), element_));
  } else {
    HidePopup();
  }
}

void AutofillAgent::DidChangeScrollOffsetImpl(
    const WebFormControlElement& element) {
  if (element != element_ || element.IsNull() || focus_requires_scroll_ ||
      !is_popup_possibly_visible_ || !element.Focused()) {
    return;
  }

  DCHECK(IsOwnedByFrame(element, render_frame()));

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    GetAutofillDriver().TextFieldDidScroll(form, field, field.bounds);
  }

  // Ignore subsequent scroll offset changes.
  HidePopup();
}

void AutofillAgent::FocusedElementChanged(const WebElement& element) {
  HidePopup();

  if (element.IsNull()) {
    // Focus moved away from the last interacted form (if any) to somewhere else
    // on the page.
    GetAutofillDriver().FocusNoLongerOnForm(!last_interacted_form_.IsNull());
    return;
  }

  const WebFormControlElement form_control_element =
      element.DynamicTo<WebFormControlElement>();

  bool focus_moved_to_new_form = false;
  if (!last_interacted_form_.IsNull() &&
      (form_control_element.IsNull() ||
       last_interacted_form_ != form_control_element.Form())) {
    // The focused element is not part of the last interacted form (could be
    // in a different form).
    GetAutofillDriver().FocusNoLongerOnForm(/*had_interacted_form=*/true);
    focus_moved_to_new_form = true;
  }

  // Calls HandleFocusChangeComplete() after notifying the focus is no longer on
  // the previous form, then early return. No need to notify the newly focused
  // element because that will be done by HandleFocusChangeComplete() which
  // triggers FormControlElementClicked().
  // Refer to http://crbug.com/1105254
  if ((IsKeyboardAccessoryEnabled() || !focus_requires_scroll_) &&
      !element.IsNull() &&
      element.GetDocument().GetFrame()->HasTransientUserActivation()) {
    focused_node_was_last_clicked_ = true;
    HandleFocusChangeComplete();
  }

  if (focus_moved_to_new_form)
    return;

  if (form_control_element.IsNull() || !form_control_element.IsEnabled() ||
      form_control_element.IsReadOnly() ||
      !form_util::IsTextAreaElementOrTextInput(form_control_element)) {
    return;
  }

  element_ = form_control_element;

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element_, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    GetAutofillDriver().FocusOnFormField(form, field, field.bounds);
  }
}

void AutofillAgent::OnDestruct() {
  Shutdown();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void AutofillAgent::AccessibilityModeChanged(const ui::AXMode& mode) {
  is_screen_reader_enabled_ = mode.has_mode(ui::AXMode::kScreenReader);
}

void AutofillAgent::FireHostSubmitEvents(const WebFormElement& form,
                                         bool known_success,
                                         SubmissionSource source) {
  DCHECK(IsOwnedByFrame(form, render_frame()));

  FormData form_data;
  if (!form_util::ExtractFormData(form, *field_data_manager_.get(), &form_data))
    return;

  FireHostSubmitEvents(form_data, known_success, source);
}

void AutofillAgent::FireHostSubmitEvents(const FormData& form_data,
                                         bool known_success,
                                         SubmissionSource source) {
  // We don't want to fire duplicate submission event.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAllowDuplicateFormSubmissions) &&
      !submitted_forms_.insert(form_data.unique_renderer_id).second) {
    return;
  }

  GetAutofillDriver().FormSubmitted(form_data, known_success, source);
}

void AutofillAgent::Shutdown() {
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AutofillAgent::TextFieldDidEndEditing(const WebInputElement& element) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  // Sometimes "blur" events are side effects of the password generation
  // handling the page. They should not affect any UI in the browser.
  if (password_generation_agent_ &&
      password_generation_agent_->ShouldIgnoreBlur()) {
    return;
  }
  GetAutofillDriver().DidEndTextFieldEditing();
  password_autofill_agent_->DidEndTextFieldEditing();
  if (password_generation_agent_)
    password_generation_agent_->DidEndTextFieldEditing(element);

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::SetUserGestureRequired(bool required) {
  form_tracker_.set_user_gesture_required(required);
}

void AutofillAgent::TextFieldDidChange(const WebFormControlElement& element) {
  form_tracker_.TextFieldDidChange(element);
}

void AutofillAgent::OnTextFieldDidChange(const WebInputElement& element) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  if (password_generation_agent_ &&
      password_generation_agent_->TextDidChangeInTextField(element)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (password_autofill_agent_->TextDidChangeInTextField(element)) {
    is_popup_possibly_visible_ = true;
    element_ = element;
    return;
  }

  ShowSuggestions(element, {.requires_caret_at_end = true});

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    GetAutofillDriver().TextFieldDidChange(form, field, field.bounds,
                                           AutofillTickClock::NowTicks());
  }
}

void AutofillAgent::TextFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  if (event.windows_key_code == ui::VKEY_DOWN ||
      event.windows_key_code == ui::VKEY_UP) {
    ShowSuggestions(element,
                    {.autofill_on_empty_values = true,
                     .requires_caret_at_end = true,
                     .autoselect_first_suggestion =
                         ShouldAutoselectFirstSuggestionOnArrowDown()});
  }
}

void AutofillAgent::OpenTextDataListChooser(const WebInputElement& element) {
  DCHECK(IsOwnedByFrame(element, render_frame()));
  ShowSuggestions(element, {.autofill_on_empty_values = true});
}

// Notifies the AutofillDriver about changes in the <datalist> options in
// batches.
//
// A batch ends if no event occurred for `kWaitTimeForOptionsChangesMs`
// milliseconds. For a given batch, the AutofillDriver is informed only about
// the last field. That is, if within one batch the options of different
// fields changed, all but one of these events will be lost.
void AutofillAgent::DataListOptionsChanged(const WebInputElement& element) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  if (element.GetDocument().IsNull() || !is_popup_possibly_visible_ ||
      !element.Focused()) {
    return;
  }

  if (datalist_option_change_batch_timer_.IsRunning())
    datalist_option_change_batch_timer_.AbandonAndStop();

  datalist_option_change_batch_timer_.Start(
      FROM_HERE, base::Milliseconds(kWaitTimeForOptionsChangesMs),
      base::BindRepeating(&AutofillAgent::BatchDataListOptionChange,
                          weak_ptr_factory_.GetWeakPtr(), element));
}

void AutofillAgent::BatchDataListOptionChange(
    const blink::WebFormControlElement& element) {
  if (element.GetDocument().IsNull())
    return;

  OnProvisionallySaveForm(element.Form(), element,
                          ElementChangeSource::TEXTFIELD_CHANGED);
}

void AutofillAgent::UserGestureObserved() {
  password_autofill_agent_->UserGestureObserved();
}

void AutofillAgent::TriggerRefillIfNeeded(const FormData& form) {
  ReplaceElementIfNowInvalid(form);

  FormFieldData field;
  FormData updated_form;
  if (FindFormAndFieldForFormControlElement(element_, field_data_manager_.get(),
                                            &updated_form, &field) &&
      (!element_.IsAutofilled() || !form.DynamicallySameFormAs(updated_form))) {
    WebLocalFrame* frame = render_frame()->GetWebFrame();
    std::vector<FormData> forms;
    forms.push_back(updated_form);
    // Always communicate to browser process for the outermost main frame.
    if (!forms.empty() || frame->IsOutermostMainFrame()) {
      GetAutofillDriver().FormsSeen(forms, {});
    }
  }
}

// mojom::AutofillAgent:
void AutofillAgent::FillOrPreviewForm(int32_t query_id,
                                      const FormData& form,
                                      mojom::RendererFormDataAction action) {
  // If |element_| is null or not focused, a Autofill was triggered from another
  // frame. In this case, set |element_| to some form field as if Autofill had
  // been triggered from that field. This is necessary because currently
  // AutofillAgent's relies on |elemet_| in many places.
  if (query_id == kCrossFrameFill && !form.fields.empty() &&
      (element_.IsNull() || !element_.Focused())) {
    WebDocument document = render_frame()->GetWebFrame()->GetDocument();
    element_ = form_util::FindFormControlElementByUniqueRendererId(
        document, form.fields.front().unique_renderer_id);
  }

  if (element_.IsNull())
    return;

  if (query_id != autofill_query_id_ && query_id != kCrossFrameFill &&
      (action == mojom::RendererFormDataAction::kPreview ||
       query_id != kNoQueryId)) {
    return;
  }

  if (action == mojom::RendererFormDataAction::kPreview) {
    ClearPreviewedForm();

    query_node_autofill_state_ = element_.GetAutofillState();
    previewed_elements_ = form_util::FillOrPreviewForm(form, element_, action);

    GetAutofillDriver().DidPreviewAutofillFormData();
  } else {
    was_last_action_fill_ = true;

    // If this is a re-fill, replace the triggering element if it's invalid.
    if (query_id == kNoQueryId)
      ReplaceElementIfNowInvalid(form);

    query_node_autofill_state_ = element_.GetAutofillState();
    bool filled_some_fields =
        !form_util::FillOrPreviewForm(form, element_, action).empty();

    if (!element_.Form().IsNull()) {
      UpdateLastInteractedForm(element_.Form());
    } else {
      formless_elements_were_autofilled_ |= filled_some_fields;
    }

    // TODO(crbug.com/1198811): Inform the BrowserAutofillManager about the
    // fields that were actually filled. It's possible that the form has changed
    // since the time filling was triggered.
    GetAutofillDriver().DidFillAutofillFormData(form,
                                                AutofillTickClock::NowTicks());

    TriggerRefillIfNeeded(form);
    SendPotentiallySubmittedFormToBrowser();
  }
}

void AutofillAgent::FieldTypePredictionsAvailable(
    const std::vector<FormDataPredictions>& forms) {
  bool attach_predictions_to_dom =
      base::FeatureList::IsEnabled(features::kAutofillShowTypePredictions);
  for (const auto& form : forms) {
    form_cache_.ShowPredictions(form, attach_predictions_to_dom);
  }
}

void AutofillAgent::ClearSection() {
  if (element_.IsNull())
    return;

  form_cache_.ClearSectionWithElement(element_);
}

void AutofillAgent::ClearPreviewedForm() {
  // TODO(crbug.com/816533): It is very rare, but it looks like the |element_|
  // can be null if a provisional load was committed immediately prior to
  // clearing the previewed form.
  if (element_.IsNull())
    return;

  if (password_autofill_agent_->DidClearAutofillSelection(element_))
    return;

  form_util::ClearPreviewedElements(previewed_elements_, element_,
                                    query_node_autofill_state_);
  previewed_elements_ = {};
}

void AutofillAgent::FillFieldWithValue(FieldRendererId field_id,
                                       const std::u16string& value) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  if (form_util::IsTextAreaElementOrTextInput(element_))
    DoFillFieldWithValue(value, element_, WebAutofillState::kAutofilled);
}

void AutofillAgent::PreviewFieldWithValue(FieldRendererId field_id,
                                          const std::u16string& value) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  WebInputElement input_element = element_.DynamicTo<WebInputElement>();
  if (!input_element.IsNull())
    DoPreviewFieldWithValue(value, input_element);
}

void AutofillAgent::SetSuggestionAvailability(
    FieldRendererId field_id,
    const mojom::AutofillState state) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  WebInputElement input_element = element_.DynamicTo<WebInputElement>();
  if (!input_element.IsNull()) {
    switch (state) {
      case mojom::AutofillState::kAutofillAvailable:
        WebAXObject::FromWebNode(input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kAutofillAvailable);
        return;
      case mojom::AutofillState::kAutocompleteAvailable:
        WebAXObject::FromWebNode(input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kAutocompleteAvailable);
        return;
      case mojom::AutofillState::kNoSuggestions:
        WebAXObject::FromWebNode(input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kNoSuggestions);
        return;
    }
    NOTREACHED();
  }
}

void AutofillAgent::AcceptDataListSuggestion(
    FieldRendererId field_id,
    const std::u16string& suggested_value) {
  if (element_.IsNull() ||
      field_id != FieldRendererId(element_.UniqueRendererFormControlId())) {
    return;
  }

  WebInputElement input_element = element_.DynamicTo<WebInputElement>();
  if (input_element.IsNull()) {
    // Early return for non-input fields such as textarea.
    return;
  }
  std::u16string new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (input_element.IsMultiple() && input_element.IsEmailField()) {
    std::u16string value = input_element.EditingValue().Utf16();
    std::vector<base::StringPiece16> parts = base::SplitStringPiece(
        value, u",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() == 0)
      parts.push_back(base::StringPiece16());

    std::u16string last_part(parts.back());
    // We want to keep just the leading whitespace.
    for (size_t i = 0; i < last_part.size(); ++i) {
      if (!base::IsUnicodeWhitespace(last_part[i])) {
        last_part = last_part.substr(0, i);
        break;
      }
    }
    last_part.append(suggested_value);
    parts.back() = last_part;

    new_value = base::JoinString(parts, u",");
  }
  DoFillFieldWithValue(new_value, element_, WebAutofillState::kNotFilled);
}

void AutofillAgent::FillPasswordSuggestion(const std::u16string& username,
                                           const std::u16string& password) {
  if (element_.IsNull())
    return;

  bool handled =
      password_autofill_agent_->FillSuggestion(element_, username, password);
  DCHECK(handled);
}

void AutofillAgent::PreviewPasswordSuggestion(const std::u16string& username,
                                              const std::u16string& password) {
  if (element_.IsNull())
    return;

  bool handled = password_autofill_agent_->PreviewSuggestion(
      element_, blink::WebString::FromUTF16(username),
      blink::WebString::FromUTF16(password));
  DCHECK(handled);
}

bool AutofillAgent::CollectFormlessElements(FormData* output) const {
  if (render_frame() == nullptr || render_frame()->GetWebFrame() == nullptr)
    return false;

  WebDocument document = render_frame()->GetWebFrame()->GetDocument();

  // Build up the FormData from the unowned elements. This logic mostly
  // mirrors the construction of the synthetic form in form_cache.cc, but
  // happens at submit-time so we can capture the modifications the user
  // has made, and doesn't depend on form_cache's internal state.
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document, &fieldsets);

  std::vector<WebElement> iframe_elements =
      form_util::GetUnownedIframeElements(document);

  const ExtractMask extract_mask = static_cast<ExtractMask>(
      form_util::EXTRACT_VALUE | form_util::EXTRACT_OPTIONS);

  return form_util::UnownedFormElementsAndFieldSetsToFormData(
      fieldsets, control_elements, iframe_elements, nullptr, document,
      field_data_manager_.get(), extract_mask, output, nullptr);
}

void AutofillAgent::ShowSuggestions(const WebFormControlElement& element,
                                    const ShowSuggestionsOptions& options) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  if (!element.IsEnabled() || element.IsReadOnly())
    return;
  if (!element.SuggestedValue().IsEmpty())
    return;

  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  if (!input_element.IsNull()) {
    if (!input_element.IsTextField())
      return;
    if (!input_element.SuggestedValue().IsEmpty())
      return;
  } else {
    DCHECK(form_util::IsTextAreaElement(element));
    if (!element.To<WebFormControlElement>().SuggestedValue().IsEmpty())
      return;
  }

  // Don't attempt to autofill with values that are too large or if filling
  // criteria are not met.
  WebString value = element.EditingValue();
  if (value.length() > kMaxDataLength ||
      (!options.autofill_on_empty_values && value.IsEmpty()) ||
      (options.requires_caret_at_end &&
       (element.SelectionStart() != element.SelectionEnd() ||
        element.SelectionEnd() != static_cast<int>(value.length())))) {
    // Any popup currently showing is obsolete.
    HidePopup();
    return;
  }

  element_ = element;
  if (form_util::IsAutofillableInputElement(input_element) &&
      password_autofill_agent_->ShowSuggestions(
          input_element, ShowAll(options.show_full_suggestion_list),
          GenerationShowing(is_generation_popup_possibly_visible_))) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (is_generation_popup_possibly_visible_)
    return;

  // Password field elements should only have suggestions shown by the password
  // autofill agent.
  // The /*disable presubmit*/ comment below is used to disable a presubmit
  // script that ensures that only IsPasswordFieldForAutofill() is used in this
  // code (it has to appear between the function name and the parentesis to not
  // match a regex). In this specific case we are actually interested in whether
  // the field is currently a password field, not whether it has ever been a
  // password field.
  if (!input_element.IsNull() &&
      input_element.IsPasswordField /*disable presubmit*/ () &&
      !query_password_suggestion_) {
    return;
  }

  QueryAutofillSuggestions(element, options.autoselect_first_suggestion,
                           options.touch_to_fill_eligible);
}

void AutofillAgent::SetQueryPasswordSuggestion(bool query) {
  query_password_suggestion_ = query;
}

void AutofillAgent::SetSecureContextRequired(bool required) {
  is_secure_context_required_ = required;
}

void AutofillAgent::SetFocusRequiresScroll(bool require) {
  focus_requires_scroll_ = require;
}

void AutofillAgent::GetElementFormAndFieldDataForDevToolsNodeId(
    const int backend_node_id,
    GetElementFormAndFieldDataForDevToolsNodeIdCallback callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  blink::WebElement target_element =
      frame->GetDocument().GetElementByDevToolsNodeId(backend_node_id);

  FormData form;
  FormFieldData field;

  if (target_element.IsNull() || !target_element.IsFormControlElement()) {
    return std::move(callback).Run(form, field);
  }

  blink::WebFormControlElement target_form_control_element =
      target_element.To<blink::WebFormControlElement>();
  bool success = FindFormAndFieldForFormControlElement(
      target_form_control_element, field_data_manager_.get(), &form, &field);
  if (success) {
    // Remember this element so as to autofill the form without focusing the
    // field for Autofill Assistant.
    element_ = target_form_control_element;
  }
  // Do not expect failure.
  DCHECK(success);

  return std::move(callback).Run(form, field);
}

void AutofillAgent::SetAssistantKeyboardSuppressState(bool suppress) {
  DCHECK(autofill_assistant_agent_);
  if (suppress) {
    autofill_assistant_agent_->DisableKeyboard();
  } else {
    autofill_assistant_agent_->EnableKeyboard();
  }
}

void AutofillAgent::EnableHeavyFormDataScraping() {
  is_heavy_form_data_scraping_enabled_ = true;
}

void AutofillAgent::SetFieldsEligibleForManualFilling(
    const std::vector<FieldRendererId>& fields) {
  form_cache_.SetFieldsEligibleForManualFilling(fields);
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element,
    bool autoselect_first_suggestion,
    TouchToFillEligible touch_to_fill_eligible) {
  blink::WebLocalFrame* frame = element.GetDocument().GetFrame();
  if (!frame)
    return;

  DCHECK(!element.DynamicTo<WebInputElement>().IsNull() ||
         form_util::IsTextAreaElement(element));

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  FormData form;
  FormFieldData field;
  if (!FindFormAndFieldForFormControlElement(
          element, field_data_manager_.get(),
          static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                   GetExtractDatalistMask()),
          &form, &field)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(
        form.unique_renderer_id, element, nullptr,
        static_cast<ExtractMask>(form_util::EXTRACT_VALUE |
                                 form_util::EXTRACT_BOUNDS |
                                 GetExtractDatalistMask()),
        &field);
  }

  if (is_secure_context_required_ &&
      !(element.GetDocument().IsSecureContext())) {
    LOG(WARNING) << "Autofill suggestions are disabled because the document "
                    "isn't a secure context.";
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kAutofillExtractAllDatalists)) {
    const WebInputElement input_element = element.DynamicTo<WebInputElement>();
    if (!input_element.IsNull()) {
      // Find the datalist values and send them to the browser process.
      form_util::GetDataListSuggestions(input_element, &field.datalist_values,
                                        &field.datalist_labels);
    }
  }

  is_popup_possibly_visible_ = true;
  GetAutofillDriver().AskForValuesToFill(
      form, field, field.bounds, autofill_query_id_,
      autoselect_first_suggestion, touch_to_fill_eligible);
}

void AutofillAgent::DoFillFieldWithValue(const std::u16string& value,
                                         blink::WebFormControlElement& element,
                                         WebAutofillState autofill_state) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  form_tracker_.set_ignore_control_changes(true);

  element.SetAutofillValue(blink::WebString::FromUTF16(value), autofill_state);

  WebInputElement input_element = element.DynamicTo<WebInputElement>();
  // `input_element` can be null for textarea elements.
  if (!input_element.IsNull())
    password_autofill_agent_->UpdateStateForTextChange(input_element);

  form_tracker_.set_ignore_control_changes(false);
}

void AutofillAgent::DoPreviewFieldWithValue(const std::u16string& value,
                                            WebInputElement& node) {
  DCHECK(IsOwnedByFrame(node, render_frame()));

  ClearPreviewedForm();
  query_node_autofill_state_ = element_.GetAutofillState();
  node.SetSuggestedValue(blink::WebString::FromUTF16(value));
  form_util::PreviewSuggestion(node.SuggestedValue().Utf16(),
                               node.Value().Utf16(), &node);
  previewed_elements_.push_back(node);
}

void AutofillAgent::TriggerReparse() {
  if (!reparse_timer_.IsRunning()) {
    reparse_timer_.Start(FROM_HERE, base::Milliseconds(100),
                         base::BindRepeating(&AutofillAgent::ProcessForms,
                                             weak_ptr_factory_.GetWeakPtr()));
  }
}

void AutofillAgent::ProcessForms() {
  FormCache::UpdateFormCacheResult cache =
      form_cache_.UpdateFormCache(field_data_manager_.get());

  // Always communicate to browser process for the outermost main frame.
  if (!cache.updated_forms.empty() || !cache.removed_forms.empty() ||
      render_frame()->GetWebFrame()->IsOutermostMainFrame()) {
    GetAutofillDriver().FormsSeen(cache.updated_forms,
                                  std::move(cache.removed_forms).extract());
  }
}

void AutofillAgent::HidePopup() {
  if (!is_popup_possibly_visible_)
    return;
  is_popup_possibly_visible_ = false;
  is_generation_popup_possibly_visible_ = false;

  // The keyboard accessory has a separate, more complex hiding logic.
  if (IsKeyboardAccessoryEnabled())
    return;

  GetAutofillDriver().HidePopup();
}

void AutofillAgent::DidAssociateFormControlsDynamically() {
  // If the control flow is here than the document was at least loaded. The
  // whole page doesn't have to be loaded.
  ProcessForms();
  password_autofill_agent_->OnDynamicFormsSeen();
}

void AutofillAgent::DidCompleteFocusChangeInFrame() {
  WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  WebElement focused_element;
  if (!doc.IsNull())
    focused_element = doc.FocusedElement();

  if (!focused_element.IsNull() && password_autofill_agent_)
    password_autofill_agent_->FocusedNodeHasChanged(focused_element);

  // PasswordGenerationAgent needs to know about focus changes, even if there is
  // no focused element.
  if (password_generation_agent_ &&
      password_generation_agent_->FocusedNodeHasChanged(focused_element)) {
    is_generation_popup_possibly_visible_ = true;
    is_popup_possibly_visible_ = true;
  }

  if (!IsKeyboardAccessoryEnabled() && focus_requires_scroll_)
    HandleFocusChangeComplete();

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::DidReceiveLeftMouseDownOrGestureTapInNode(
    const WebNode& node) {
  DCHECK(!node.IsNull());
  focused_node_was_last_clicked_ = node.Focused();

#if defined(ANDROID)
  HandleFocusChangeComplete();
#else
  if (!focus_requires_scroll_) {
    HandleFocusChangeComplete();
  }
#endif
}

void AutofillAgent::SelectControlDidChange(
    const WebFormControlElement& element) {
  form_tracker_.SelectControlDidChange(element);
}

// Notifies the AutofillDriver about changes in the <select> options in batches.
//
// A batch ends if no event occurred for `kWaitTimeForOptionsChangesMs`
// milliseconds. For a given batch, the AutofillDriver is informed only about
// the last FormData. That is, if within one batch the options of different
// forms changed, all but one of these events will be lost.
void AutofillAgent::SelectFieldOptionsChanged(
    const blink::WebFormControlElement& element) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  if (!was_last_action_fill_ || element_.IsNull())
    return;

  if (select_option_change_batch_timer_.IsRunning())
    select_option_change_batch_timer_.AbandonAndStop();

  select_option_change_batch_timer_.Start(
      FROM_HERE, base::Milliseconds(kWaitTimeForOptionsChangesMs),
      base::BindRepeating(&AutofillAgent::BatchSelectOptionChange,
                          weak_ptr_factory_.GetWeakPtr(), element));
}

void AutofillAgent::BatchSelectOptionChange(
    const blink::WebFormControlElement& element) {
  if (element.GetDocument().IsNull())
    return;

  // Look for the form and field associated with the select element. If they are
  // found, notify the driver that the form was modified dynamically.
  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(element, field_data_manager_.get(),
                                            &form, &field) &&
      !field.options.empty()) {
    GetAutofillDriver().SelectFieldOptionsDidChange(form);
  }
}

bool AutofillAgent::ShouldSuppressKeyboard(
    const WebFormControlElement& element) {
  // Note: Consider supporting other autofill types in the future as well.
#if BUILDFLAG(IS_ANDROID)
  if (password_autofill_agent_->ShouldSuppressKeyboard())
    return true;
#endif
  return autofill_assistant_agent_ &&
         autofill_assistant_agent_->ShouldSuppressKeyboard();
}

void AutofillAgent::FormElementReset(const WebFormElement& form) {
  DCHECK(IsOwnedByFrame(form, render_frame()));

  password_autofill_agent_->InformAboutFormClearing(form);
}

void AutofillAgent::PasswordFieldReset(const WebInputElement& element) {
  DCHECK(IsOwnedByFrame(element, render_frame()));

  password_autofill_agent_->InformAboutFieldClearing(element);
}

bool AutofillAgent::IsPrerendering() const {
  return blink::features::IsPrerender2Enabled() &&
         render_frame()->GetWebFrame()->GetDocument().IsPrerendering();
}

void AutofillAgent::FormControlElementClicked(
    const WebFormControlElement& element) {
  last_clicked_form_control_element_for_testing_ =
      FieldRendererId(element.UniqueRendererFormControlId());
  was_last_action_fill_ = false;

  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  if (input_element.IsNull() && !form_util::IsTextAreaElement(element))
    return;

#if BUILDFLAG(IS_ANDROID)
  password_autofill_agent_->TryToShowTouchToFill(element);
#endif

  ShowSuggestions(
      element, {.autofill_on_empty_values = true,
                // Even if the user has not edited an input element, it may
                // still contain a value: A default value filled by the website.
                // In that case, we don't want to elide suggestions that don't
                // have a common prefix with the default value.
                .show_full_suggestion_list =
                    element.IsAutofilled() || !element.UserHasEditedTheField(),
                .touch_to_fill_eligible = TouchToFillEligible(true)});

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::HandleFocusChangeComplete() {
  WebElement focused_element =
      render_frame()->GetWebFrame()->GetDocument().FocusedElement();
  // When using Talkback on Android, and possibly others, traversing to and
  // focusing a field will not register as a click. Thus, when screen readers
  // are used, treat the focused node as if it was the last clicked. Also check
  // to ensure focus is on a field where text can be entered.
  if ((focused_node_was_last_clicked_ || is_screen_reader_enabled_) &&
      !focused_element.IsNull() && focused_element.IsFormControlElement()) {
    WebFormControlElement focused_form_control_element =
        focused_element.To<WebFormControlElement>();
    if (form_util::IsTextAreaElementOrTextInput(focused_form_control_element))
      FormControlElementClicked(focused_form_control_element);
  }

  focused_node_was_last_clicked_ = false;

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::AjaxSucceeded() {
  form_tracker_.AjaxSucceeded();

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::JavaScriptChangedAutofilledValue(
    const blink::WebFormControlElement& element,
    const blink::WebString& old_value) {
  if (old_value == element.Value())
    return;
  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(element, field_data_manager_.get(),
                                            &form, &field)) {
    GetAutofillDriver().JavaScriptChangedAutofilledValue(form, field,
                                                         old_value.Utf16());
  }
}

void AutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form,
    const WebFormControlElement& element,
    ElementChangeSource source) {
  if (source == ElementChangeSource::WILL_SEND_SUBMIT_EVENT) {
    // Fire the form submission event to avoid missing submission when web site
    // handles the onsubmit event, this also gets the form before Javascript
    // could change it.
    // We don't clear submitted_forms_ because OnFormSubmitted will normally be
    // invoked afterwards and we don't want to fire the same event twice.
    FireHostSubmitEvents(form, /*known_success=*/false,
                         SubmissionSource::FORM_SUBMISSION);
    ResetLastInteractedElements();
  } else if (source == ElementChangeSource::TEXTFIELD_CHANGED ||
             source == ElementChangeSource::SELECT_CHANGED) {
    // Remember the last form the user interacted with.
    if (!element.Form().IsNull()) {
      UpdateLastInteractedForm(element.Form());
    } else {
      // Remove visible elements.
      WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
      if (!doc.IsNull()) {
        base::EraseIf(
            formless_elements_user_edited_,
            [&doc](const FieldRendererId field_id) {
              WebFormControlElement field =
                  form_util::FindFormControlElementByUniqueRendererId(
                      doc, field_id, /*form_to_be_searched =*/FormRendererId());
              return !field.IsNull() && form_util::IsWebElementFocusable(field);
            });
      }
      formless_elements_user_edited_.insert(
          FieldRendererId(element.UniqueRendererFormControlId()));
      provisionally_saved_form_ = absl::make_optional<FormData>();
      if (!CollectFormlessElements(&provisionally_saved_form_.value())) {
        provisionally_saved_form_.reset();
      } else {
        last_interacted_form_.Reset();
      }
    }

    if (source == ElementChangeSource::TEXTFIELD_CHANGED) {
      OnTextFieldDidChange(element.To<WebInputElement>());
    } else {
      FormData form_data;
      FormFieldData field;
      if (FindFormAndFieldForFormControlElement(
              element, field_data_manager_.get(),
              static_cast<ExtractMask>(form_util::EXTRACT_BOUNDS |
                                       GetExtractDatalistMask()),
              &form_data, &field)) {
        GetAutofillDriver().SelectControlDidChange(form_data, field,
                                                   field.bounds);
      }
    }
  }
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnProbablyFormSubmitted() {
  absl::optional<FormData> form_data = GetSubmittedForm();
  if (form_data.has_value()) {
    FireHostSubmitEvents(form_data.value(), /*known_success=*/false,
                         SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  DCHECK(IsOwnedByFrame(form, render_frame()));

  // Fire the submission event here because WILL_SEND_SUBMIT_EVENT is skipped
  // if javascript calls submit() directly.
  FireHostSubmitEvents(form, /*known_success=*/false,
                       SubmissionSource::FORM_SUBMISSION);
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnInferredFormSubmission(SubmissionSource source) {
  if (source == SubmissionSource::FRAME_DETACHED &&
      render_frame()->GetWebFrame()->IsOutermostMainFrame()) {
    // No op.
  } else if (source == SubmissionSource::SAME_DOCUMENT_NAVIGATION &&
             !render_frame()->GetWebFrame()->IsOutermostMainFrame()) {
    // No op.
  } else if (source == SubmissionSource::FRAME_DETACHED) {
    // Should not access the frame because it is now detached. Instead, use
    // |provisionally_saved_form_|.
    if (provisionally_saved_form_.has_value())
      FireHostSubmitEvents(provisionally_saved_form_.value(),
                           /*known_success=*/true, source);
  } else {
    absl::optional<FormData> form_data = GetSubmittedForm();
    if (form_data.has_value())
      FireHostSubmitEvents(form_data.value(), /*known_success=*/true, source);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::AddFormObserver(Observer* observer) {
  form_tracker_.AddObserver(observer);
}

void AutofillAgent::RemoveFormObserver(Observer* observer) {
  form_tracker_.RemoveObserver(observer);
}

void AutofillAgent::TrackAutofilledElement(
    const blink::WebFormControlElement& element) {
  form_tracker_.TrackAutofilledElement(element);
}

absl::optional<FormData> AutofillAgent::GetSubmittedForm() const {
  if (!last_interacted_form_.IsNull()) {
    FormData form;
    if (form_util::ExtractFormData(last_interacted_form_,
                                   *field_data_manager_.get(), &form)) {
      return absl::make_optional(form);
    } else if (provisionally_saved_form_.has_value()) {
      return absl::make_optional(provisionally_saved_form_.value());
    }
  } else if (formless_elements_were_autofilled_ ||
             (formless_elements_user_edited_.size() != 0 &&
              !form_util::IsSomeControlElementVisible(
                  render_frame()->GetWebFrame(),
                  formless_elements_user_edited_))) {
    // we check if all the elements the user has interacted with are gone,
    // to decide if submission has occurred, and use the
    // provisionally_saved_form_ saved in OnProvisionallySaveForm() if fail to
    // construct form.
    FormData form;
    if (CollectFormlessElements(&form)) {
      return absl::make_optional(form);
    } else if (provisionally_saved_form_.has_value()) {
      return absl::make_optional(provisionally_saved_form_.value());
    }
  }
  return absl::nullopt;
}

void AutofillAgent::SendPotentiallySubmittedFormToBrowser() {
  GetAutofillDriver().SetFormToBeProbablySubmitted(GetSubmittedForm());
}

void AutofillAgent::ResetLastInteractedElements() {
  last_interacted_form_.Reset();
  last_clicked_form_control_element_for_testing_ = {};
  formless_elements_user_edited_.clear();
  formless_elements_were_autofilled_ = false;
  provisionally_saved_form_.reset();
}

void AutofillAgent::UpdateLastInteractedForm(
    const blink::WebFormElement& form) {
  DCHECK(IsOwnedByFrame(form, render_frame()));

  last_interacted_form_ = form;
  provisionally_saved_form_ = absl::make_optional<FormData>();
  if (!form_util::ExtractFormData(last_interacted_form_,
                                  *field_data_manager_.get(),
                                  &provisionally_saved_form_.value())) {
    provisionally_saved_form_.reset();
  }
}

void AutofillAgent::OnFormNoLongerSubmittable() {
  submitted_forms_.clear();
}

bool AutofillAgent::FindTheUniqueNewVersionOfOldElement(
    const WebVector<WebFormControlElement>& elements,
    bool& potential_match_encountered,
    WebFormControlElement& matching_element,
    const WebFormControlElement& original_element) {
  if (original_element.IsNull())
    return false;

  const auto original_element_section = original_element.AutofillSection();
  for (const WebFormControlElement& current_element : elements) {
    if (current_element.IsFocusable() &&
        original_element.NameForAutofill() ==
            current_element.NameForAutofill()) {
      // If this is the first matching element, or is the first with the right
      // section, this is the best match so far.
      // In other words: bad, then good. => pick good.
      if (!potential_match_encountered ||
          (current_element.AutofillSection() == original_element_section &&
           (matching_element.IsNull() ||
            matching_element.AutofillSection() != original_element_section))) {
        matching_element = current_element;
        potential_match_encountered = true;
      } else if (current_element.AutofillSection() !=
                     original_element_section &&
                 !matching_element.IsNull() &&
                 matching_element.AutofillSection() !=
                     original_element_section) {
        // The so far matching fields are equally bad. Continue the search if
        // none of them have the correct section.
        // In other words: bad, then bad => pick none.
        matching_element.Reset();
      } else if (current_element.AutofillSection() ==
                     original_element_section &&
                 !matching_element.IsNull() &&
                 matching_element.AutofillSection() ==
                     original_element_section) {
        // If two or more fields have the matching name and section, we can't
        // decide. Two equally good fields => fail.
        matching_element.Reset();
        return false;
      }  // For the good, then bad case => keep good. Continue the search.
    }
  }
  return true;
}

// TODO(crbug.com/896689): Update this method to use the unique ids once they
// are implemented.
void AutofillAgent::ReplaceElementIfNowInvalid(const FormData& original_form) {
  // If the document is invalid, bail out.
  if (element_.GetDocument().IsNull())
    return;

  const auto original_element = element_;
  WebFormControlElement matching_element;
  bool potential_match_encountered = false;

  if (original_form.name.empty()) {
    // If the form has no name, check all the forms.
    for (const WebFormElement& form : element_.GetDocument().Forms()) {
      // If finding a unique element is impossible, don't look further.
      if (!FindTheUniqueNewVersionOfOldElement(
              form.GetFormControlElements(), potential_match_encountered,
              matching_element, original_element))
        return;
    }
    // If the element is not found, we should still check for unowned elements.
    if (!matching_element.IsNull()) {
      element_ = matching_element;
      return;
    }
  }

  // If |element_|'s parent form has no elements, |element_| is now invalid
  // and should be updated.
  if (!element_.Form().IsNull() &&
      element_.Form().GetFormControlElements().empty()) {
    return;
  }

  WebFormElement form_element;
  bool form_is_found = false;
  if (!original_form.name.empty()) {
    // Try to find the new version of the form.
    for (const WebFormElement& form : element_.GetDocument().Forms()) {
      if (original_form.name == form.GetName().Utf16() ||
          original_form.name == form.GetAttribute("id").Utf16()) {
        if (!form_is_found)
          form_element = form;
        else  // multiple forms with the matching name.
          return;
      }
    }
  }

  if (form_element.IsNull()) {
    // Could not find the new version of the form, get all the unowned elements.
    std::vector<WebElement> fieldsets;
    WebVector<WebFormControlElement> elements =
        form_util::GetUnownedAutofillableFormFieldElements(
            element_.GetDocument(), &fieldsets);
    // If a unique match was found.
    if (FindTheUniqueNewVersionOfOldElement(
            elements, potential_match_encountered, matching_element,
            original_element) &&
        !matching_element.IsNull()) {
      element_ = matching_element;
    }
    return;
  }
  // This is the case for owned fields that belong to the right named form.
  // Get all the elements of the new version of the form.
  // If a unique match was found.
  if (FindTheUniqueNewVersionOfOldElement(form_element.GetFormControlElements(),
                                          potential_match_encountered,
                                          matching_element, original_element) &&
      !matching_element.IsNull()) {
    element_ = matching_element;
  }
}

mojom::AutofillDriver& AutofillAgent::GetAutofillDriver() {
  if (IsPrerendering()) {
    if (!deferring_autofill_driver_) {
      deferring_autofill_driver_ =
          std::make_unique<DeferringAutofillDriver>(this);
    }
    return *deferring_autofill_driver_;
  }

  // Lazily bind this interface.
  if (!autofill_driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_driver_);
  }

  return *autofill_driver_;
}

mojom::PasswordManagerDriver& AutofillAgent::GetPasswordManagerDriver() {
  DCHECK(password_autofill_agent_);
  return password_autofill_agent_->GetPasswordManagerDriver();
}

}  // namespace autofill
