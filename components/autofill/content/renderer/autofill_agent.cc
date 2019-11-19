// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stddef.h>

#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
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
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "net/cert/cert_status_flags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
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
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
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
using blink::WebUserGestureIndicator;
using blink::WebVector;

namespace autofill {

using mojom::SubmissionSource;

namespace {

// Time to wait, in ms, o ensure that only a single select change will be acted
// upon, instead of multiple in close succession (debounce time).
size_t kWaitTimeForSelectOptionsChangesMs = 50;

// Gets all the data list values (with corresponding label) for the given
// element.
void GetDataListSuggestions(const WebInputElement& element,
                            std::vector<base::string16>* values,
                            std::vector<base::string16>* labels) {
  for (const auto& option : element.FilteredDataListOptions()) {
    values->push_back(option.Value().Utf16());
    if (option.Value() != option.Label())
      labels->push_back(option.Label().Utf16());
    else
      labels->push_back(base::string16());
  }
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<base::string16>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaxListSize)
    strings->resize(kMaxListSize);

  // Limit the size of the strings in the vector.
  for (size_t i = 0; i < strings->size(); ++i) {
    if ((*strings)[i].length() > kMaxDataLength)
      (*strings)[i].resize(kMaxDataLength);
  }
}

}  // namespace

AutofillAgent::ShowSuggestionsOptions::ShowSuggestionsOptions()
    : autofill_on_empty_values(false),
      requires_caret_at_end(false),
      show_full_suggestion_list(false),
      show_password_suggestions_only(false),
      autoselect_first_suggestion(false) {}

AutofillAgent::AutofillAgent(content::RenderFrame* render_frame,
                             PasswordAutofillAgent* password_autofill_agent,
                             PasswordGenerationAgent* password_generation_agent,
                             blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      form_cache_(render_frame->GetWebFrame()),
      password_autofill_agent_(password_autofill_agent),
      password_generation_agent_(password_generation_agent),
      autofill_query_id_(0),
      query_node_autofill_state_(WebAutofillState::kNotFilled),
      ignore_text_changes_(false),
      is_popup_possibly_visible_(false),
      is_generation_popup_possibly_visible_(false),
      is_user_gesture_required_(true),
      is_secure_context_required_(false),
      form_tracker_(render_frame) {
  render_frame->GetWebFrame()->SetAutofillClient(this);
  password_autofill_agent->SetAutofillAgent(this);
  AddFormObserver(this);
  registry->AddInterface(
      base::Bind(&AutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

AutofillAgent::~AutofillAgent() {
  RemoveFormObserver(this);
}

void AutofillAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

bool AutofillAgent::FormDataCompare::operator()(const FormData& lhs,
                                                const FormData& rhs) const {
  return std::tie(lhs.name, lhs.url, lhs.action, lhs.is_form_tag) <
         std::tie(rhs.name, rhs.url, rhs.action, rhs.is_form_tag);
}

void AutofillAgent::DidCommitProvisionalLoad(bool is_same_document_navigation,
                                             ui::PageTransition transition) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // TODO(dvadym): check if we need to check if it is main frame navigation
  // http://crbug.com/443155
  if (frame->Parent())
    return;  // Not a top-level navigation.

  if (is_same_document_navigation)
    return;

  // Navigation to a new page or a page refresh.

  element_.Reset();

  form_cache_.Reset();
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
}

void AutofillAgent::DidFinishDocumentLoad() {
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
  if (element != element_ || element_.IsNull() || focus_requires_scroll_ ||
      !is_popup_possibly_visible_ || !element_.Focused())
    return;

  FormData form;
  FormFieldData field;
  if (form_util::FindFormAndFieldForFormControlElement(element_, &form,
                                                       &field)) {
    GetAutofillDriver()->TextFieldDidScroll(
        form, field, render_frame()->ElementBoundsInWindow(element_));
  }

  // Ignore subsequent scroll offset changes.
  HidePopup();
}

void AutofillAgent::FocusedElementChanged(const WebElement& element) {
  was_focused_before_now_ = false;

  if ((IsKeyboardAccessoryEnabled() || !focus_requires_scroll_) &&
      WebUserGestureIndicator::IsProcessingUserGesture(
          element.IsNull() ? nullptr : element.GetDocument().GetFrame())) {
    focused_node_was_last_clicked_ = true;
    HandleFocusChangeComplete();
  }

  HidePopup();

  if (element.IsNull()) {
    if (!last_interacted_form_.IsNull()) {
      // Focus moved away from the last interacted form to somewhere else on
      // the page.
      GetAutofillDriver()->FocusNoLongerOnForm();
    }
    return;
  }

  const WebInputElement* input = ToWebInputElement(&element);

  if (!last_interacted_form_.IsNull() &&
      (!input || last_interacted_form_ != input->Form())) {
    // The focused element is not part of the last interacted form (could be
    // in a different form).
    GetAutofillDriver()->FocusNoLongerOnForm();
    return;
  }

  if (!input || !input->IsEnabled() || input->IsReadOnly() ||
      !input->IsTextField())
    return;

  element_ = *input;

  FormData form;
  FormFieldData field;
  if (form_util::FindFormAndFieldForFormControlElement(element_, &form,
                                                       &field)) {
    GetAutofillDriver()->FocusOnFormField(
        form, field, render_frame()->ElementBoundsInWindow(element_));
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
  FormData form_data;
  if (!form_util::ExtractFormData(form, &form_data))
    return;

  FireHostSubmitEvents(form_data, known_success, source);
}

void AutofillAgent::FireHostSubmitEvents(const FormData& form_data,
                                         bool known_success,
                                         SubmissionSource source) {
  // We don't want to fire duplicate submission event.
  if (!submitted_forms_.insert(form_data).second)
    return;

  GetAutofillDriver()->FormSubmitted(form_data, known_success, source);
}

void AutofillAgent::Shutdown() {
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AutofillAgent::TextFieldDidEndEditing(const WebInputElement& element) {
  // Sometimes "blur" events are side effects of the password generation
  // handling the page. They should not affect any UI in the browser.
  if (password_generation_agent_ &&
      password_generation_agent_->ShouldIgnoreBlur()) {
    return;
  }
  GetAutofillDriver()->DidEndTextFieldEditing();
  password_autofill_agent_->DidEndTextFieldEditing();
  if (password_generation_agent_)
    password_generation_agent_->DidEndTextFieldEditing(element);
}

void AutofillAgent::SetUserGestureRequired(bool required) {
  form_tracker_.set_user_gesture_required(required);
}

void AutofillAgent::TextFieldDidChange(const WebFormControlElement& element) {
  form_tracker_.TextFieldDidChange(element);
}

void AutofillAgent::OnTextFieldDidChange(const WebInputElement& element) {
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

  ShowSuggestionsOptions options;
  options.requires_caret_at_end = true;
  ShowSuggestions(element, options);

  FormData form;
  FormFieldData field;
  if (form_util::FindFormAndFieldForFormControlElement(element, &form,
                                                       &field)) {
    GetAutofillDriver()->TextFieldDidChange(
        form, field, render_frame()->ElementBoundsInWindow(element),
        AutofillTickClock::NowTicks());
  }
}

void AutofillAgent::TextFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  if (event.windows_key_code == ui::VKEY_DOWN ||
      event.windows_key_code == ui::VKEY_UP) {
    ShowSuggestionsOptions options;
    options.autofill_on_empty_values = true;
    options.requires_caret_at_end = true;
    options.autoselect_first_suggestion =
        ShouldAutoselectFirstSuggestionOnArrowDown();
    ShowSuggestions(element, options);
  }
}

void AutofillAgent::OpenTextDataListChooser(const WebInputElement& element) {
  ShowSuggestionsOptions options;
  options.autofill_on_empty_values = true;
  ShowSuggestions(element, options);
}

void AutofillAgent::DataListOptionsChanged(const WebInputElement& element) {
  if (!is_popup_possibly_visible_ || !element.Focused())
    return;

  OnProvisionallySaveForm(WebFormElement(), element,
                          ElementChangeSource::TEXTFIELD_CHANGED);
}

void AutofillAgent::UserGestureObserved() {
  password_autofill_agent_->UserGestureObserved();
}

void AutofillAgent::DoAcceptDataListSuggestion(
    const base::string16& suggested_value) {
  if (element_.IsNull())
    return;

  WebInputElement* input_element = ToWebInputElement(&element_);
  DCHECK(input_element);
  base::string16 new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (input_element->IsMultiple() && input_element->IsEmailField()) {
    base::string16 value = input_element->EditingValue().Utf16();
    std::vector<base::StringPiece16> parts =
        base::SplitStringPiece(value, base::ASCIIToUTF16(","),
                               base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() == 0)
      parts.push_back(base::StringPiece16());

    base::string16 last_part = parts.back().as_string();
    // We want to keep just the leading whitespace.
    for (size_t i = 0; i < last_part.size(); ++i) {
      if (!base::IsUnicodeWhitespace(last_part[i])) {
        last_part = last_part.substr(0, i);
        break;
      }
    }
    last_part.append(suggested_value);
    parts.back() = last_part;

    new_value = base::JoinString(parts, base::ASCIIToUTF16(","));
  }
  DoFillFieldWithValue(new_value, input_element);
}

void AutofillAgent::TriggerRefillIfNeeded(const FormData& form) {
  ReplaceElementIfNowInvalid(form);

  FormFieldData field;
  FormData updated_form;
  if (form_util::FindFormAndFieldForFormControlElement(element_, &updated_form,
                                                       &field) &&
      (!element_.IsAutofilled() || !form.DynamicallySameFormAs(updated_form))) {
    base::TimeTicks forms_seen_timestamp = AutofillTickClock::NowTicks();
    WebLocalFrame* frame = render_frame()->GetWebFrame();
    std::vector<FormData> forms;
    forms.push_back(updated_form);
    // Always communicate to browser process for topmost frame.
    if (!forms.empty() || !frame->Parent()) {
      GetAutofillDriver()->FormsSeen(forms, forms_seen_timestamp);
    }
  }
}

// mojom::AutofillAgent:
void AutofillAgent::FillForm(int32_t id, const FormData& form) {
  if (element_.IsNull())
    return;

  if (id != autofill_query_id_ && id != kNoQueryId)
    return;

  was_last_action_fill_ = true;

  // If this is a re-fill, replace the triggering element if it's invalid.
  if (id == kNoQueryId)
    ReplaceElementIfNowInvalid(form);

  query_node_autofill_state_ = element_.GetAutofillState();
  form_util::FillForm(form, element_);
  if (!element_.Form().IsNull())
    UpdateLastInteractedForm(element_.Form());

  GetAutofillDriver()->DidFillAutofillFormData(form,
                                               AutofillTickClock::NowTicks());

  TriggerRefillIfNeeded(form);
}

void AutofillAgent::PreviewForm(int32_t id, const FormData& form) {
  if (element_.IsNull())
    return;

  if (id != autofill_query_id_)
    return;

  query_node_autofill_state_ = element_.GetAutofillState();
  form_util::PreviewForm(form, element_);

  GetAutofillDriver()->DidPreviewAutofillFormData();
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

  form_util::ClearPreviewedFormWithElement(element_,
                                           query_node_autofill_state_);
}

void AutofillAgent::FillFieldWithValue(const base::string16& value) {
  if (element_.IsNull())
    return;

  WebInputElement* input_element = ToWebInputElement(&element_);
  if (input_element) {
    DoFillFieldWithValue(value, input_element);
    input_element->SetAutofillState(WebAutofillState::kAutofilled);
  }
}

void AutofillAgent::PreviewFieldWithValue(const base::string16& value) {
  if (element_.IsNull())
    return;

  WebInputElement* input_element = ToWebInputElement(&element_);
  if (input_element)
    DoPreviewFieldWithValue(value, input_element);
}

void AutofillAgent::SetSuggestionAvailability(
    const mojom::AutofillState state) {
  if (element_.IsNull())
    return;

  WebInputElement* input_element = ToWebInputElement(&element_);
  if (input_element) {
    switch (state) {
      case autofill::mojom::AutofillState::kAutofillAvailable:
        WebAXObject::FromWebNode(*input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kAutofillAvailable);
        return;
      case autofill::mojom::AutofillState::kAutocompleteAvailable:
        WebAXObject::FromWebNode(*input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kAutocompleteAvailable);
        return;
      case autofill::mojom::AutofillState::kNoSuggestions:
        WebAXObject::FromWebNode(*input_element)
            .HandleAutofillStateChanged(
                blink::WebAXAutofillState::kNoSuggestions);
        return;
    }
    NOTREACHED();
  }
}

void AutofillAgent::AcceptDataListSuggestion(const base::string16& value) {
  DoAcceptDataListSuggestion(value);
}

void AutofillAgent::FillPasswordSuggestion(const base::string16& username,
                                           const base::string16& password) {
  if (element_.IsNull())
    return;

  bool handled =
      password_autofill_agent_->FillSuggestion(element_, username, password);
  DCHECK(handled);
}

void AutofillAgent::PreviewPasswordSuggestion(const base::string16& username,
                                              const base::string16& password) {
  if (element_.IsNull())
    return;

  bool handled = password_autofill_agent_->PreviewSuggestion(
      element_, blink::WebString::FromUTF16(username),
      blink::WebString::FromUTF16(password));
  DCHECK(handled);
}

bool AutofillAgent::CollectFormlessElements(FormData* output) {
  if (render_frame() == nullptr || render_frame()->GetWebFrame() == nullptr)
    return false;

  WebDocument document = render_frame()->GetWebFrame()->GetDocument();

  // Build up the FormData from the unowned elements. This logic mostly
  // mirrors the construction of the synthetic form in form_cache.cc, but
  // happens at submit-time so we can capture the modifications the user
  // has made, and doesn't depend on form_cache's internal state.
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document.All(),
                                                         &fieldsets);

  if (control_elements.size() > form_util::kMaxParseableFields)
    return false;

  const form_util::ExtractMask extract_mask =
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTIONS);

  return form_util::UnownedCheckoutFormElementsAndFieldSetsToFormData(
      fieldsets, control_elements, nullptr, document, extract_mask, output,
      nullptr);
}

void AutofillAgent::ShowSuggestions(const WebFormControlElement& element,
                                    const ShowSuggestionsOptions& options) {
  if (!element.IsEnabled() || element.IsReadOnly())
    return;
  if (!element.SuggestedValue().IsEmpty())
    return;

  const WebInputElement* input_element = ToWebInputElement(&element);
  if (input_element) {
    if (!input_element->IsTextField())
      return;
    if (!input_element->SuggestedValue().IsEmpty())
      return;
  } else {
    DCHECK(form_util::IsTextAreaElement(element));
    if (!element.ToConst<WebFormControlElement>().SuggestedValue().IsEmpty())
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
          *input_element, options.show_full_suggestion_list,
          is_generation_popup_possibly_visible_)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (is_generation_popup_possibly_visible_)
    return;

  if (options.show_password_suggestions_only)
    return;

  // Password field elements should only have suggestions shown by the password
  // autofill agent.
  // The /*disable presubmit*/ comment below is used to disable a presubmit
  // script that ensures that only IsPasswordFieldForAutofill() is used in this
  // code (it has to appear between the function name and the parentesis to not
  // match a regex). In this specific case we are actually interested in whether
  // the field is currently a password field, not whether it has ever been a
  // password field.
  if (input_element &&
      input_element->IsPasswordField /*disable presubmit*/ () &&
      !query_password_suggestion_) {
    return;
  }

  QueryAutofillSuggestions(element, options.autoselect_first_suggestion);
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

void AutofillAgent::GetElementFormAndFieldData(
    const std::vector<std::string>& selectors,
    GetElementFormAndFieldDataCallback callback) {
  FormData form;
  FormFieldData field;
  blink::WebElement target_element = FindUniqueWebElement(selectors);
  if (target_element.IsNull() || !target_element.IsFormControlElement()) {
    return std::move(callback).Run(form, field);
  }

  blink::WebFormControlElement target_form_control_element =
      target_element.To<blink::WebFormControlElement>();
  bool success = form_util::FindFormAndFieldForFormControlElement(
      target_form_control_element, &form, &field);
  if (success) {
    // Remember this element so as to autofill the form without focusing the
    // field for Autofill Assistant.
    element_ = target_form_control_element;
  }
  // Do not expect failure.
  DCHECK(success);

  return std::move(callback).Run(form, field);
}

blink::WebElement AutofillAgent::FindUniqueWebElement(
    const std::vector<std::string>& selectors) {
  DCHECK(selectors.size() > 0);

  blink::WebVector<blink::WebElement> elements =
      render_frame()->GetWebFrame()->GetDocument().QuerySelectorAll(
          blink::WebString::FromUTF8(selectors[0]));
  if (elements.size() != 1) {
    return blink::WebElement();
  }

  // Get the unique element in |elements| and match the next selector inside it
  // if there are remaining selectors haven't been matched.
  blink::WebElement query_element = elements[0];
  for (size_t i = 1; i < selectors.size(); i++) {
    elements = query_element.QuerySelectorAll(
        blink::WebString::FromUTF8(selectors[i]));

    // Query shadow DOM if necessary.
    if (elements.size() == 0 && !query_element.ShadowRoot().IsNull()) {
      // TODO(806868): Query shadow dom when Autofill is available for forms in
      // shadow DOM (crbug.com/746593).
      return blink::WebElement();
    }

    // Return an empty element if there are multiple matching elements.
    if (elements.size() != 1) {
      return blink::WebElement();
    }

    query_element = elements[0];
  }

  return query_element;
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element,
    bool autoselect_first_suggestion) {
  if (!element.GetDocument().GetFrame())
    return;

  DCHECK(ToWebInputElement(&element) || form_util::IsTextAreaElement(element));

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  FormData form;
  FormFieldData field;
  if (!form_util::FindFormAndFieldForFormControlElement(element, &form,
                                                        &field)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(element, nullptr, form_util::EXTRACT_VALUE,
                                     &field);
  }

  if (is_secure_context_required_ &&
      !(element.GetDocument().IsSecureContext())) {
    LOG(WARNING) << "Autofill suggestions are disabled because the document "
                    "isn't a secure context.";
    return;
  }

  std::vector<base::string16> data_list_values;
  std::vector<base::string16> data_list_labels;
  const WebInputElement* input_element = ToWebInputElement(&element);
  if (input_element) {
    // Find the datalist values and send them to the browser process.
    GetDataListSuggestions(*input_element, &data_list_values,
                           &data_list_labels);
    TrimStringVectorForIPC(&data_list_values);
    TrimStringVectorForIPC(&data_list_labels);
  }

  is_popup_possibly_visible_ = true;

  GetAutofillDriver()->SetDataList(data_list_values, data_list_labels);
  GetAutofillDriver()->QueryFormFieldAutofill(
      autofill_query_id_, form, field,
      render_frame()->ElementBoundsInWindow(element_),
      autoselect_first_suggestion);
}

void AutofillAgent::DoFillFieldWithValue(const base::string16& value,
                                         WebInputElement* node) {
  form_tracker_.set_ignore_control_changes(true);
  node->SetAutofillValue(blink::WebString::FromUTF16(value));
  password_autofill_agent_->UpdateStateForTextChange(*node);
  form_tracker_.set_ignore_control_changes(false);
}

void AutofillAgent::DoPreviewFieldWithValue(const base::string16& value,
                                            WebInputElement* node) {
  query_node_autofill_state_ = element_.GetAutofillState();
  node->SetSuggestedValue(blink::WebString::FromUTF16(value));
  node->SetAutofillState(WebAutofillState::kPreviewed);
  form_util::PreviewSuggestion(node->SuggestedValue().Utf16(),
                               node->Value().Utf16(), node);
}

void AutofillAgent::ProcessForms() {
  // Record timestamp of when the forms are first seen. This is used to
  // measure the overhead of the Autofill feature.
  base::TimeTicks forms_seen_timestamp = AutofillTickClock::NowTicks();

  WebLocalFrame* frame = render_frame()->GetWebFrame();
  std::vector<FormData> forms = form_cache_.ExtractNewForms();

  // Always communicate to browser process for topmost frame.
  if (!forms.empty() || !frame->Parent()) {
    GetAutofillDriver()->FormsSeen(forms, forms_seen_timestamp);
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

  GetAutofillDriver()->HidePopup();
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
}

void AutofillAgent::DidReceiveLeftMouseDownOrGestureTapInNode(
    const WebNode& node) {
  DCHECK(!node.IsNull());
  focused_node_was_last_clicked_ = node.Focused();

  if (IsTouchToFillEnabled() || IsKeyboardAccessoryEnabled() ||
      !focus_requires_scroll_) {
    HandleFocusChangeComplete();
  }
}

void AutofillAgent::SelectControlDidChange(
    const WebFormControlElement& element) {
  form_tracker_.SelectControlDidChange(element);
}

void AutofillAgent::SelectFieldOptionsChanged(
    const blink::WebFormControlElement& element) {
  if (!was_last_action_fill_ || element_.IsNull())
    return;

  // Since a change of a select options often come in batches, use a timer
  // to wait for other changes. Stop the timer if it was already running. It
  // will be started again for this change.
  if (on_select_update_timer_.IsRunning())
    on_select_update_timer_.AbandonAndStop();

  // Start the timer to notify the driver that the select field was updated
  // after the options have finished changing,
  on_select_update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kWaitTimeForSelectOptionsChangesMs),
      base::BindRepeating(&AutofillAgent::SelectWasUpdated,
                          weak_ptr_factory_.GetWeakPtr(), element));
}

bool AutofillAgent::ShouldSuppressKeyboard(
    const WebFormControlElement& element) {
  // Note: This is currently only implemented for passwords. Consider supporting
  // other autofill types in the future as well.
  return password_autofill_agent_->ShouldSuppressKeyboard();
}

void AutofillAgent::SelectWasUpdated(
    const blink::WebFormControlElement& element) {
  // Look for the form and field associated with the select element. If they are
  // found, notify the driver that the the form was modified dynamically.
  FormData form;
  FormFieldData field;
  if (form_util::FindFormAndFieldForFormControlElement(element, &form,
                                                       &field) &&
      !field.option_values.empty()) {
    GetAutofillDriver()->SelectFieldOptionsDidChange(form);
  }
}

void AutofillAgent::FormControlElementClicked(
    const WebFormControlElement& element,
    bool was_focused) {
  last_clicked_form_control_element_for_testing_ = element;
  last_clicked_form_control_element_was_focused_for_testing_ = was_focused;
  was_last_action_fill_ = false;

  const WebInputElement* input_element = ToWebInputElement(&element);
  if (!input_element && !form_util::IsTextAreaElement(element))
    return;

  if (IsTouchToFillEnabled())
    password_autofill_agent_->TryToShowTouchToFill(element);

  ShowSuggestionsOptions options;
  options.autofill_on_empty_values = true;
  // Show full suggestions when clicking on an already-focused form field.
  options.show_full_suggestion_list = element.IsAutofilled() || was_focused;

  ShowSuggestions(element, options);
}

void AutofillAgent::HandleFocusChangeComplete() {
  WebElement focused_element =
      render_frame()->GetWebFrame()->GetDocument().FocusedElement();
  // When using Talkback on Android, and possibly others, traversing to and
  // focusing a field will not register as a click. Thus, when screen readers
  // are used, treat the focused node as if it was the last clicked. Also check
  // to ensure focus is on a field where text can be entered.
  if ((focused_node_was_last_clicked_ || is_screen_reader_enabled_) &&
      !focused_element.IsNull() && focused_element.IsFormControlElement() &&
      (form_util::IsTextInput(blink::ToWebInputElement(&focused_element)) ||
       focused_element.HasHTMLTagName("textarea"))) {
    FormControlElementClicked(focused_element.ToConst<WebFormControlElement>(),
                              was_focused_before_now_);
  }

  was_focused_before_now_ = true;
  focused_node_was_last_clicked_ = false;
}

void AutofillAgent::AjaxSucceeded() {
  form_tracker_.AjaxSucceeded();
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
    return;
  } else if (source == ElementChangeSource::TEXTFIELD_CHANGED ||
             source == ElementChangeSource::SELECT_CHANGED) {
    // Remember the last form the user interacted with.
    if (!element.Form().IsNull()) {
      UpdateLastInteractedForm(element.Form());
    } else {
      // Remove invisible elements
      for (auto it = formless_elements_user_edited_.begin();
           it != formless_elements_user_edited_.end();) {
        if (form_util::IsWebElementVisible(*it)) {
          it = formless_elements_user_edited_.erase(it);
        } else {
          ++it;
        }
      }
      formless_elements_user_edited_.insert(element);
      provisionally_saved_form_ = std::make_unique<FormData>();
      if (!CollectFormlessElements(provisionally_saved_form_.get())) {
        provisionally_saved_form_.reset();
      } else {
        last_interacted_form_.Reset();
      }
    }

    if (source == ElementChangeSource::TEXTFIELD_CHANGED)
      OnTextFieldDidChange(*ToWebInputElement(&element));
    else {
      FormData form;
      FormFieldData field;
      if (form_util::FindFormAndFieldForFormControlElement(element, &form,
                                                           &field)) {
        GetAutofillDriver()->SelectControlDidChange(
            form, field, render_frame()->ElementBoundsInWindow(element));
      }
    }
  }
}

void AutofillAgent::OnProbablyFormSubmitted() {
  FormData form_data;
  if (GetSubmittedForm(&form_data)) {
    FireHostSubmitEvents(form_data, /*known_success=*/false,
                         SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
}

void AutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  // Fire the submission event here because WILL_SEND_SUBMIT_EVENT is skipped
  // if javascript calls submit() directly.
  FireHostSubmitEvents(form, /*known_success=*/false,
                       SubmissionSource::FORM_SUBMISSION);
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
}

void AutofillAgent::OnInferredFormSubmission(SubmissionSource source) {
  // Only handle iframe for FRAME_DETACHED or main frame for
  // SAME_DOCUMENT_NAVIGATION.
  if ((source == SubmissionSource::FRAME_DETACHED &&
       !render_frame()->GetWebFrame()->Parent()) ||
      (source == SubmissionSource::SAME_DOCUMENT_NAVIGATION &&
       render_frame()->GetWebFrame()->Parent())) {
    ResetLastInteractedElements();
    OnFormNoLongerSubmittable();
    return;
  }

  if (source == SubmissionSource::FRAME_DETACHED) {
    // Should not access the frame because it is now detached. Instead, use
    // |provisionally_saved_form_|.
    if (provisionally_saved_form_)
      FireHostSubmitEvents(*provisionally_saved_form_, /*known_success=*/true,
                           source);
  } else {
    FormData form_data;
    if (GetSubmittedForm(&form_data))
      FireHostSubmitEvents(form_data, /*known_success=*/true, source);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
}

void AutofillAgent::AddFormObserver(Observer* observer) {
  form_tracker_.AddObserver(observer);
}

void AutofillAgent::RemoveFormObserver(Observer* observer) {
  form_tracker_.RemoveObserver(observer);
}

bool AutofillAgent::GetSubmittedForm(FormData* form) {
  if (!last_interacted_form_.IsNull()) {
    if (form_util::ExtractFormData(last_interacted_form_, form)) {
      return true;
    } else if (provisionally_saved_form_) {
      *form = *provisionally_saved_form_;
      return true;
    }
  } else if (formless_elements_user_edited_.size() != 0 &&
             !form_util::IsSomeControlElementVisible(
                 formless_elements_user_edited_)) {
    // we check if all the elements the user has interacted with are gone,
    // to decide if submission has occurred, and use the
    // provisionally_saved_form_ saved in OnProvisionallySaveForm() if fail to
    // construct form.
    if (CollectFormlessElements(form)) {
      return true;
    } else if (provisionally_saved_form_) {
      *form = *provisionally_saved_form_;
      return true;
    }
  }
  return false;
}

void AutofillAgent::ResetLastInteractedElements() {
  last_interacted_form_.Reset();
  last_clicked_form_control_element_for_testing_.Reset();
  formless_elements_user_edited_.clear();
  provisionally_saved_form_.reset();
}

void AutofillAgent::UpdateLastInteractedForm(blink::WebFormElement form) {
  last_interacted_form_ = form;
  provisionally_saved_form_ = std::make_unique<FormData>();
  if (!form_util::ExtractFormData(last_interacted_form_,
                                  provisionally_saved_form_.get())) {
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

  WebVector<WebFormElement> forms;
  WebVector<WebFormControlElement> elements;

  const auto original_element = element_;
  WebFormControlElement matching_element;
  bool potential_match_encountered = false;

  if (original_form.name.empty()) {
    // If the form has no name, check all the forms.
    element_.GetDocument().Forms(forms);
    for (const WebFormElement& form : forms) {
      form.GetFormControlElements(elements);
      // If finding a unique element is impossible, don't look further.
      if (!FindTheUniqueNewVersionOfOldElement(
              elements, potential_match_encountered, matching_element,
              original_element))
        return;
    }
    // If the element is not found, we should still check for unowned elements.
    if (!matching_element.IsNull()) {
      element_ = matching_element;
      return;
    }
  }

  if (!element_.Form().IsNull()) {
    // If |element_|'s parent form has no elements, |element_| is now invalid
    // and should be updated.
    WebVector<WebFormControlElement> form_elements;
    element_.Form().GetFormControlElements(form_elements);
    if (!form_elements.empty())
      return;
  }

  WebFormElement form_element;
  bool form_is_found = false;
  if (!original_form.name.empty()) {
    // Try to find the new version of the form.
    element_.GetDocument().Forms(forms);
    for (const WebFormElement& form : forms) {
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
    elements = form_util::GetUnownedAutofillableFormFieldElements(
        element_.GetDocument().All(), &fieldsets);
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
  form_element.GetFormControlElements(elements);
  // If a unique match was found.
  if (FindTheUniqueNewVersionOfOldElement(elements, potential_match_encountered,
                                          matching_element, original_element) &&
      !matching_element.IsNull()) {
    element_ = matching_element;
  }
}

const mojo::AssociatedRemote<mojom::AutofillDriver>&
AutofillAgent::GetAutofillDriver() {
  if (!autofill_driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_driver_);
  }

  return autofill_driver_;
}

const mojo::AssociatedRemote<mojom::PasswordManagerDriver>&
AutofillAgent::GetPasswordManagerDriver() {
  DCHECK(password_autofill_agent_);
  return password_autofill_agent_->GetPasswordManagerDriver();
}

}  // namespace autofill
