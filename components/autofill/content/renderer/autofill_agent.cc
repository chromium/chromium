// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
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
#include "components/autofill/content/renderer/suggestion_properties.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_form_related_change_type.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_range.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebAutofillClient;
using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFormRelatedChangeType;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebRange;
using blink::WebString;

namespace autofill {

namespace {

constexpr char kSubmissionSourceHistogram[] =
    "Autofill.SubmissionDetectionSource.AutofillAgent";

// Time to wait in ms to ensure that only a single select or datalist change
// will be acted upon, instead of multiple in close succession (debounce time).
constexpr base::TimeDelta kWaitTimeForOptionsChanges = base::Milliseconds(50);

using FormAndField = std::pair<FormData, FormFieldData>;

// Compare the values before and after JavaScript value changes after:
// - Converting to lower case.
// - Removing special characters
// - Removing whitespaces.
// If values are equal after this comparison, we claim that the modification
// was not big enough to drop the autofilled state of the field.
bool JavaScriptOnlyReformattedValue(std::u16string old_value,
                                    std::u16string new_value) {
  static constexpr char16_t kSpecialChars[] =
      uR"(`~!@#$%^&*()-_=+[]{}\|;:'",.<>/?)";
  static const base::NoDestructor<std::u16string> removable(
      base::StrCat({kSpecialChars, base::kWhitespaceUTF16}));
  base::RemoveChars(base::i18n::ToLower(old_value), *removable, &old_value);
  base::RemoveChars(base::i18n::ToLower(new_value), *removable, &new_value);
  // This normalization is a best effort approach that might not be prefect
  // across all use cases of JavaScript formatting a value (e.g. for
  // normalizing single-byte and double-byte encoding of digits in Japan, an
  // NKFC normalization may be appropriate).
  // TODO(b/40947225): Internationalize this normalization.
  return old_value == new_value;
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
                          base::TimeTicks timestamp) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldDidChange, form, field,
             timestamp);
  }
  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldDidScroll, form, field);
  }
  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field) override {
    DeferMsg(&mojom::AutofillDriver::SelectControlDidChange, form, field);
  }
  void SelectOrSelectListFieldOptionsDidChange(const FormData& form) override {
    DeferMsg(&mojom::AutofillDriver::SelectOrSelectListFieldOptionsDidChange,
             form);
  }
  void AskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source) override {
    DeferMsg(&mojom::AutofillDriver::AskForValuesToFill, form, field,
             trigger_source);
  }
  void HidePopup() override { DeferMsg(&mojom::AutofillDriver::HidePopup); }
  void FocusOnNonFormField(bool had_interacted_form) override {
    DeferMsg(&mojom::AutofillDriver::FocusOnNonFormField, had_interacted_form);
  }
  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field) override {
    DeferMsg(&mojom::AutofillDriver::FocusOnFormField, form, field);
  }
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {
    DeferMsg(&mojom::AutofillDriver::DidFillAutofillFormData, form, timestamp);
  }
  void DidEndTextFieldEditing() override {
    DeferMsg(&mojom::AutofillDriver::DidEndTextFieldEditing);
  }
  void JavaScriptChangedAutofilledValue(const FormData& form,
                                        const FormFieldData& field,
                                        const std::u16string& old_value,
                                        bool formatting_only) override {
    DeferMsg(&mojom::AutofillDriver::JavaScriptChangedAutofilledValue, form,
             field, old_value, formatting_only);
  }

  const raw_ref<AutofillAgent> agent_;
  base::WeakPtrFactory<DeferringAutofillDriver> weak_ptr_factory_{this};
};

AutofillAgent::FocusStateNotifier::FocusStateNotifier(AutofillAgent* agent)
    : agent_(CHECK_DEREF(agent)) {}

AutofillAgent::FocusStateNotifier::~FocusStateNotifier() = default;

void AutofillAgent::FocusStateNotifier::FocusedInputChanged(
    const WebNode& node) {
  CHECK(!node.IsNull());
  mojom::FocusedFieldType new_focused_field_type =
      mojom::FocusedFieldType::kUnknown;
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
  mojom::FocusedFieldType new_focused_field_type =
      mojom::FocusedFieldType::kUnknown;
  NotifyIfChanged(new_focused_field_type, new_focused_field_id);
}

mojom::FocusedFieldType AutofillAgent::FocusStateNotifier::GetFieldType(
    const WebFormControlElement& node) {
  if (form_util::IsTextAreaElement(node.To<WebFormControlElement>())) {
    return mojom::FocusedFieldType::kFillableTextArea;
  }

  WebInputElement input_element = node.DynamicTo<WebInputElement>();
  if (input_element.IsNull() || !input_element.IsTextField() ||
      !form_util::IsElementEditable(input_element)) {
    return mojom::FocusedFieldType::kUnfillableElement;
  }

  if (input_element.FormControlTypeForAutofill() ==
      blink::mojom::FormControlType::kInputSearch) {
    return mojom::FocusedFieldType::kFillableSearchField;
  }
  if (input_element.IsPasswordFieldForAutofill()) {
    return mojom::FocusedFieldType::kFillablePasswordField;
  }
  if (agent_->password_autofill_agent_->IsUsernameInputField(input_element)) {
    return mojom::FocusedFieldType::kFillableUsernameField;
  }
  if (form_util::IsWebauthnTaggedElement(node)) {
    return mojom::FocusedFieldType::kFillableWebauthnTaggedField;
  }
  return mojom::FocusedFieldType::kFillableNonSearchField;
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

  // TODO(crbug.com/40260756): Move FocusedInputChanged to AutofillDriver.
  agent_->GetPasswordManagerDriver().FocusedInputChanged(
      new_focused_field_id, new_focused_field_type);

  focused_field_type_ = new_focused_field_type;
  focused_field_id_ = new_focused_field_id;
}

AutofillAgent::AutofillAgent(
    content::RenderFrame* render_frame,
    Config config,
    std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
    std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      config_(config),
      form_cache_(std::make_unique<FormCache>(render_frame->GetWebFrame())),
      password_autofill_agent_(std::move(password_autofill_agent)),
      password_generation_agent_(std::move(password_generation_agent)) {
  render_frame->GetWebFrame()->SetAutofillClient(this);
  password_autofill_agent_->Init(this);
  AddFormObserver(this);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    AddFormObserver(password_autofill_agent_.get());
  }
  registry->AddInterface<mojom::AutofillAgent>(base::BindRepeating(
      &AutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

// The destructor is not guaranteed to be called. Destruction happens (only)
// through the OnDestruct() event, which posts a task to delete this object.
// The process may be killed before this deletion can happen.
AutofillAgent::~AutofillAgent() {
  RemoveFormObserver(this);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    RemoveFormObserver(password_autofill_agent_.get());
  }
}

void AutofillAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAgent> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void AutofillAgent::DidCommitProvisionalLoad(ui::PageTransition transition) {
  Reset();
}

void AutofillAgent::DidCreateDocumentElement() {
  // Some navigations seem not to call DidCommitProvisionalLoad()
  // (crbug.com/328161303).
  Reset();
}

void AutofillAgent::Reset() {
  // Navigation to a new page or a page refresh.
  last_queried_element_ = {};
  form_cache_ =
      unsafe_render_frame()
          ? std::make_unique<FormCache>(unsafe_render_frame()->GetWebFrame())
          : nullptr;
  is_dom_content_loaded_ = false;
  select_or_selectlist_option_change_batch_timer_.Stop();
  datalist_option_change_batch_timer_.Stop();
  process_forms_after_dynamic_change_timer_.Stop();
  process_forms_form_extraction_timer_.Stop();
  process_forms_form_extraction_with_response_timer_.Stop();
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
}

void AutofillAgent::DidDispatchDOMContentLoadedEvent() {
  is_dom_content_loaded_ = true;
  ExtractFormsUnthrottled(/*callback=*/{});
}

void AutofillAgent::DidChangeScrollOffset() {
  if (!config_.focus_requires_scroll) {
    // Post a task here since scroll offset may change during layout.
    // TODO(crbug.com/40559425): Do not cancel other tasks and do not invalidate
    // PasswordAutofillAgent::autofill_agent_.
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (auto* render_frame = unsafe_render_frame()) {
      render_frame->GetTaskRunner(blink::TaskType::kInternalUserInteraction)
          ->PostTask(FROM_HERE,
                     base::BindOnce(&AutofillAgent::DidChangeScrollOffsetImpl,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    last_queried_element_.GetId()));
    }
  } else {
    HidePopup();
  }
}

void AutofillAgent::DidChangeScrollOffsetImpl(FieldRendererId element_id) {
  WebFormControlElement element =
      form_util::GetFormControlByRendererId(element_id);
  if (element != last_queried_element_.GetField() || element.IsNull() ||
      config_.focus_requires_scroll || !is_popup_possibly_visible_ ||
      !element.Focused()) {
    return;
  }

  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (std::optional<FormAndField> form_and_field =
          FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidScroll(form, field);
    }
  }

  // Ignore subsequent scroll offset changes.
  HidePopup();
}

void AutofillAgent::FocusedElementChangedDeprecated(const WebElement& element) {
  CHECK(!base::FeatureList::IsEnabled(features::kAutofillNewFocusEvents));
  HidePopup();

  WebFormElement last_focused_form = last_interacted_form().GetForm();
  if (element.IsNull()) {
    // Focus moved away from the last interacted form (if any) to somewhere else
    // on the page.
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FocusOnNonFormField(!last_focused_form.IsNull());
    }
    return;
  }

  const WebFormControlElement form_control_element =
      element.DynamicTo<WebFormControlElement>();

  bool focus_moved_to_new_form = false;
  if (!last_focused_form.IsNull() &&
      (form_control_element.IsNull() ||
       last_focused_form != form_control_element.Form())) {
    // The focused element is not part of the last interacted form (could be
    // in a different form).
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FocusOnNonFormField(/*had_interacted_form=*/true);
    }
    focus_moved_to_new_form = true;
  }

  // Calls HandleFocusChangeComplete() after notifying the focus is no longer on
  // the previous form, then early return. No need to notify the newly focused
  // element because that will be done by HandleFocusChangeComplete().
  // Refer to http://crbug.com/1105254
  if ((config_.uses_keyboard_accessory_for_suggestions ||
       !config_.focus_requires_scroll) &&
      !element.IsNull() &&
      element.GetDocument().GetFrame()->HasTransientUserActivation()) {
    // If the focus change was caused by a user gesture,
    // DidReceiveLeftMouseDownOrGestureTapInNode() will show the autofill
    // suggestions. See crbug.com/730764 for why showing autofill suggestions as
    // a result of JavaScript changing focus is enabled on WebView.
    bool focused_node_was_last_clicked =
        !base::FeatureList::IsEnabled(
            features::kAutofillAndroidDisableSuggestionsOnJSFocus) ||
        !config_.focus_requires_scroll;
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

  if (form_control_element.IsReadOnly()) {
    return;
  }
  if (std::optional<FormAndField> form_and_field =
          FindFormAndFieldForFormControlElement(
              last_queried_element_.GetField(), field_data_manager(),
              MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FocusOnFormField(form, field);
    }
  }
}

void AutofillAgent::FocusedElementChanged(
    const WebElement& new_focused_element) {
  if (!base::FeatureList::IsEnabled(features::kAutofillNewFocusEvents)) {
    FocusedElementChangedDeprecated(new_focused_element);
    return;
  }

  HidePopup();

  // This behavior was introduced for to fix http://crbug.com/1105254. It's
  // unclear if this is still needed.
  auto handle_focus_change = [&]() {
    if ((config_.uses_keyboard_accessory_for_suggestions ||
         !config_.focus_requires_scroll) &&
        !new_focused_element.IsNull() &&
        new_focused_element.GetDocument()
            .GetFrame()
            ->HasTransientUserActivation()) {
      // If the focus change was caused by a user gesture,
      // DidReceiveLeftMouseDownOrGestureTapInNode() will show the autofill
      // suggestions. See crbug.com/730764 for why showing autofill suggestions
      // as a result of JavaScript changing focus is enabled on WebView.
      bool focused_node_was_last_clicked =
          !base::FeatureList::IsEnabled(
              features::kAutofillAndroidDisableSuggestionsOnJSFocus) ||
          !config_.focus_requires_scroll;
      HandleFocusChangeComplete(
          /*focused_node_was_last_clicked=*/focused_node_was_last_clicked);
    }
  };

  if (auto control = new_focused_element.DynamicTo<WebFormControlElement>();
      !control.IsNull()) {
    if (std::optional<FormAndField> form_and_field =
            FindFormAndFieldForFormControlElement(
                control, field_data_manager(),
                MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
      auto& [form, field] = *form_and_field;
      if (auto* autofill_driver = unsafe_autofill_driver()) {
        last_queried_element_ = FieldRef(control);
        autofill_driver->FocusOnFormField(form, field);
        handle_focus_change();
        return;
      }
    }
  }

  if (!new_focused_element.IsNull() &&
      new_focused_element.IsContentEditable()) {
    if (std::optional<FormData> form =
            form_util::FindFormForContentEditable(new_focused_element)) {
      CHECK_EQ(form->fields.size(), 1u);
      if (auto* autofill_driver = unsafe_autofill_driver()) {
        last_queried_element_ = FieldRef(new_focused_element);
        autofill_driver->FocusOnFormField(*form, form->fields.front());
        handle_focus_change();
        return;
      }
    }
  }

  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->FocusOnNonFormField(true);
    handle_focus_change();
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

void AutofillAgent::FireHostSubmitEvents(const FormData& form_data,
                                         bool known_success,
                                         mojom::SubmissionSource source) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    password_autofill_agent_->FireHostSubmitEvent(form_data.renderer_id,
                                                  source);
  }
  // We don't want to fire duplicate submission event.
  if (!submitted_forms_.insert(form_data.renderer_id).second) {
    return;
  }
  base::UmaHistogramEnumeration(kSubmissionSourceHistogram, source);
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->FormSubmitted(form_data, known_success, source);
  }
}

void AutofillAgent::TextFieldDidEndEditing(const WebInputElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

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
}

void AutofillAgent::TextFieldDidChange(const WebFormControlElement& element) {
  form_tracker_->TextFieldDidChange(element);
}

void AutofillAgent::ContentEditableDidChange(const WebElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  if (!base::FeatureList::IsEnabled(
          features::kAutofillContentEditableChangeEvents)) {
    return;
  }
  // TODO(crbug.com/40286232): Add throttling to avoid sending this event for
  // rapid changes.
  if (std::optional<FormData> form =
          form_util::FindFormForContentEditable(element)) {
    CHECK_EQ(form->fields.size(), 1u);
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidChange(*form, form->fields.front(),
                                          base::TimeTicks::Now());
    }
  }
}

void AutofillAgent::OnTextFieldDidChange(const WebFormControlElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  // TODO(crbug.com/40286232): Add throttling to avoid sending this event for
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

  if (std::optional<FormAndField> form_and_field =
          FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidChange(form, field, base::TimeTicks::Now());
    }
  }
}

void AutofillAgent::TextFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (event.windows_key_code == ui::VKEY_DOWN ||
      event.windows_key_code == ui::VKEY_UP) {
    ShowSuggestions(
        element, AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown);
  }
}

void AutofillAgent::OpenTextDataListChooser(const WebInputElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  ShowSuggestions(element,
                  AutofillSuggestionTriggerSource::kOpenTextDataListChooser);
}

// Notifies the AutofillDriver about changes in the <datalist> options in
// batches.
//
// A batch ends if no event occurred for `kWaitTimeForOptionsChanges`.
// For a given batch, the AutofillDriver is informed only about the last field.
// That is, if within one batch the options of different fields changed, all but
// one of these events will be lost.
void AutofillAgent::DataListOptionsChanged(const WebInputElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (element.GetDocument().IsNull() || !is_popup_possibly_visible_ ||
      !element.Focused()) {
    return;
  }

  if (datalist_option_change_batch_timer_.IsRunning()) {
    datalist_option_change_batch_timer_.AbandonAndStop();
  }

  datalist_option_change_batch_timer_.Start(
      FROM_HERE, kWaitTimeForOptionsChanges,
      base::BindRepeating(&AutofillAgent::BatchDataListOptionChange,
                          base::Unretained(this),
                          form_util::GetFieldRendererId(element)));
}

void AutofillAgent::BatchDataListOptionChange(FieldRendererId element_id) {
  WebFormControlElement element =
      form_util::GetFormControlByRendererId(element_id);
  if (element.IsNull() || element.GetDocument().IsNull()) {
    return;
  }

  OnProvisionallySaveForm(element.Form(), element,
                          SaveFormReason::kTextFieldChanged);
}

void AutofillAgent::UserGestureObserved() {
  password_autofill_agent_->UserGestureObserved();
}

// mojom::AutofillAgent:
void AutofillAgent::ApplyFieldsAction(
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    const std::vector<FormFieldData::FillData>& fields) {
  CHECK(!fields.empty());
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
  if (last_queried_element.IsNull() || !last_queried_element.Focused() ||
      !base::Contains(fields,
                      form_util::GetFormRendererId(
                          form_util::GetOwningForm(last_queried_element)),
                      &FormFieldData::FillData::host_form_id)) {
    for (const FormFieldData::FillData& field : fields) {
      last_queried_element =
          form_util::GetFormControlByRendererId(field.renderer_id);
      if (!last_queried_element.IsNull()) {
        last_queried_element_ = FieldRef(last_queried_element);
        break;
      }
    }
  }
  if (last_queried_element.IsNull()) {
    return;
  }
  if (!unsafe_render_frame()) {
    return;
  }

  WebDocument document = unsafe_render_frame()->GetWebFrame()->GetDocument();
  ClearPreviewedForm();
  if (action_persistence == mojom::ActionPersistence::kPreview) {
    previewed_elements_ =
        form_util::ApplyFieldsAction(document, fields, action_type,
                                     action_persistence, field_data_manager());
  } else {
    was_last_action_fill_ = true;

    std::vector<std::pair<FieldRef, WebAutofillState>> filled_fields =
        form_util::ApplyFieldsAction(document, fields, action_type,
                                     action_persistence, field_data_manager());

    // Notify Password Manager of filled fields.
    for (const auto& [filled_field, field_autofill_state] : filled_fields) {
      WebInputElement input_element =
          filled_field.GetField().DynamicTo<WebInputElement>();
      if (!input_element.IsNull()) {
        password_autofill_agent_->UpdatePasswordStateForTextChange(
            input_element);
      }
    }

    if (auto it =
            base::ranges::find_if(fields, std::not_fn(&FormRendererId::is_null),
                                  &FormFieldData::FillData::host_form_id);
        it != fields.end()) {
      UpdateLastInteractedElement(it->host_form_id);
    } else if (!base::FeatureList::IsEnabled(
                   features::kAutofillUnifyAndFixFormTracking)) {
      UpdateLastInteractedElement(FormRendererId());
    } else {
      for (const auto& [field_ref, state] : filled_fields) {
        if (!field_ref.GetField().IsNull()) {
          UpdateLastInteractedElement(field_ref.GetId());
        }
      }
    }

    formless_elements_were_autofilled_ |= base::ranges::any_of(
        filled_fields, [](const std::pair<FieldRef, WebAutofillState>& field) {
          WebFormControlElement element = field.first.GetField();
          return !element.IsNull() &&
                 form_util::GetOwningForm(element).IsNull();
        });

    base::flat_set<FormRendererId> extracted_form_ids;
    std::vector<FormData> filled_forms;
    for (const FormFieldData::FillData& field : fields) {
      if (extracted_form_ids.insert(field.host_form_id).second) {
        std::optional<FormData> form = form_util::ExtractFormData(
            document, form_util::GetFormByRendererId(field.host_form_id),
            *field_data_manager_);
        if (!form) {
          continue;
        }
        filled_forms.push_back(*form);
        if (auto* autofill_driver = unsafe_autofill_driver()) {
          CHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);
          autofill_driver->DidFillAutofillFormData(*form,
                                                   base::TimeTicks::Now());
        }
      }
    }
    if (auto* autofill_driver = unsafe_autofill_driver();
        autofill_driver && !filled_forms.empty()) {
      CHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);
      autofill_driver->FormsSeen(filled_forms, /*removed_forms=*/{});
    }
  }
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

void AutofillAgent::ClearPreviewedForm() {
  WebFormControlElement last_queried_element = last_queried_element_.GetField();
  // TODO(crbug.com/40564702): It is very rare, but it looks like the |element_|
  // can be null if a provisional load was committed immediately prior to
  // clearing the previewed form.
  if (last_queried_element.IsNull()) {
    return;
  }
  // `password_generation_agent_` can be null in WebView.
  // TODO(b/326213028): Clear fields previewed by `PasswordGenerationAgent`
  // directly using `PasswordGenerationAgent`.
  if (password_generation_agent_) {
    password_generation_agent_->ClearPreviewedForm();
  }
  // TODO(b/326213028): Clear fields previewed by `PasswordAutofillAgent`
  // directly using `PasswordAutofillAgent`.
  password_autofill_agent_->ClearPreviewedForm();

  std::vector<std::pair<WebFormControlElement, WebAutofillState>>
      previewed_elements;
  for (const auto& [previewed_element, prior_autofill_state] :
       previewed_elements_) {
    if (WebFormControlElement field = previewed_element.GetField();
        !field.IsNull()) {
      previewed_elements.emplace_back(field, prior_autofill_state);
    }
  }
  form_util::ClearPreviewedElements(previewed_elements, last_queried_element);
  previewed_elements_ = {};
}

void AutofillAgent::TriggerSuggestions(
    FieldRendererId field_id,
    AutofillSuggestionTriggerSource trigger_source) {
  if (WebFormControlElement control_element =
          form_util::GetFormControlByRendererId(field_id);
      !control_element.IsNull()) {
    last_queried_element_ = FieldRef(control_element);
    ShowSuggestions(control_element, trigger_source);
    return;
  }
  if (trigger_source ==
          AutofillSuggestionTriggerSource::kComposeDialogLostFocus ||
      trigger_source ==
          AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge) {
    if (WebElement content_editable =
            form_util::GetContentEditableByRendererId(field_id);
        !content_editable.IsNull()) {
      ShowSuggestionsForContentEditable(content_editable, trigger_source);
    }
  }
}

void AutofillAgent::ApplyFieldAction(
    mojom::FieldActionType action_type,
    mojom::ActionPersistence action_persistence,
    FieldRendererId field_id,
    const std::u16string& value) {
  if (!unsafe_render_frame()) {
    return;
  }
  WebFormControlElement form_control =
      form_util::GetFormControlByRendererId(field_id);
  if (!form_control.IsNull() &&
      form_util::IsTextAreaElementOrTextInput(form_control)) {
    DCHECK(
        form_util::MaybeWasOwnedByFrame(form_control, unsafe_render_frame()));
    ClearPreviewedForm();
    switch (action_persistence) {
      case mojom::ActionPersistence::kPreview:
        switch (action_type) {
          case mojom::FieldActionType::kReplaceSelection:
            NOTIMPLEMENTED()
                << "Previewing replacement of selection is not implemented";
            break;
          case mojom::FieldActionType::kReplaceAll:
            previewed_elements_.emplace_back(last_queried_element_,
                                             form_control.GetAutofillState());
            form_control.SetSuggestedValue(WebString::FromUTF16(value));
            break;
          case mojom::FieldActionType::kSelectAll:
            NOTIMPLEMENTED() << "Previewing select all is not implemented";
            break;
        }
        break;
      case mojom::ActionPersistence::kFill:
        switch (action_type) {
          case mojom::FieldActionType::kReplaceSelection: {
            form_control.PasteText(WebString::FromUTF16(value),
                                   /*replace_all=*/false);
            break;
          }
          case mojom::FieldActionType::kReplaceAll: {
            DoFillFieldWithValue(value, form_control,
                                 WebAutofillState::kAutofilled);
            break;
          }
          case mojom::FieldActionType::kSelectAll:
            DCHECK(value.empty());
            form_control.SelectText(/*select_all=*/true);
            break;
        }
        if (base::FeatureList::IsEnabled(
                features::kAutofillUnifyAndFixFormTracking)) {
          if (WebFormElement form_element =
                  form_util::GetOwningForm(form_control);
              !form_element.IsNull()) {
            UpdateLastInteractedElement(
                form_util::GetFormRendererId(form_element));
          } else {
            UpdateLastInteractedElement(
                form_util::GetFieldRendererId(form_control));
          }
        }
        break;
    }
    return;
  }

  if (WebElement content_editable =
          form_util::GetContentEditableByRendererId(field_id);
      !content_editable.IsNull()) {
    switch (action_persistence) {
      case mojom::ActionPersistence::kPreview:
        NOTIMPLEMENTED()
            << "Previewing replacement of selection is not implemented";
        break;
      case mojom::ActionPersistence::kFill:
        switch (action_type) {
          case mojom::FieldActionType::kSelectAll:
            DCHECK(value.empty());
            content_editable.SelectText(/*select_all=*/true);
            break;
          case mojom::FieldActionType::kReplaceAll:
            [[fallthrough]];
          case mojom::FieldActionType::kReplaceSelection:
            content_editable.PasteText(
                WebString::FromUTF16(value),
                /*replace_all=*/
                (action_type == mojom::FieldActionType::kReplaceAll));
            break;
        }
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
    std::vector<std::u16string_view> parts = base::SplitStringPiece(
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

  password_autofill_agent_->PreviewSuggestion(last_queried_element, username,
                                              password);
}

void AutofillAgent::PreviewPasswordGenerationSuggestion(
    const std::u16string& password) {
  DCHECK(password_generation_agent_);
  password_generation_agent_->PreviewGenerationSuggestion(password);
}

void AutofillAgent::ShowSuggestions(
    const WebFormControlElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  // TODO(crbug.com/40068004): Make this a CHECK.
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
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

  const bool show_for_empty_value =
      config_.uses_keyboard_accessory_for_suggestions ||
      ShouldAutofillOnEmptyValues(trigger_source);
  const bool element_value_valid = [&element, trigger_source,
                                    show_for_empty_value] {
    WebString value = element.EditingValue();
    // Don't attempt to autofill with values that are too large.
    if (!ShouldAutofillOnLongValues(trigger_source) &&
        value.length() > kMaxStringLength) {
      return false;
    }
    if (!show_for_empty_value && value.IsEmpty()) {
      return false;
    }
    return !(RequiresCaretAtEnd(trigger_source) &&
             (element.SelectionStart() != element.SelectionEnd() ||
              element.SelectionEnd() != value.length()));
  }();
  if (!element_value_valid) {
    // Any popup currently showing is obsolete.
    HidePopup();
    return;
  }

  last_queried_element_ = FieldRef(element);

  // Manual fallbacks override any prioritization done based on the field type.
  // TODO(b/333990908): Test manual fallback on different form types.
  if (IsAddressAutofillManuallyTriggered(trigger_source) ||
      IsPaymentsAutofillManuallyTriggered(trigger_source)) {
    QueryAutofillSuggestions(element, trigger_source);
    return;
  }
  if (IsPasswordsAutofillManuallyTriggered(trigger_source)) {
    is_popup_possibly_visible_ = password_autofill_agent_->ShowSuggestions(
        input_element, trigger_source);
    return;
  }

  // Proceed with generating suggestions based on the field type.
  if (form_util::IsAutofillableInputElement(input_element)) {
    if (password_generation_agent_ &&
        password_generation_agent_->ShowPasswordGenerationSuggestions(
            input_element)) {
      is_popup_possibly_visible_ = true;
      return;
    }
    if (password_autofill_agent_->ShowSuggestions(input_element,
                                                  trigger_source)) {
      is_popup_possibly_visible_ = true;
      return;
    }
  }

  // Password field elements should only have suggestions shown by the password
  // autofill agent. The /*disable presubmit*/ comment below is used to disable
  // a presubmit script that ensures that only IsPasswordFieldForAutofill() is
  // used in this code (it has to appear between the function name and the
  // parenthesis to not match a regex). In this specific case we are actually
  // interested in whether the field is currently a password field, not whether
  // it has ever been a password field.
  if (!input_element.IsNull() &&
      input_element.IsPasswordField /*disable presubmit*/ () &&
      !config_.query_password_suggestions) {
    return;
  }

  QueryAutofillSuggestions(element, trigger_source);
}

void AutofillAgent::ShowSuggestionsForContentEditable(
    const blink::WebElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  std::optional<FormData> form = form_util::FindFormForContentEditable(element);
  if (!form) {
    return;
  }
  CHECK_EQ(form->fields.size(), 1u);
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    is_popup_possibly_visible_ = true;
    autofill_driver->AskForValuesToFill(*form, form->fields[0], trigger_source);
  }
}

void AutofillAgent::GetPotentialLastFourCombinationsForStandaloneCvc(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  if (!unsafe_render_frame()) {
    std::vector<std::string> matches;
    std::move(potential_matches).Run(matches);
  } else {
    WebDocument document = unsafe_render_frame()->GetWebFrame()->GetDocument();
    form_util::TraverseDomForFourDigitCombinations(
        document, std::move(potential_matches));
  }
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  DCHECK(!element.DynamicTo<WebInputElement>().IsNull() ||
         form_util::IsTextAreaElement(element));

  std::optional<FormAndField> form_and_field =
      FindFormAndFieldForFormControlElement(
          element, field_data_manager(),
          MaybeExtractDatalist({form_util::ExtractOption::kBounds}));
  auto [form, field] =
      form_and_field.value_or(std::make_pair(FormData(), FormFieldData()));
  if (!form_and_field) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(
        form_util::GetOwningForm(element), element, nullptr,
        MaybeExtractDatalist({form_util::ExtractOption::kValue,
                              form_util::ExtractOption::kBounds}),
        &field);
  }

  if (config_.secure_context_required &&
      !element.GetDocument().IsSecureContext()) {
    LOG(WARNING) << "Autofill suggestions are disabled because the document "
                    "isn't a secure context.";
    return;
  }

  if (!config_.extract_all_datalists) {
    const WebInputElement input_element = element.DynamicTo<WebInputElement>();
    if (!input_element.IsNull()) {
      // Find the datalist values and send them to the browser process.
      std::vector<SelectOption> datalist_options;
      form_util::GetDataListSuggestions(input_element, &datalist_options);
      field.set_datalist_options(std::move(datalist_options));
    }
  }

  is_popup_possibly_visible_ = true;
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->AskForValuesToFill(form, field, trigger_source);
  }
}

void AutofillAgent::DoFillFieldWithValue(std::u16string_view value,
                                         WebFormControlElement& element,
                                         WebAutofillState autofill_state) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  element.SetAutofillValue(WebString::FromUTF16(value), autofill_state);
  UpdateStateForTextChange(element,
                           autofill_state == WebAutofillState::kAutofilled
                               ? FieldPropertiesFlags::kAutofilled
                               : FieldPropertiesFlags::kUserTyped);
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
  DenseSet<form_util::ExtractOption> extract_options = MaybeExtractDatalist(
      {form_util::ExtractOption::kBounds, form_util::ExtractOption::kOptions,
       form_util::ExtractOption::kOptionText,
       form_util::ExtractOption::kValue});
  WebDocument document = unsafe_render_frame()->GetWebFrame()->GetDocument();
  if (!form_id) {
    if (std::optional<FormData> form =
            form_util::ExtractFormData(document, WebFormElement(),
                                       field_data_manager(), extract_options)) {
      std::move(callback).Run(std::move(form));
      return;
    }
  }
  if (WebFormElement form_element = form_util::GetFormByRendererId(form_id);
      !form_element.IsNull()) {
    if (std::optional<FormData> form = form_util::ExtractFormData(
            document, form_element, field_data_manager(), extract_options)) {
      std::move(callback).Run(std::move(form));
      return;
    }
  }
  if (WebElement ce =
          form_util::GetContentEditableByRendererId(FieldRendererId(*form_id));
      !ce.IsNull()) {
    std::move(callback).Run(form_util::FindFormForContentEditable(ce));
    return;
  }
  std::move(callback).Run(std::nullopt);
}

void AutofillAgent::EmitFormIssuesToDevtools() {
  // TODO(crbug.com/1399414,crbug.com/1444566): Throttle this call if possible.
  ExtractFormsUnthrottled(/*callback=*/{});
}

void AutofillAgent::ExtractForms(base::OneShotTimer& timer,
                                 base::OnceCallback<void(bool)> callback) {
  if (!is_dom_content_loaded_ || timer.IsRunning()) {
    if (!callback.is_null()) {
      std::move(callback).Run(/*success=*/false);
    }
    return;
  }
  timer.Start(FROM_HERE, kFormsSeenThrottle,
              base::BindOnce(&AutofillAgent::ExtractFormsUnthrottled,
                             base::Unretained(this), std::move(callback)));
}

void AutofillAgent::ExtractFormsAndNotifyPasswordAutofillAgent(
    base::OneShotTimer& timer) {
  if (!is_dom_content_loaded_ || timer.IsRunning()) {
    return;
  }
  timer.Start(
      FROM_HERE, kFormsSeenThrottle,
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
  if (content::RenderFrame* render_frame = unsafe_render_frame()) {
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
  if (config_.uses_keyboard_accessory_for_suggestions) {
    return;
  }

  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->HidePopup();
  }
}

void AutofillAgent::DidChangeFormRelatedElementDynamically(
    const WebElement& element,
    WebFormRelatedChangeType form_related_change) {
  if (!is_dom_content_loaded_) {
    return;
  }
  switch (form_related_change) {
    case blink::WebFormRelatedChangeType::kAdd:
    case blink::WebFormRelatedChangeType::kReassociate:
      ExtractFormsAndNotifyPasswordAutofillAgent(
          process_forms_after_dynamic_change_timer_);
      break;
    case blink::WebFormRelatedChangeType::kRemove:
      form_tracker_->ElementDisappeared(element);
      if (base::FeatureList::IsEnabled(
              features::kAutofillDetectRemovedFormControls)) {
        ExtractFormsAndNotifyPasswordAutofillAgent(
            process_forms_after_dynamic_change_timer_);
      }
      break;
    case blink::WebFormRelatedChangeType::kHide:
      form_tracker_->ElementDisappeared(element);
      break;
  }
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

  if (!config_.uses_keyboard_accessory_for_suggestions &&
      config_.focus_requires_scroll) {
    HandleFocusChangeComplete(
        /*focused_node_was_last_clicked=*/
        last_left_mouse_down_or_gesture_tap_in_node_caused_focus_);
  }
  last_left_mouse_down_or_gesture_tap_in_node_caused_focus_ = false;
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
// A batch ends if no event occurred for `kWaitTimeForOptionsChanges`. For a
// given batch, the AutofillDriver is informed only about the last FormData.
// That is, if within one batch the options of different forms changed, all but
// one of these events will be lost.
void AutofillAgent::SelectOrSelectListFieldOptionsChanged(
    const WebFormControlElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (!was_last_action_fill_ || last_queried_element_.GetField().IsNull()) {
    return;
  }

  if (select_or_selectlist_option_change_batch_timer_.IsRunning()) {
    select_or_selectlist_option_change_batch_timer_.AbandonAndStop();
  }

  select_or_selectlist_option_change_batch_timer_.Start(
      FROM_HERE, kWaitTimeForOptionsChanges,
      base::BindRepeating(&AutofillAgent::BatchSelectOrSelectListOptionChange,
                          base::Unretained(this),
                          form_util::GetFieldRendererId(element)));
}

void AutofillAgent::BatchSelectOrSelectListOptionChange(
    FieldRendererId element_id) {
  WebFormControlElement element =
      form_util::GetFormControlByRendererId(element_id);
  if (element.IsNull() || element.GetDocument().IsNull()) {
    return;
  }

  // Look for the form and field associated with the select element. If they are
  // found, notify the driver that the form was modified dynamically.
  if (std::optional<FormAndField> form_and_field =
          form_util::FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              /*extract_options=*/{})) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver();
        autofill_driver && !field.options().empty()) {
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
  DCHECK(form_util::MaybeWasOwnedByFrame(form, unsafe_render_frame()));
  password_autofill_agent_->InformAboutFormClearing(form);
}

void AutofillAgent::PasswordFieldReset(const WebInputElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  password_autofill_agent_->InformAboutFieldClearing(element);
}

bool AutofillAgent::IsPrerendering() const {
  return unsafe_render_frame() &&
         unsafe_render_frame()->GetWebFrame()->GetDocument().IsPrerendering();
}

void AutofillAgent::HandleFocusChangeComplete(
    bool focused_node_was_last_clicked) {
  if (!unsafe_render_frame()) {
    return;
  }

  // When using Talkback on Android, and possibly others, traversing to and
  // focusing a field will not register as a click. Thus, when screen readers
  // are used, treat the focused node as if it was the last clicked.
  focused_node_was_last_clicked |= is_screen_reader_enabled_;

  WebElement focused_element =
      unsafe_render_frame()->GetWebFrame()->GetDocument().FocusedElement();
  if (focused_element.IsNull()) {
    return;
  }

  if (auto focused_control = focused_element.DynamicTo<WebFormControlElement>();
      form_util::IsTextAreaElementOrTextInput(focused_control)) {
    if (focused_node_was_last_clicked) {
      was_last_action_fill_ = false;
#if BUILDFLAG(IS_ANDROID)
      if (!base::FeatureList::IsEnabled(
              password_manager::features::kPasswordSuggestionBottomSheetV2)) {
        password_autofill_agent_->TryToShowKeyboardReplacingSurface(
            focused_control);
      }
#endif
      ShowSuggestions(
          focused_control,
          AutofillSuggestionTriggerSource::kFormControlElementClicked);
    } else if (form_util::IsTextAreaElement(focused_control)) {
#if !BUILDFLAG(IS_ANDROID)
      // Compose reacts to tab area focus even when not triggered by a click -
      // therefore call `ShowSuggestions` with a separate trigger source.
      ShowSuggestions(
          focused_control,
          AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick);
#endif
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
  ShowSuggestionsForContentEditable(
      focused_element,
      AutofillSuggestionTriggerSource::kContentEditableClicked);
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
}

void AutofillAgent::JavaScriptChangedValue(WebFormControlElement element,
                                           const WebString& old_value,
                                           bool was_autofilled) {
  // The provisionally saved form must be updated on JS changes. However, it
  // should not be changed to another form, so that only the user can set the
  // tracked form and not JS. This call here is meant to keep the tracked form
  // up to date with the form's most recent version.
  if (provisionally_saved_form() &&
      form_util::GetFormRendererId(form_util::GetOwningForm(element)) ==
          last_interacted_form().GetId() &&
      base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
    // Ideally, we re-extract the form at this moment, but to avoid performance
    // regression, we just update what JS updated on the Blink side.
    if (auto it = base::ranges::find(provisionally_saved_form()->fields,
                                     form_util::GetFieldRendererId(element),
                                     &FormFieldData::renderer_id);
        it != provisionally_saved_form()->fields.end()) {
      it->set_value(element.Value().Utf16());
      it->set_is_autofilled(element.IsAutofilled());
    }
  }

  const auto input_element = element.DynamicTo<WebInputElement>();
  if (!input_element.IsNull() && !element.Value().IsEmpty() &&
      (input_element.IsPasswordFieldForAutofill() ||
       password_autofill_agent_->IsUsernameInputField(input_element))) {
    password_autofill_agent_->UpdatePasswordStateForTextChange(input_element);
  }

  if (!was_autofilled) {
    return;
  }
  bool formatting_only = JavaScriptOnlyReformattedValue(
      old_value.Utf16(), element.Value().Utf16());
  if (formatting_only &&
      base::FeatureList::IsEnabled(
          features::kAutofillFixCachingOnJavaScriptChanges)) {
    element.SetAutofillState(WebAutofillState::kAutofilled);
  }
  if (std::optional<FormAndField> form_and_field =
          form_util::FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              /*extract_options=*/{})) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->JavaScriptChangedAutofilledValue(
          form, field, old_value.Utf16(), formatting_only);
    }
  }
}

void AutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form_element,
    const WebFormControlElement& element,
    SaveFormReason source) {
  DCHECK(form_util::MaybeWasOwnedByFrame(form_element, unsafe_render_frame()));
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  WebDocument document =
      unsafe_render_frame()
          ? unsafe_render_frame()->GetWebFrame()->GetDocument()
          : WebDocument();

  // Updates cached data needed for submission so that we only cache the latest
  // version of the to-be-submitted form.
  auto update_submission_data_on_user_edit = [&]() {
    if (!form_element.IsNull()) {
      UpdateLastInteractedElement(form_util::GetFormRendererId(form_element));
      return;
    }
    std::erase_if(formless_elements_user_edited_,
                  [](const FieldRendererId field_id) {
                    WebFormControlElement field =
                        form_util::GetFormControlByRendererId(field_id);
                    return !field.IsNull() &&
                           form_util::IsWebElementFocusableForAutofill(field);
                  });
    formless_elements_user_edited_.insert(
        form_util::GetFieldRendererId(element));
    if (base::FeatureList::IsEnabled(
            features::kAutofillUnifyAndFixFormTracking)) {
      UpdateLastInteractedElement(form_util::GetFieldRendererId(element));
    } else {
      UpdateLastInteractedElement(FormRendererId());
    }
  };

  switch (source) {
    case FormTracker::Observer::SaveFormReason::kWillSendSubmitEvent:
      // Fire the form submission event to avoid missing submissions where
      // websites handle the onsubmit event. This also gets the form before
      // Javascript's submit event handler could change it. We don't clear
      // submitted_forms_ because OnFormSubmitted will normally be invoked
      // afterwards and we don't want to fire the same event twice.
      if (base::FeatureList::IsEnabled(
              features::kAutofillUnifyAndFixFormTracking)) {
        // TODO(b/40281981): Figure out if this is still needed, and document
        // the reason, otherwise remove.
        password_autofill_agent_->InformBrowserAboutUserInput(
            form_element, WebInputElement());
        // TODO(b/40281981): Figure out if this is still needed, and document
        // the reason, otherwise remove.
        update_submission_data_on_user_edit();
      }
      if (std::optional<FormData> form_data = form_util::ExtractFormData(
              document, form_element, field_data_manager())) {
        FireHostSubmitEvents(*form_data, /*known_success=*/false,
                             mojom::SubmissionSource::FORM_SUBMISSION);
      }
      if (!base::FeatureList::IsEnabled(
              features::kAutofillUnifyAndFixFormTracking)) {
        ResetLastInteractedElements();
      }
      break;
    case FormTracker::Observer::SaveFormReason::kTextFieldChanged:
      update_submission_data_on_user_edit();
      OnTextFieldDidChange(element);
      break;
    case FormTracker::Observer::SaveFormReason::kSelectChanged:
      update_submission_data_on_user_edit();
      // Signal the browser of change in select fields.
      // TODO(crbug.com/40281981): Investigate if this is necessary: if it is,
      // document the reason, if not, remove.
      if (std::optional<FormAndField> form_and_field =
              FindFormAndFieldForFormControlElement(
                  element, field_data_manager(),
                  MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
        auto& [form, field] = *form_and_field;
        if (auto* autofill_driver = unsafe_autofill_driver()) {
          autofill_driver->SelectControlDidChange(form, field);
        }
      }
      break;
  }
}

void AutofillAgent::OnProbablyFormSubmitted() {
  if (std::optional<FormData> form_data = GetSubmittedForm()) {
    FireHostSubmitEvents(form_data.value(), /*known_success=*/false,
                         mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
}

void AutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  DCHECK(form_util::MaybeWasOwnedByFrame(form, unsafe_render_frame()));
  // Fire the submission event here because WILL_SEND_SUBMIT_EVENT is skipped
  // if javascript calls submit() directly.
  if (std::optional<FormData> form_data = form_util::ExtractFormData(
          form.GetDocument(), form, field_data_manager())) {
    FireHostSubmitEvents(*form_data, /*known_success=*/false,
                         mojom::SubmissionSource::FORM_SUBMISSION);
  }
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    ResetLastInteractedElements();
    OnFormNoLongerSubmittable();
  }
}

void AutofillAgent::OnInferredFormSubmission(mojom::SubmissionSource source) {
  if (!unsafe_render_frame()) {
    return;
  }
  switch (source) {
    // This source is only used as a default value to variables.
    case mojom::SubmissionSource::NONE:
    // This source is handled by `AutofillAgent::OnFormSubmitted`.
    case mojom::SubmissionSource::FORM_SUBMISSION:
    // This source is handled by `AutofillAgent::OnProbablyFormSubmitted`.
    case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
      NOTREACHED_NORETURN();
    case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      if (base::FeatureList::IsEnabled(
              features::kAutofillUnifyAndFixFormTracking)) {
        password_autofill_agent_->FireHostSubmitEvent(
            FormRendererId(),
            mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
      }
      return;
    // This event occurs only when either this frame or a same process parent
    // frame of it gets detached.
    case mojom::SubmissionSource::FRAME_DETACHED:
      // Detaching the main frame means that navigation happened or the current
      // tab was closed, both reasons being too general to be able to deduce
      // submission from it (and the relevant use cases will most probably be
      // handled by other sources), therefore we only consider detached
      // subframes.
      if ((!unsafe_render_frame()->GetWebFrame()->IsOutermostMainFrame() ||
           base::FeatureList::IsEnabled(
               features::kAutofillUnifyAndFixFormTracking)) &&
          provisionally_saved_form()) {
        // Should not access the frame because it is now detached. Instead, use
        // `provisionally_saved_form()`.
        FireHostSubmitEvents(*provisionally_saved_form(),
                             /*known_success=*/true, source);
      }
      break;
    case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
    case mojom::SubmissionSource::XHR_SUCCEEDED:
      if (std::optional<FormData> form_data = GetSubmittedForm()) {
        FireHostSubmitEvents(*form_data, /*known_success=*/true, source);
      }
      break;
  }
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
}

void AutofillAgent::AddFormObserver(Observer* observer) {
  form_tracker_->AddObserver(observer);
}

void AutofillAgent::RemoveFormObserver(Observer* observer) {
  form_tracker_->RemoveObserver(observer);
}

void AutofillAgent::TrackAutofilledElement(
    const WebFormControlElement& element) {
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
  if (base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
    return provisionally_saved_form();
  }
  auto has_been_user_edited = [this](const FormFieldData& field) {
    return formless_elements_user_edited_.contains(field.renderer_id());
  };
  // The three cases handled by this function:
  bool user_autofilled_or_edited_owned_form = !!last_interacted_form().GetId();
  bool user_autofilled_unowned_form = formless_elements_were_autofilled_;
  bool user_edited_unowned_form = !user_autofilled_or_edited_owned_form &&
                                  !user_autofilled_unowned_form &&
                                  !formless_elements_user_edited_.empty();
  if ((!user_autofilled_or_edited_owned_form && !user_autofilled_unowned_form &&
       !user_edited_unowned_form) ||
      !unsafe_render_frame()) {
    return std::nullopt;
  }
  // Extracts the last-interacted form, with fallback to its last-saved state.
  std::optional<FormData> form = form_util::ExtractFormData(
      unsafe_render_frame()->GetWebFrame()->GetDocument(),
      last_interacted_form().GetForm(), field_data_manager());
  return !form || (user_edited_unowned_form &&
                   base::ranges::none_of(form->fields, has_been_user_edited))
             ? provisionally_saved_form()
             : form;
}

void AutofillAgent::ResetLastInteractedElements() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    form_tracker_->ResetLastInteractedElements();
  } else {
    last_interacted_form_ = {};
    provisionally_saved_form() = {};
  }
  formless_elements_user_edited_.clear();
  formless_elements_were_autofilled_ = false;
}

void AutofillAgent::UpdateLastInteractedElement(
    absl::variant<FormRendererId, FieldRendererId> element_id) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    form_tracker_->UpdateLastInteractedElement(element_id);
  } else {
    CHECK(absl::holds_alternative<FormRendererId>(element_id));
    WebFormElement form_element =
        form_util::GetFormByRendererId(absl::get<FormRendererId>(element_id));
    last_interacted_form_ = FormRef(form_element);
    provisionally_saved_form() =
        unsafe_render_frame()
            ? form_util::ExtractFormData(
                  unsafe_render_frame()->GetWebFrame()->GetDocument(),
                  form_util::GetFormByRendererId(
                      absl::get<FormRendererId>(element_id)),
                  field_data_manager())
            : std::nullopt;
  }
}

void AutofillAgent::OnFormNoLongerSubmittable() {
  submitted_forms_.clear();
}

DenseSet<form_util::ExtractOption> AutofillAgent::MaybeExtractDatalist(
    DenseSet<form_util::ExtractOption> extract_options) {
  if (config_.extract_all_datalists) {
    extract_options.insert(form_util::ExtractOption::kDatalist);
  }
  return extract_options;
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
