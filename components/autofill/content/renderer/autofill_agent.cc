// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stddef.h>
#include <memory>
#include <optional>
#include <tuple>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/a11y_utils.h"
#include "components/autofill/content/renderer/form_autofill_issues.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/content/renderer/suggestion_properties.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/common/password_manager_features.h"
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
#include "third_party/blink/public/web/web_form_related_change_type.h"
#include "third_party/blink/public/web/web_input_element.h"
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

using form_util::ExtractOption;
using form_util::FindContentEditableByRendererId;
using form_util::FindFormAndFieldForFormControlElement;
using form_util::FindFormByRendererId;
using form_util::FindFormForContentEditable;
using form_util::IsElementEditable;
using form_util::MaybeWasOwnedByFrame;
using form_util::TraverseDomForFourDigitCombinations;
using mojom::SubmissionSource;
using ShowAll = PasswordAutofillAgent::ShowAll;
using mojom::FocusedFieldType;

namespace {

// Time to wait in ms to ensure that only a single select or datalist change
// will be acted upon, instead of multiple in close succession (debounce time).
size_t kWaitTimeForOptionsChangesMs = 50;

DenseSet<ExtractOption> MaybeExtractDatalist(
    DenseSet<ExtractOption> extract_options) {
  if (base::FeatureList::IsEnabled(features::kAutofillExtractAllDatalists)) {
    extract_options.insert(ExtractOption::kDatalist);
  }
  return extract_options;
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
  explicit DeferringAutofillDriver(AutofillAgent* agent)
      : agent_(CHECK_DEREF(agent)) {}
  ~DeferringAutofillDriver() override = default;

 private:
  template <typename F, typename... Args>
  void SendMsg(F fn, Args&&... args) {
    if (auto* autofill_driver = agent_->unsafe_autofill_driver()) {
      DCHECK(!agent_->IsPrerendering());
      DCHECK_NE(autofill_driver, this);
      (autofill_driver->*fn)(std::forward<Args>(args)...);
    }
  }

  template <typename F, typename... Args>
  void DeferMsg(F fn, Args... args) {
    if (auto* render_frame = agent_->unsafe_render_frame()) {
      DCHECK(agent_->IsPrerendering());
      render_frame->GetWebFrame()
          ->GetDocument()
          .AddPostPrerenderingActivationStep(base::BindOnce(
              &DeferringAutofillDriver::SendMsg<F, Args...>,
              weak_ptr_factory_.GetWeakPtr(), fn, std::forward<Args>(args)...));
    }
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
  void SelectOrSelectListFieldOptionsDidChange(const FormData& form) override {
    DeferMsg(&mojom::AutofillDriver::SelectOrSelectListFieldOptionsDidChange,
             form);
  }
  void AskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source) override {
    DeferMsg(&mojom::AutofillDriver::AskForValuesToFill, form, field,
             bounding_box, trigger_source);
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

  const raw_ref<AutofillAgent, ExperimentalRenderer> agent_;
  base::WeakPtrFactory<DeferringAutofillDriver> weak_ptr_factory_{this};
};

AutofillAgent::FocusStateNotifier::FocusStateNotifier(AutofillAgent* agent)
    : agent_(CHECK_DEREF(agent)) {}

AutofillAgent::FocusStateNotifier::~FocusStateNotifier() = default;

void AutofillAgent::FocusStateNotifier::FocusedInputChanged(
    const WebNode& node) {
  CHECK(!node.IsNull());
  FocusedFieldType new_focused_field_type = FocusedFieldType::kUnknown;
  FieldRendererId new_focused_field_id = FieldRendererId();
  if (auto form_control_element = node.DynamicTo<WebFormControlElement>();
      !form_control_element.IsNull()) {
    new_focused_field_type = GetFieldType(form_control_element);
    new_focused_field_id = form_util::GetFieldRendererId(form_control_element);
  }
  NotifyIfChanged(new_focused_field_type, new_focused_field_id);
}

void AutofillAgent::FocusStateNotifier::ResetFocus() {
  FieldRendererId new_focused_field_id = FieldRendererId();
  FocusedFieldType new_focused_field_type = FocusedFieldType::kUnknown;
  NotifyIfChanged(new_focused_field_type, new_focused_field_id);
}

FocusedFieldType AutofillAgent::FocusStateNotifier::GetFieldType(
    const WebFormControlElement& node) {
  if (form_util::IsTextAreaElement(node.To<WebFormControlElement>())) {
    return FocusedFieldType::kFillableTextArea;
  }

  WebInputElement input_element = node.DynamicTo<WebInputElement>();
  if (input_element.IsNull() || !input_element.IsTextField() ||
      !IsElementEditable(input_element)) {
    return FocusedFieldType::kUnfillableElement;
  }

  if (input_element.FormControlTypeForAutofill() ==
      blink::mojom::FormControlType::kInputSearch) {
    return FocusedFieldType::kFillableSearchField;
  }
  if (input_element.IsPasswordFieldForAutofill()) {
    return FocusedFieldType::kFillablePasswordField;
  }
  if (agent_->password_autofill_agent_->IsUsernameInputField(input_element)) {
    return FocusedFieldType::kFillableUsernameField;
  }
  if (form_util::IsWebauthnTaggedElement(node)) {
    return FocusedFieldType::kFillableWebauthnTaggedField;
  }
  return FocusedFieldType::kFillableNonSearchField;
}

void AutofillAgent::FocusStateNotifier::NotifyIfChanged(
    mojom::FocusedFieldType new_focused_field_type,
    FieldRendererId new_focused_field_id) {
  // Forward the request if the focused field is different from the previous
  // one.
  if (focused_field_id_ == new_focused_field_id &&
      focused_field_type_ == new_focused_field_type) {
    return;
  }

  // TODO(crbug.com/1425166): Move FocusedInputChanged to AutofillDriver.
  agent_->GetPasswordManagerDriver().FocusedInputChanged(
      new_focused_field_id, new_focused_field_type);

  focused_field_type_ = new_focused_field_type;
  focused_field_id_ = new_focused_field_id;
}

AutofillAgent::AutofillAgent(
    content::RenderFrame* render_frame,
    std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
    std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      form_cache_(std::make_unique<FormCache>(render_frame->GetWebFrame())),
      password_autofill_agent_(std::move(password_autofill_agent)),
      password_generation_agent_(std::move(password_generation_agent)),
      query_node_autofill_state_(WebAutofillState::kNotFilled),
      is_popup_possibly_visible_(false),
      is_secure_context_required_(false),
      form_tracker_(std::make_unique<FormTracker>(render_frame)),
      field_data_manager_(base::MakeRefCounted<FieldDataManager>()),
      focus_state_notifier_(this) {
  render_frame->GetWebFrame()->SetAutofillClient(this);
  password_autofill_agent_->Init(this);
  AddFormObserver(this);
  AddFormObserver(password_autofill_agent_.get());
  registry->AddInterface<mojom::AutofillAgent>(base::BindRepeating(
      &AutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

// The destructor is not guaranteed to be called. Destruction happens (only)
// through the OnDestruct() event, which posts a task to delete this object.
// The process may be killed before this deletion can happen.
AutofillAgent::~AutofillAgent() {
  RemoveFormObserver(this);
  RemoveFormObserver(password_autofill_agent_.get());
}

void AutofillAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void AutofillAgent::DidCommitProvisionalLoad(ui::PageTransition transition) {
  // Navigation to a new page or a page refresh.
  last_queried_element_ = {};
  form_cache_ =
      unsafe_render_frame()
          ? std::make_unique<FormCache>(unsafe_render_frame()->GetWebFrame())
          : nullptr;
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::DidDispatchDOMContentLoadedEvent() {
  is_dom_content_loaded_ = true;
  ExtractFormsUnthrottled(/*callback=*/{});
}

void AutofillAgent::DidChangeScrollOffset() {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (last_queried_element.IsNull()) {
    return;
  }

  if (!focus_requires_scroll_) {
    // Post a task here since scroll offset may change during layout.
    // TODO(crbug.com/804886): Do not cancel other tasks and do not invalidate
    // PasswordAutofillAgent::autofill_agent_.
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (auto* render_frame = unsafe_render_frame()) {
      render_frame->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
          ->PostTask(FROM_HERE,
                     base::BindOnce(&AutofillAgent::DidChangeScrollOffsetImpl,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    last_queried_element));
    }
  } else {
    HidePopup();
  }
}

void AutofillAgent::DidChangeScrollOffsetImpl(
    const WebFormControlElement& element) {
  if (element != last_queried_element_.GetField() || element.IsNull() ||
      focus_requires_scroll_ || !is_popup_possibly_visible_ ||
      !element.Focused()) {
    return;
  }

  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element, field_data_manager(),
          MaybeExtractDatalist({ExtractOption::kBounds}), &form, &field)) {
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidScroll(form, field, field.bounds);
    }
  }

  // Ignore subsequent scroll offset changes.
  HidePopup();
}

void AutofillAgent::FocusedElementChanged(const WebElement& element) {
  HidePopup();

  WebFormElement last_interacted_form = last_interacted_form_.GetForm();
  if (element.IsNull()) {
    // Focus moved away from the last interacted form (if any) to somewhere else
    // on the page.
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FocusNoLongerOnForm(!last_interacted_form.IsNull());
    }
    return;
  }

  const WebFormControlElement form_control_element =
      element.DynamicTo<WebFormControlElement>();

  bool focus_moved_to_new_form = false;
  if (!last_interacted_form.IsNull() &&
      (form_control_element.IsNull() ||
       last_interacted_form != form_control_element.Form())) {
    // The focused element is not part of the last interacted form (could be
    // in a different form).
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FocusNoLongerOnForm(/*had_interacted_form=*/true);
    }
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
    // If the focus change was caused by a user gesture,
    // DidReceiveLeftMouseDownOrGestureTapInNode() will show the autofill
    // suggestions. See crbug.com/730764 for why showing autofill suggestions as
    // a result of JavaScript changing focus is enabled on WebView.
    bool focused_node_was_last_clicked =
        !base::FeatureList::IsEnabled(
            features::kAutofillAndroidDisableSuggestionsOnJSFocus) ||
        !focus_requires_scroll_;
    HandleFocusChangeComplete(
        /*focused_node_was_last_clicked=*/focused_node_was_last_clicked);
  }

  if (focus_moved_to_new_form) {
    return;
  }

  if (form_control_element.IsNull() || !form_control_element.IsEnabled() ||
      !form_util::IsTextAreaElementOrTextInput(form_control_element)) {
    return;
  }

  last_queried_element_ = FieldRef(form_control_element);

  FormData form;
  FormFieldData field;
  if (!form_control_element.IsReadOnly() &&
      FindFormAndFieldForFormControlElement(
          last_queried_element_.GetField(), field_data_manager(),
          MaybeExtractDatalist({ExtractOption::kBounds}), &form, &field)) {
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FocusOnFormField(form, field, field.bounds);
    }
  }
}

// AutofillAgent is deleted asynchronously because OnDestruct() may be
// triggered by JavaScript, which in turn may be triggered by AutofillAgent.
void AutofillAgent::OnDestruct() {
  receiver_.reset();
  form_cache_ = nullptr;
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void AutofillAgent::AccessibilityModeChanged(const ui::AXMode& mode) {
  is_screen_reader_enabled_ = mode.has_mode(ui::AXMode::kScreenReader);
}

void AutofillAgent::FireHostSubmitEvents(const WebFormElement& form,
                                         bool known_success,
                                         SubmissionSource source) {
  DCHECK(MaybeWasOwnedByFrame(form, unsafe_render_frame()));
  if (std::optional<FormData> form_data =
          form_util::ExtractFormData(form, field_data_manager())) {
    FireHostSubmitEvents(*form_data, known_success, source);
  }
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
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->FormSubmitted(form_data, known_success, source);
  }
}

void AutofillAgent::TextFieldDidEndEditing(const WebInputElement& element) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  // Sometimes "blur" events are side effects of the password generation
  // handling the page. They should not affect any UI in the browser.
  if (password_generation_agent_ &&
      password_generation_agent_->ShouldIgnoreBlur()) {
    return;
  }
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->DidEndTextFieldEditing();
  }
  focus_state_notifier_.ResetFocus();
  if (password_generation_agent_) {
    password_generation_agent_->DidEndTextFieldEditing(element);
  }

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::SetUserGestureRequired(bool required) {
  form_tracker_->set_user_gesture_required(required);
}

void AutofillAgent::TextFieldDidChange(const WebFormControlElement& element) {
  form_tracker_->TextFieldDidChange(element);
}

void AutofillAgent::OnTextFieldDidChange(const WebFormControlElement& element) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  // TODO(crbug.com/1494479): Add throttling to avoid sending this event for
  // rapid changes.

  // The field might have changed while the user was hovering on a suggestion,
  // the preview in that case should be cleared since new suggestions will be
  // showing up.
  ClearPreviewedForm();

  UpdateStateForTextChange(element, FieldPropertiesFlags::kUserTyped);

  const auto input_element = element.DynamicTo<WebInputElement>();
  if (password_generation_agent_ && !input_element.IsNull() &&
      password_generation_agent_->TextDidChangeInTextField(input_element)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (!input_element.IsNull() &&
      password_autofill_agent_->TextDidChangeInTextField(input_element)) {
    is_popup_possibly_visible_ = true;
    last_queried_element_ = FieldRef(element);
    return;
  }

  if (!input_element.IsNull()) {
    ShowSuggestions(element,
                    AutofillSuggestionTriggerSource::kTextFieldDidChange);
  }

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(
          element, field_data_manager(),
          MaybeExtractDatalist({ExtractOption::kBounds}), &form, &field)) {
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidChange(form, field, field.bounds,
                                          AutofillTickClock::NowTicks());
    }
  }
}

void AutofillAgent::TextFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (event.windows_key_code == ui::VKEY_DOWN ||
      event.windows_key_code == ui::VKEY_UP) {
    ShowSuggestions(
        element, AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown);
  }
}

void AutofillAgent::OpenTextDataListChooser(const WebInputElement& element) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  ShowSuggestions(element,
                  AutofillSuggestionTriggerSource::kOpenTextDataListChooser);
}

// Notifies the AutofillDriver about changes in the <datalist> options in
// batches.
//
// A batch ends if no event occurred for `kWaitTimeForOptionsChangesMs`
// milliseconds. For a given batch, the AutofillDriver is informed only about
// the last field. That is, if within one batch the options of different
// fields changed, all but one of these events will be lost.
void AutofillAgent::DataListOptionsChanged(const WebInputElement& element) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (element.GetDocument().IsNull() || !is_popup_possibly_visible_ ||
      !element.Focused()) {
    return;
  }

  if (datalist_option_change_batch_timer_.IsRunning()) {
    datalist_option_change_batch_timer_.AbandonAndStop();
  }

  datalist_option_change_batch_timer_.Start(
      FROM_HERE, base::Milliseconds(kWaitTimeForOptionsChangesMs),
      base::BindRepeating(&AutofillAgent::BatchDataListOptionChange,
                          weak_ptr_factory_.GetWeakPtr(), element));
}

void AutofillAgent::BatchDataListOptionChange(
    const blink::WebFormControlElement& element) {
  if (element.GetDocument().IsNull()) {
    return;
  }

  OnProvisionallySaveForm(element.Form(), element,
                          ElementChangeSource::TEXTFIELD_CHANGED);
}

void AutofillAgent::UserGestureObserved() {
  password_autofill_agent_->UserGestureObserved();
}

void AutofillAgent::TriggerRefillIfNeeded(const FormData& form) {
  if (!unsafe_render_frame()) {
    return;
  }
  WebFormElement updated_form_element = form_util::FindFormByRendererId(
      unsafe_render_frame()->GetWebFrame()->GetDocument(),
      form.unique_renderer_id);
  std::optional<FormData> updated_form_data =
      updated_form_element.IsNull()
          ? CollectFormlessElements()
          : form_util::ExtractFormData(updated_form_element,
                                       field_data_manager());
  // Deep-compare forms, but don't take into account the fields' values.
  if (auto* autofill_driver = unsafe_autofill_driver();
      autofill_driver && updated_form_data &&
      !FormData::DeepEqual(form, *updated_form_data)) {
    autofill_driver->FormsSeen({*updated_form_data}, {});
  }
}

// mojom::AutofillAgent:
void AutofillAgent::ApplyFormAction(mojom::ActionType action_type,
                                    mojom::ActionPersistence action_persistence,
                                    const FormData& form) {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  // If `last_queried_element_` is null or not focused, Autofill was either
  // triggered from another frame or the `last_queried_element_` has been
  // detached from the DOM or the focus was moved otherwise.
  //
  // If `last_queried_element_` is from a different form than `form`, then
  // Autofill was triggered from a different form in the same frame, and either
  // this is a subframe and both forms should be filled, or focus has changed
  // right after the user accepted the suggestions.
  //
  // In these cases, we set `last_queried_element_` to some form field as if
  // Autofill had been triggered from that field. This is necessary because
  // currently AutofillAgent relies on `last_queried_element_` in many places.
  if (!form.fields.empty() &&
      (last_queried_element.IsNull() || !last_queried_element.Focused() ||
       form_util::GetFormRendererId(form_util::GetOwningForm(
           last_queried_element)) != form.unique_renderer_id)) {
    if (!unsafe_render_frame()) {
      return;
    }
    WebDocument document = unsafe_render_frame()->GetWebFrame()->GetDocument();
    for (const FormFieldData& field : form.fields) {
      last_queried_element = form_util::FindFormControlByRendererId(
          document, field.unique_renderer_id);
      if (!last_queried_element.IsNull()) {
        last_queried_element_ = FieldRef(last_queried_element);
        break;
      }
    }
  }

  if (last_queried_element.IsNull()) {
    return;
  }

  ClearPreviewedForm();

  if (action_persistence == mojom::ActionPersistence::kPreview) {
    query_node_autofill_state_ = last_queried_element.GetAutofillState();
    previewed_elements_ = form_util::ApplyFormAction(
        form.fields, last_queried_element, action_type, action_persistence,
        field_data_manager());
  } else {
    was_last_action_fill_ = true;

    query_node_autofill_state_ = last_queried_element.GetAutofillState();
    bool filled_some_fields =
        !form_util::ApplyFormAction(form.fields, last_queried_element,
                                    action_type, action_persistence,
                                    field_data_manager())
             .empty();

    if (!last_queried_element.Form().IsNull()) {
      UpdateLastInteractedForm(last_queried_element.Form());
    } else {
      formless_elements_were_autofilled_ |= filled_some_fields;
    }

    // TODO(crbug.com/1198811): Inform the BrowserAutofillManager about the
    // fields that were actually filled. It's possible that the form has changed
    // since the time filling was triggered.
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->DidFillAutofillFormData(form,
                                               AutofillTickClock::NowTicks());
    }
    if (action_type == mojom::ActionType::kFill) {
      TriggerRefillIfNeeded(form);
    }
    SendPotentiallySubmittedFormToBrowser();
  }
  last_action_type_ = action_type;
}

void AutofillAgent::FieldTypePredictionsAvailable(
    const std::vector<FormDataPredictions>& forms) {
  bool attach_predictions_to_dom = base::FeatureList::IsEnabled(
      features::test::kAutofillShowTypePredictions);
  if (!form_cache_) {
    return;
  }
  for (const auto& form : forms) {
    form_cache_->ShowPredictions(form, attach_predictions_to_dom);
  }
}

void AutofillAgent::ClearSection() {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (last_queried_element.IsNull() || !form_cache_) {
    return;
  }
  form_cache_->ClearSectionWithElement(last_queried_element,
                                       field_data_manager());
}

void AutofillAgent::ClearPreviewedForm() {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  // TODO(crbug.com/816533): It is very rare, but it looks like the |element_|
  // can be null if a provisional load was committed immediately prior to
  // clearing the previewed form.
  if (last_queried_element.IsNull()) {
    return;
  }
  // |password_generation_agent_| can be null in android_webview & weblayer.
  if (password_generation_agent_ &&
      password_generation_agent_->DidClearGenerationSuggestion(
          last_queried_element)) {
    return;
  }
  if (password_autofill_agent_->DidClearAutofillSelection(
          last_queried_element)) {
    return;
  }
  std::vector<WebFormControlElement> previewed_elements;
  for (const FieldRef& previewed_element : previewed_elements_) {
    previewed_elements.push_back(previewed_element.GetField());
  }
  form_util::ClearPreviewedElements(last_action_type_, previewed_elements,
                                    last_queried_element,
                                    query_node_autofill_state_);
  previewed_elements_ = {};
}

void AutofillAgent::TriggerSuggestions(
    FieldRendererId field_id,
    AutofillSuggestionTriggerSource trigger_source) {
  if (!unsafe_render_frame()) {
    return;
  }
  if (WebFormControlElement control_element =
          form_util::FindFormControlByRendererId(
              unsafe_render_frame()->GetWebFrame()->GetDocument(), field_id);
      !control_element.IsNull()) {
    last_queried_element_ = FieldRef(control_element);
    ShowSuggestions(control_element, trigger_source);
  }
}

void AutofillAgent::ApplyFieldAction(
    mojom::ActionPersistence action_persistence,
    mojom::TextReplacement text_replacement,
    FieldRendererId field_id,
    const std::u16string& value) {
  if (!unsafe_render_frame()) {
    return;
  }

  // TODO(crbug.com/1427131): Look up `field_id` rather than using
  // `last_queried_element_` once
  // blink::features::kAutofillUseDomNodeIdForRendererId is enabled.
  WebFormControlElement form_control = last_queried_element_.GetField();
  if (!form_control.IsNull() &&
      field_id == form_util::GetFieldRendererId(form_control) &&
      form_util::IsTextAreaElementOrTextInput(form_control)) {
    DCHECK(MaybeWasOwnedByFrame(form_control, unsafe_render_frame()));
    ClearPreviewedForm();
    switch (action_persistence) {
      case mojom::ActionPersistence::kPreview:
        switch (text_replacement) {
          case mojom::TextReplacement::kReplaceSelection:
            NOTIMPLEMENTED()
                << "Previewing replacement of selection is not implemented";
            break;
          case mojom::TextReplacement::kReplaceAll:
            query_node_autofill_state_ = form_control.GetAutofillState();
            form_control.SetSuggestedValue(blink::WebString::FromUTF16(value));
            form_util::PreviewSuggestion(form_control.SuggestedValue().Utf16(),
                                         form_control.Value().Utf16(),
                                         &form_control);
            previewed_elements_.emplace_back(last_queried_element_);
            break;
        }
        break;
      case mojom::ActionPersistence::kFill:
        switch (text_replacement) {
          case mojom::TextReplacement::kReplaceSelection: {
            form_control.PasteText(WebString::FromUTF16(value),
                                   /*replace_all=*/false);
            break;
          }
          case mojom::TextReplacement::kReplaceAll: {
            DoFillFieldWithValue(value, form_control,
                                 WebAutofillState::kAutofilled);
            break;
          }
        }
        break;
    }
    return;
  }

  if (WebElement content_editable =
          form_util::FindContentEditableByRendererId(field_id);
      !content_editable.IsNull()) {
    switch (action_persistence) {
      case mojom::ActionPersistence::kPreview:
        NOTIMPLEMENTED()
            << "Previewing replacement of selection is not implemented";
        break;
      case mojom::ActionPersistence::kFill:
        content_editable.PasteText(
            WebString::FromUTF16(value),
            /*replace_all=*/
            (text_replacement == mojom::TextReplacement::kReplaceAll));
        break;
    }
  }
}

void AutofillAgent::SetSuggestionAvailability(
    FieldRendererId field_id,
    mojom::AutofillSuggestionAvailability suggestion_availability) {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (last_queried_element.IsNull() ||
      field_id != form_util::GetFieldRendererId(last_queried_element)) {
    return;
  }

  SetAutofillSuggestionAvailability(
      last_queried_element.DynamicTo<WebInputElement>(),
      suggestion_availability);
}

void AutofillAgent::AcceptDataListSuggestion(
    FieldRendererId field_id,
    const std::u16string& suggested_value) {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (last_queried_element.IsNull() ||
      field_id != form_util::GetFieldRendererId(last_queried_element)) {
    return;
  }

  WebInputElement input_element =
      last_queried_element.DynamicTo<WebInputElement>();
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
    if (parts.empty()) {
      parts.emplace_back();
    }
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
  DoFillFieldWithValue(new_value, last_queried_element,
                       WebAutofillState::kNotFilled);
}

void AutofillAgent::PreviewPasswordSuggestion(const std::u16string& username,
                                              const std::u16string& password) {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  if (last_queried_element.IsNull()) {
    return;
  }

  bool handled = password_autofill_agent_->PreviewSuggestion(
      last_queried_element, blink::WebString::FromUTF16(username),
      blink::WebString::FromUTF16(password));
  DCHECK(handled);
}

void AutofillAgent::PreviewPasswordGenerationSuggestion(
    const std::u16string& password) {
  DCHECK(password_generation_agent_);
  password_generation_agent_->PreviewGenerationSuggestion(password);
}

std::optional<FormData> AutofillAgent::CollectFormlessElements(
    DenseSet<ExtractOption> extract_options) const {
  if (!unsafe_render_frame()) {
    return std::nullopt;
  }
  WebDocument document = unsafe_render_frame()->GetWebFrame()->GetDocument();

  // Build up the FormData from the unowned elements. This logic mostly
  // mirrors the construction of the synthetic form in form_cache.cc, but
  // happens at submit-time so we can capture the modifications the user
  // has made, and doesn't depend on form_cache's internal state.
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(document);

  std::vector<WebElement> iframe_elements =
      form_util::GetUnownedIframeElements(document);

  FormData formless_elements_form;
  // TODO(crbug.com/1007974): Make this function return std::optional too.
  bool extraction_successful = form_util::UnownedFormElementsToFormData(
      control_elements, iframe_elements, nullptr, document,
      field_data_manager(), extract_options, &formless_elements_form,
      /*field=*/nullptr);
  return extraction_successful
             ? std::optional(std::move(formless_elements_form))
             : std::nullopt;
}

void AutofillAgent::ShowSuggestions(
    const WebFormControlElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  // TODO(crbug.com/1467359): Make this a CHECK.
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  CHECK_NE(trigger_source, AutofillSuggestionTriggerSource::kUnspecified);

  if (!element.IsEnabled() || element.IsReadOnly()) {
    return;
  }
  if (!element.SuggestedValue().IsEmpty()) {
    return;
  }

  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  if (!input_element.IsNull()) {
    if (!input_element.IsTextField()) {
      return;
    }
    if (!input_element.SuggestedValue().IsEmpty()) {
      return;
    }
  } else {
    DCHECK(form_util::IsTextAreaElement(element));
    if (!element.To<WebFormControlElement>().SuggestedValue().IsEmpty()) {
      return;
    }
  }

  // Don't attempt to autofill with values that are too large or if filling
  // criteria are not met. Keyboard Accessory may still be shown when the
  // |value| is empty, do not attempt to hide it.
  WebString value = element.EditingValue();
  if (value.length() > kMaxStringLength ||
      (!ShouldAutofillOnEmptyValues(trigger_source) && value.IsEmpty() &&
       !IsKeyboardAccessoryEnabled()) ||
      (RequiresCaretAtEnd(trigger_source) &&
       (element.SelectionStart() != element.SelectionEnd() ||
        element.SelectionEnd() != value.length()))) {
    // Any popup currently showing is obsolete.
    HidePopup();
    return;
  }

  last_queried_element_ = FieldRef(element);
  if (form_util::IsAutofillableInputElement(input_element)) {
    if (password_generation_agent_ &&
        password_generation_agent_->ShowPasswordGenerationSuggestions(
            input_element)) {
      is_popup_possibly_visible_ = true;
      return;
    }
    if (password_autofill_agent_->ShowSuggestions(
            input_element,
            ShowAll(ShouldShowFullSuggestionListForPasswordManager(
                trigger_source, element)))) {
      is_popup_possibly_visible_ = true;
      return;
    }
  }

  // Password field elements should only have suggestions shown by the password
  // autofill agent.
  // The /*disable presubmit*/ comment below is used to disable a presubmit
  // script that ensures that only IsPasswordFieldForAutofill() is used in this
  // code (it has to appear between the function name and the parenthesis to not
  // match a regex). In this specific case we are actually interested in whether
  // the field is currently a password field, not whether it has ever been a
  // password field.
  if (!input_element.IsNull() &&
      input_element.IsPasswordField /*disable presubmit*/ () &&
      !query_password_suggestion_) {
    return;
  }

  QueryAutofillSuggestions(element, trigger_source);
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

void AutofillAgent::EnableHeavyFormDataScraping() {
  is_heavy_form_data_scraping_enabled_ = true;
}

void AutofillAgent::SetFieldsEligibleForManualFilling(
    const std::vector<FieldRendererId>& fields) {
  if (!form_cache_) {
    return;
  }
  form_cache_->SetFieldsEligibleForManualFilling(fields);
}

void AutofillAgent::GetPotentialLastFourCombinationsForStandaloneCvc(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  if (!unsafe_render_frame()) {
    std::vector<std::string> matches;
    std::move(potential_matches).Run(matches);
  } else {
    WebDocument document = unsafe_render_frame()->GetWebFrame()->GetDocument();
    TraverseDomForFourDigitCombinations(document, std::move(potential_matches));
  }
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  DCHECK(!element.DynamicTo<WebInputElement>().IsNull() ||
         form_util::IsTextAreaElement(element));

  FormData form;
  FormFieldData field;
  if (!FindFormAndFieldForFormControlElement(
          element, field_data_manager(),
          MaybeExtractDatalist({ExtractOption::kBounds}), &form, &field)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(
        form_util::GetOwningForm(element), element, nullptr,
        MaybeExtractDatalist({ExtractOption::kValue, ExtractOption::kBounds}),
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
      form_util::GetDataListSuggestions(input_element, &field.datalist_options);
    }
  }

  is_popup_possibly_visible_ = true;
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->AskForValuesToFill(form, field, field.bounds,
                                        trigger_source);
  }
}

void AutofillAgent::DoFillFieldWithValue(std::u16string_view value,
                                         blink::WebFormControlElement& element,
                                         WebAutofillState autofill_state) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  form_tracker_->set_ignore_control_changes(true);

  element.SetAutofillValue(blink::WebString::FromUTF16(value), autofill_state);

  UpdateStateForTextChange(element,
                           autofill_state == WebAutofillState::kAutofilled
                               ? FieldPropertiesFlags::kAutofilled
                               : FieldPropertiesFlags::kUserTyped);

  form_tracker_->set_ignore_control_changes(false);
}

void AutofillAgent::TriggerFormExtraction() {
  ExtractForms(process_forms_form_extraction_timer_, /*callback=*/{});
}

void AutofillAgent::TriggerFormExtractionWithResponse(
    base::OnceCallback<void(bool)> callback) {
  ExtractForms(process_forms_form_extraction_with_response_timer_,
               std::move(callback));
}

void AutofillAgent::ExtractForm(
    FormRendererId form_id,
    base::OnceCallback<void(const std::optional<FormData>&)> callback) {
  if (!unsafe_render_frame()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  DenseSet<ExtractOption> extract_options =
      MaybeExtractDatalist({ExtractOption::kBounds, ExtractOption::kOptions,
                            ExtractOption::kOptionText, ExtractOption::kValue});
  if (!form_id) {
    if (std::optional<FormData> form =
            CollectFormlessElements(extract_options)) {
      std::move(callback).Run(std::move(form));
      return;
    }
  }
  WebDocument doc = unsafe_render_frame()->GetWebFrame()->GetDocument();
  if (WebFormElement fe = FindFormByRendererId(doc, form_id); !fe.IsNull()) {
    FormData form;
    if (WebFormElementToFormData(fe, WebFormControlElement(),
                                 field_data_manager(), extract_options, &form,
                                 nullptr)) {
      std::move(callback).Run(std::move(form));
      return;
    }
  }
  if (base::FeatureList::IsEnabled(features::kAutofillContentEditables)) {
    FieldRendererId field(*form_id);
    if (WebElement ce = FindContentEditableByRendererId(field); !ce.IsNull()) {
      std::move(callback).Run(FindFormForContentEditable(ce));
      return;
    }
  }
  std::move(callback).Run(std::nullopt);
}

std::vector<blink::WebAutofillClient::FormIssue>
AutofillAgent::ProccessFormsAndReturnIssues() {
  // TODO(crbug.com/1399414,crbug.com/1444566): Throttle this call if possible.
  ExtractFormsUnthrottled(/*callback=*/{});
  return {};
}

void AutofillAgent::ExtractForms(base::OneShotTimer& timer,
                                 base::OnceCallback<void(bool)> callback) {
  static constexpr base::TimeDelta kThrottle = base::Milliseconds(100);
  if (!is_dom_content_loaded_ || timer.IsRunning()) {
    if (!callback.is_null()) {
      std::move(callback).Run(/*success=*/false);
    }
    return;
  }
  timer.Start(FROM_HERE, kThrottle,
              base::BindOnce(&AutofillAgent::ExtractFormsUnthrottled,
                             base::Unretained(this), std::move(callback)));
}

void AutofillAgent::ExtractFormsForPasswordAutofillAgent(
    base::OneShotTimer& timer) {
  static constexpr base::TimeDelta kThrottle = base::Milliseconds(100);
  if (!is_dom_content_loaded_ || timer.IsRunning()) {
    return;
  }
  timer.Start(
      FROM_HERE, kThrottle,
      base::BindOnce(
          &AutofillAgent::ExtractFormsUnthrottled, base::Unretained(this),
          base::BindOnce(
              [](PasswordAutofillAgent* password_autofill_agent, bool success) {
                if (success) {
                  password_autofill_agent->OnDynamicFormsSeen();
                }
              },
              base::Unretained(password_autofill_agent_.get()))));
}

void AutofillAgent::ExtractFormsUnthrottled(
    base::OnceCallback<void(bool)> callback) {
  if (!form_cache_) {
    return;
  }
  FormCache::UpdateFormCacheResult cache =
      form_cache_->UpdateFormCache(field_data_manager());
  content::RenderFrame* render_frame = unsafe_render_frame();
  if (render_frame) {
    form_issues::MaybeEmitFormIssuesToDevtools(*render_frame->GetWebFrame(),
                                               cache.updated_forms);
  }
  if (!cache.updated_forms.empty() || !cache.removed_forms.empty()) {
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FormsSeen(cache.updated_forms,
                                 std::move(cache.removed_forms).extract());
    }
  }
  if (!callback.is_null()) {
    std::move(callback).Run(/*success=*/true);
  }
}

void AutofillAgent::HidePopup() {
  if (!is_popup_possibly_visible_) {
    return;
  }
  is_popup_possibly_visible_ = false;

  // The keyboard accessory has a separate, more complex hiding logic.
  if (IsKeyboardAccessoryEnabled()) {
    return;
  }

  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->HidePopup();
  }
}

void AutofillAgent::DidChangeFormRelatedElementDynamically(
    const WebElement& element,
    blink::WebFormRelatedChangeType form_related_change) {
  if (form_related_change == blink::WebFormRelatedChangeType::kHide) {
    form_tracker_->ElementDisappeared(element);
    return;
  }
  if (form_related_change == blink::WebFormRelatedChangeType::kRemove &&
      !base::FeatureList::IsEnabled(
          features::kAutofillDetectRemovedFormControls)) {
    return;
  }
  // If the control flow is here than the document was at least loaded. The
  // whole page doesn't have to be loaded.
  ExtractFormsForPasswordAutofillAgent(
      process_forms_after_dynamic_change_timer_);
}

void AutofillAgent::DidCompleteFocusChangeInFrame() {
  if (!unsafe_render_frame()) {
    return;
  }
  WebDocument doc = unsafe_render_frame()->GetWebFrame()->GetDocument();
  WebElement focused_element;
  if (!doc.IsNull()) {
    focused_element = doc.FocusedElement();
  }

  if (!focused_element.IsNull()) {
    SendFocusedInputChangedNotificationToBrowser(focused_element);
  }

  if (!IsKeyboardAccessoryEnabled() && focus_requires_scroll_) {
    HandleFocusChangeComplete(
        /*focused_node_was_last_clicked=*/
        last_left_mouse_down_or_gesture_tap_in_node_caused_focus_);
  }
  last_left_mouse_down_or_gesture_tap_in_node_caused_focus_ = false;

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::DidReceiveLeftMouseDownOrGestureTapInNode(
    const WebNode& node) {
  DCHECK(!node.IsNull());
#if defined(ANDROID)
  HandleFocusChangeComplete(/*focused_node_was_last_clicked=*/node.Focused());
#else
  last_left_mouse_down_or_gesture_tap_in_node_caused_focus_ = node.Focused();
#endif
}

void AutofillAgent::SelectControlDidChange(
    const WebFormControlElement& element) {
  form_tracker_->SelectControlDidChange(element);
}

// Notifies the AutofillDriver about changes in the <select> or <selectlist>
// options in batches.
//
// A batch ends if no event occurred for `kWaitTimeForOptionsChangesMs`
// milliseconds. For a given batch, the AutofillDriver is informed only about
// the last FormData. That is, if within one batch the options of different
// forms changed, all but one of these events will be lost.
void AutofillAgent::SelectOrSelectListFieldOptionsChanged(
    const blink::WebFormControlElement& element) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (!was_last_action_fill_ || last_queried_element_.GetField().IsNull()) {
    return;
  }

  if (select_or_selectlist_option_change_batch_timer_.IsRunning()) {
    select_or_selectlist_option_change_batch_timer_.AbandonAndStop();
  }

  select_or_selectlist_option_change_batch_timer_.Start(
      FROM_HERE, base::Milliseconds(kWaitTimeForOptionsChangesMs),
      base::BindRepeating(&AutofillAgent::BatchSelectOrSelectListOptionChange,
                          weak_ptr_factory_.GetWeakPtr(), element));
}

void AutofillAgent::BatchSelectOrSelectListOptionChange(
    const blink::WebFormControlElement& element) {
  if (element.GetDocument().IsNull()) {
    return;
  }

  // Look for the form and field associated with the select element. If they are
  // found, notify the driver that the form was modified dynamically.
  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(element, field_data_manager(),
                                            /*extract_options=*/{}, &form,
                                            &field) &&
      !field.options.empty()) {
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->SelectOrSelectListFieldOptionsDidChange(form);
    }
  }
}

bool AutofillAgent::ShouldSuppressKeyboard(
    const WebFormControlElement& element) {
  // Note: Consider supporting other autofill types in the future as well.
#if BUILDFLAG(IS_ANDROID)
  if (password_autofill_agent_->ShouldSuppressKeyboard()) {
    return true;
  }
#endif
  return false;
}

void AutofillAgent::FormElementReset(const WebFormElement& form) {
  DCHECK(MaybeWasOwnedByFrame(form, unsafe_render_frame()));
  password_autofill_agent_->InformAboutFormClearing(form);
}

void AutofillAgent::PasswordFieldReset(const WebInputElement& element) {
  DCHECK(MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  password_autofill_agent_->InformAboutFieldClearing(element);
}

bool AutofillAgent::IsPrerendering() const {
  return unsafe_render_frame() &&
         unsafe_render_frame()->GetWebFrame()->GetDocument().IsPrerendering();
}

void AutofillAgent::FormControlElementClicked(
    const WebFormControlElement& element) {
  last_clicked_form_control_element_for_testing_ =
      form_util::GetFieldRendererId(element);
  was_last_action_fill_ = false;

  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  if (input_element.IsNull() && !form_util::IsTextAreaElement(element)) {
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordSuggestionBottomSheetV2)) {
    password_autofill_agent_->TryToShowKeyboardReplacingSurface(element);
  }
#endif

  ShowSuggestions(element,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked);

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::HandleFocusChangeComplete(
    bool focused_node_was_last_clicked) {
  if (!unsafe_render_frame()) {
    return;
  }

  // When using Talkback on Android, and possibly others, traversing to and
  // focusing a field will not register as a click. Thus, when screen readers
  // are used, treat the focused node as if it was the last clicked. Also check
  // to ensure focus is on a field where text can be entered.
  // When the focus is on a non-input field on Android, keyboard accessory may
  // be shown if autofill data is available. Make sure to hide the accessory if
  // focus changes to another element.
  focused_node_was_last_clicked |= is_screen_reader_enabled_;

  WebElement focused_element =
      unsafe_render_frame()->GetWebFrame()->GetDocument().FocusedElement();
  if (focused_node_was_last_clicked && !focused_element.IsNull() &&
      focused_element.IsFormControlElement()) {
    WebFormControlElement focused_form_control_element =
        focused_element.To<WebFormControlElement>();
    if (form_util::IsTextAreaElementOrTextInput(focused_form_control_element)) {
      FormControlElementClicked(focused_form_control_element);
    }
  }

  // TODO(crbug.com/1490372, b/308811729): This is not conditioned on
  // `focused_node_was_last_clicked`. This has two advantages:
  // - The AutofillDriverRouter is informed about the form and then can route
  //   ExtractForm() messages to the right frame.
  //   TODO(crbug.com/1490372, b/308811729): Check whether the context menu
  //   event arrives after this one. (Probably it does, analogously to the IME.)
  // - On click into a nested contenteditable `focused_node_was_last_clicked` is
  //   false. The call comes from DidCompleteFocusChangeInFrame() which passes
  //   false since at the preceding DidReceiveLeftMouseDownOrGestureTapInNode()
  //   call `node.Focused()` was false.
  if (!focused_element.IsNull() &&
      base::FeatureList::IsEnabled(features::kAutofillContentEditables)) {
    if (std::optional<FormData> form =
            form_util::FindFormForContentEditable(focused_element)) {
      CHECK_EQ(form->fields.size(), 1u);
      if (auto* autofill_driver = unsafe_autofill_driver()) {
        autofill_driver->AskForValuesToFill(
            *form, form->fields[0], form->fields[0].bounds,
            mojom::AutofillSuggestionTriggerSource::kContentEditableClicked);
      }
    }
  }

  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::SendFocusedInputChangedNotificationToBrowser(
    const WebElement& node) {
  focus_state_notifier_.FocusedInputChanged(node);
  auto input_element = node.DynamicTo<WebInputElement>();
  if (!input_element.IsNull()) {
    field_data_manager_->UpdateFieldDataMapWithNullValue(
        form_util::GetFieldRendererId(input_element),
        FieldPropertiesFlags::kHadFocus);
  }
}

void AutofillAgent::AjaxSucceeded() {
  form_tracker_->AjaxSucceeded();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::JavaScriptChangedAutofilledValue(
    const blink::WebFormControlElement& element,
    const blink::WebString& old_value) {
  if (old_value == element.Value()) {
    return;
  }
  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(element, field_data_manager(),
                                            /*extract_options=*/{}, &form,
                                            &field)) {
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->JavaScriptChangedAutofilledValue(form, field,
                                                        old_value.Utf16());
    }
  }
}

void AutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form,
    const WebFormControlElement& element,
    ElementChangeSource source) {
  // Updates cached data needed for submission so that we only cache the latest
  // version of the to-be-submitted form.
  auto update_submission_data_on_user_edit = [&]() {
    // If dealing with a form, forward directly to UpdateLastInteractedForm. The
    // remaining logic deals with formless fields.
    if (!form.IsNull()) {
      UpdateLastInteractedForm(form);
      return;
    }
    if (!unsafe_render_frame()) {
      return;
    }
    // Remove visible elements.
    // TODO(crbug.com/1483242): Investigate if this is necessary: if it is,
    // document the reason, if not, remove.
    WebDocument doc = unsafe_render_frame()->GetWebFrame()->GetDocument();
    if (!doc.IsNull()) {
      std::erase_if(formless_elements_user_edited_,
                    [&doc](const FieldRendererId field_id) {
                      WebFormControlElement field =
                          form_util::FindFormControlByRendererId(
                              doc, field_id,
                              /*form_to_be_searched =*/FormRendererId());
                      return !field.IsNull() &&
                             form_util::IsWebElementFocusableForAutofill(field);
                    });
    }
    // Update provisionally_saved_form_. Afterwards, only keep one of
    // `provisionally_saved_form_` or `last_interacted_form_` non-null to avoid
    // tracking different and outdated elements.
    formless_elements_user_edited_.insert(
        form_util::GetFieldRendererId(element));
    provisionally_saved_form_ = CollectFormlessElements();
    // TODO(crbug.com/1483242): Investigate why don't we reset
    // `last_interacted_form_` except when formless extraction fails, document
    // the reason if any, cleanup otherwise.
    if (provisionally_saved_form_) {
      last_interacted_form_ = {};
    }
  };

  switch (source) {
    case FormTracker::Observer::ElementChangeSource::WILL_SEND_SUBMIT_EVENT:
      // Fire the form submission event to avoid missing submissions where
      // websites handle the onsubmit event. This also gets the form before
      // Javascript's submit event handler could change it. We don't clear
      // submitted_forms_ because OnFormSubmitted will normally be invoked
      // afterwards and we don't want to fire the same event twice.
      FireHostSubmitEvents(form, /*known_success=*/false,
                           SubmissionSource::FORM_SUBMISSION);
      ResetLastInteractedElements();
      break;
    case FormTracker::Observer::ElementChangeSource::TEXTFIELD_CHANGED:
      update_submission_data_on_user_edit();
      OnTextFieldDidChange(element);
      break;
    case FormTracker::Observer::ElementChangeSource::SELECT_CHANGED:
      update_submission_data_on_user_edit();
      // Signal the browser of change in select fields.
      // TODO(crbug.com/1483242): Investigate if this is necessary: if it is,
      // document the reason, if not, remove.
      FormData form_data;
      FormFieldData field;
      if (auto* autofill_driver = unsafe_autofill_driver();
          autofill_driver && FindFormAndFieldForFormControlElement(
                                 element, field_data_manager(),
                                 MaybeExtractDatalist({ExtractOption::kBounds}),
                                 &form_data, &field)) {
        autofill_driver->SelectControlDidChange(form_data, field, field.bounds);
      }
      break;
  }
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnProbablyFormSubmitted() {
  if (std::optional<FormData> form_data = GetSubmittedForm()) {
    FireHostSubmitEvents(form_data.value(), /*known_success=*/false,
                         SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  DCHECK(MaybeWasOwnedByFrame(form, unsafe_render_frame()));
  // Fire the submission event here because WILL_SEND_SUBMIT_EVENT is skipped
  // if javascript calls submit() directly.
  FireHostSubmitEvents(form, /*known_success=*/false,
                       SubmissionSource::FORM_SUBMISSION);
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::OnInferredFormSubmission(SubmissionSource source) {
  if (!unsafe_render_frame()) {
    return;
  }
  switch (source) {
    // This source is only used as a default values to variables.
    case mojom::SubmissionSource::NONE:
    // This source is handled by `AutofillAgent::OnFormSubmitted`.
    case mojom::SubmissionSource::FORM_SUBMISSION:
    // This source is handled by `AutofillAgent::OnProbablyFormSubmitted`.
    case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
      NOTREACHED_NORETURN();
    case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      // TODO(crbug.com/1483242): Investigate if discarding subframe same
      // document navigation is necessary: if it is, document the reason, if
      // not, remove.
      if (!unsafe_render_frame()->GetWebFrame()->IsOutermostMainFrame()) {
        break;
      }
      if (std::optional<FormData> form_data = GetSubmittedForm()) {
        FireHostSubmitEvents(*form_data, /*known_success=*/true, source);
      }
      break;
    // This event occurs only when either this frame or a same process parent
    // frame of it gets detached.
    case mojom::SubmissionSource::FRAME_DETACHED:
      // Detaching the main frame means that navigation happened or the current
      // tab was closed, both reasons being too general to be able to deduce
      // submission from it (and the relevant use cases will most probably be
      // handled by other sources), therefore we only consider detached
      // subframes.
      if (!unsafe_render_frame()->GetWebFrame()->IsOutermostMainFrame() &&
          provisionally_saved_form_.has_value()) {
        // Should not access the frame because it is now detached. Instead, use
        // |provisionally_saved_form_|.
        FireHostSubmitEvents(provisionally_saved_form_.value(),
                             /*known_success=*/true, source);
      }
      break;
    case mojom::SubmissionSource::XHR_SUCCEEDED:
    case mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR:
      if (std::optional<FormData> form_data = GetSubmittedForm()) {
        FireHostSubmitEvents(*form_data, /*known_success=*/true, source);
      }
      break;
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  SendPotentiallySubmittedFormToBrowser();
}

void AutofillAgent::AddFormObserver(Observer* observer) {
  form_tracker_->AddObserver(observer);
}

void AutofillAgent::RemoveFormObserver(Observer* observer) {
  form_tracker_->RemoveObserver(observer);
}

void AutofillAgent::TrackAutofilledElement(
    const blink::WebFormControlElement& element) {
  form_tracker_->TrackAutofilledElement(element);
}

void AutofillAgent::UpdateStateForTextChange(
    const WebFormControlElement& element,
    FieldPropertiesFlags flag) {
  const auto input_element = element.DynamicTo<WebInputElement>();
  if (input_element.IsNull() || !input_element.IsTextField()) {
    return;
  }

  field_data_manager_->UpdateFieldDataMap(
      form_util::GetFieldRendererId(element), element.Value().Utf16(), flag);

  password_autofill_agent_->UpdatePasswordStateForTextChange(input_element);
}

std::optional<FormData> AutofillAgent::GetSubmittedForm() const {
  // Checks whether all elements represented by `element_ids` in `document` have
  // disappeared (removed/hidden).
  // TODO(crbug.com/1427131): Remove document parameter after launching
  // AutofillUseDomNodeIdForRendererId.
  auto all_control_elements_disappeared =
      [](const blink::WebDocument& document,
         const std::set<FieldRendererId>& element_ids) {
        std::vector<FieldRendererId> elements(element_ids.begin(),
                                              element_ids.end());
        return base::ranges::none_of(
            form_util::FindFormControlsByRendererId(document, elements),
            form_util::IsWebElementFocusableForAutofill);
      };

  // We check if we have a cached `last_interacted_form_`. In that case we
  // return either the extracted form or `provisionally_saved_form_` as a
  // fallback if extraction fails. The remaining logic deals with formless
  // fields.
  // The reason why we check the ID and not the form is that we might've been
  // caching a form that was removed, hence the form will be null but the ID
  // won't.
  if (last_interacted_form_.GetId()) {
    if (std::optional<FormData> form = form_util::ExtractFormData(
            last_interacted_form_.GetForm(), field_data_manager())) {
      return form;
    }
    return provisionally_saved_form_;
  }
  // Criteria to decide on the submission of the form of formless elements,
  // assuming submission has been inferred:
  // - Formless elements were autofilled.
  // - The user has edited formless elements and all those elements disappeared
  //   (removed/hidden).
  // TODO(crbug.com/1427131): Remove render_frame condition after launching
  // AutofillUseDomNodeIdForRendererId.
  if (auto* render_frame = unsafe_render_frame();
      formless_elements_were_autofilled_ ||
      (render_frame && !formless_elements_user_edited_.empty() &&
       all_control_elements_disappeared(
           render_frame->GetWebFrame()->GetDocument(),
           formless_elements_user_edited_))) {
    // Return the extracted form or `provisionally_saved_form_` as a fallback if
    // extraction fails.
    if (std::optional<FormData> form = CollectFormlessElements()) {
      return form;
    }
    return provisionally_saved_form_;
  }
  return std::nullopt;
}

void AutofillAgent::SendPotentiallySubmittedFormToBrowser() {
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->SetFormToBeProbablySubmitted(GetSubmittedForm());
  }
}

void AutofillAgent::ResetLastInteractedElements() {
  last_interacted_form_ = {};
  last_clicked_form_control_element_for_testing_ = {};
  formless_elements_user_edited_.clear();
  formless_elements_were_autofilled_ = false;
  provisionally_saved_form_.reset();
}

void AutofillAgent::UpdateLastInteractedForm(
    const blink::WebFormElement& form) {
  DCHECK(MaybeWasOwnedByFrame(form, unsafe_render_frame()));

  last_interacted_form_ = FormRef(form);
  provisionally_saved_form_ = form_util::ExtractFormData(
      last_interacted_form_.GetForm(), field_data_manager());
}

void AutofillAgent::OnFormNoLongerSubmittable() {
  submitted_forms_.clear();
}

mojom::AutofillDriver* AutofillAgent::unsafe_autofill_driver() {
  if (IsPrerendering()) {
    if (!deferring_autofill_driver_) {
      deferring_autofill_driver_ =
          std::make_unique<DeferringAutofillDriver>(this);
    }
    return deferring_autofill_driver_.get();
  }

  // Lazily bind this interface.
  if (unsafe_render_frame() && !autofill_driver_) {
    unsafe_render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_driver_);
  }
  return autofill_driver_.get();
}

mojom::PasswordManagerDriver& AutofillAgent::GetPasswordManagerDriver() {
  DCHECK(password_autofill_agent_);
  return password_autofill_agent_->GetPasswordManagerDriver();
}

}  // namespace autofill
