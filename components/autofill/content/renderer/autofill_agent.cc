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
#include "third_party/blink/public/web/web_frame_widget.h"
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

using enum CallTimerState::CallSite;

constexpr char kSubmissionSourceHistogram[] =
    "Autofill.SubmissionDetectionSource.AutofillAgent";

// Time to wait in ms to ensure that only a single select or datalist change
// will be acted upon, instead of multiple in close succession (debounce time).
constexpr base::TimeDelta kWaitTimeForOptionsChanges = base::Milliseconds(50);

using FormAndField = std::pair<FormData, raw_ref<const FormFieldData>>;

// TODO(crbug.com/40753022): Move this to the browser process.
blink::FormElementPiiType MapTypePredictionToFormElementPiiType(
    std::string_view type) {
  if (type == "NO_SERVER_DATA" || type == "UNKNOWN_TYPE" ||
      type == "EMPTY_TYPE" || type == "") {
    return blink::FormElementPiiType::kUnknown;
  }

  if (type.starts_with("EMAIL_")) {
    return blink::FormElementPiiType::kEmail;
  }
  if (type.starts_with("PHONE_")) {
    return blink::FormElementPiiType::kPhone;
  }
  return blink::FormElementPiiType::kOthers;
}

std::string GetButtonTitlesString(const ButtonTitleList& titles_list) {
  std::vector<std::string> titles;
  titles.reserve(titles_list.size());
  base::ranges::transform(
      titles_list, std::back_inserter(titles),
      [](const auto& list_item) { return base::UTF16ToUTF8(list_item.first); });
  return base::JoinString(titles, ",");
}

// For each field in the |form|, if |attach_predictions_to_dom| is true, sets
// the title to include the field's heuristic type, server type, and
// signature; as well as the form's signature and the experiment id for the
// server predictions.
//
// It also calls WebFormControlElement::SetFormElementPiiType() for every form
// control (which is actually unrelated to this function.)
//
// TODO(crbug.com/40753022): FormDataPredictions should be sent to the renderer
// process and this function should be called only if
// chrome://flags/#show-autofill-type-predictions is enabled. For this, the
// PII metric related to WebFormControlElement::SetFormElementPiiType() must be
// moved to the browser process.
bool ShowPredictions(const WebDocument& document,
                     const FormDataPredictions& form,
                     bool attach_predictions_to_dom) {
  DCHECK_EQ(form.data.fields().size(), form.fields.size());

  WebFormElement form_element =
      form_util::GetFormByRendererId(form.data.renderer_id());
  std::vector<WebFormControlElement> control_elements =
      form_util::GetOwnedAutofillableFormControls(document, form_element);
  if (control_elements.size() != form.fields.size()) {
    // Keep things simple.  Don't show predictions for forms that were modified
    // between page load and the server's response to our query.
    return false;
  }

  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement& element = control_elements[i];

    const FormFieldData& field_data = form.data.fields()[i];
    if (form_util::GetFieldRendererId(element) != field_data.renderer_id()) {
      continue;
    }
    const FormFieldDataPredictions& field = form.fields[i];

    // TODO(crbug.com/40753022): Move this to the browser process so
    // FormDataPredictions doesn't have to be sent to the renderer
    // unconditionally.
    element.SetFormElementPiiType(
        MapTypePredictionToFormElementPiiType(field.overall_type));

    // If the flag is enabled, attach the prediction to the field.
    if (attach_predictions_to_dom) {
      constexpr size_t kMaxLabelSize = 100;
      // TODO(crbug.com/40741721): Use `parseable_label()` once the feature is
      // launched.
      std::u16string truncated_label =
          field_data.label().substr(0, kMaxLabelSize);
      // The label may be derived from the placeholder attribute and may contain
      // line wraps which are normalized here.
      base::ReplaceChars(truncated_label, u"\n", u"|", &truncated_label);

      std::string form_id =
          base::NumberToString(form.data.renderer_id().value());
      std::string field_id_str =
          base::NumberToString(field_data.renderer_id().value());

      blink::LocalFrameToken frame_token;
      if (auto* frame = element.GetDocument().GetFrame()) {
        frame_token = frame->GetLocalFrameToken();
      }

      std::string title = base::StrCat({
          "overall type: ",
          field.overall_type,
          "\nhtml type: ",
          field.html_type,
          "\nserver type: ",
          field.server_type.has_value() ? field.server_type.value()
                                        : "SERVER_RESPONSE_PENDING",
          "\nheuristic type: ",
          field.heuristic_type,
          "\nlabel: ",
          base::UTF16ToUTF8(truncated_label),
          "\nparseable name: ",
          field.parseable_name,
          "\nsection: ",
          field.section,
          "\nfield signature: ",
          field.signature,
          "\nform signature: ",
          form.signature,
          "\nform signature in host form: ",
          field.host_form_signature,
          "\nalternative form signature: ",
          form.alternative_signature,
          "\nform name: ",
          base::UTF16ToUTF8(form.data.name_attribute()),
          "\nform id: ",
          base::UTF16ToUTF8(form.data.id_attribute()),
          "\nform button titles: ",
          GetButtonTitlesString(form_util::GetButtonTitles(
              form_element, /*button_titles_cache=*/nullptr)),
          "\nfield frame token: ",
          frame_token.ToString(),
          "\nform renderer id: ",
          form_id,
          "\nfield renderer id: ",
          field_id_str,
          "\nvisible: ",
          field_data.is_visible() ? "true" : "false",
          "\nfocusable: ",
          field_data.IsFocusable() ? "true" : "false",
          "\nfield rank: ",
          base::NumberToString(field.rank),
          "\nfield rank in signature group: ",
          base::NumberToString(field.rank_in_signature_group),
          "\nfield rank in host form: ",
          base::NumberToString(field.rank_in_host_form),
          "\nfield rank in host form signature group: ",
          base::NumberToString(field.rank_in_host_form_signature_group),
      });

      if (features::test::kAutofillShowTypePredictionsVerboseParam.Get()) {
        std::u16string truncated_aria_label =
            field_data.aria_label().substr(0, kMaxLabelSize);
        base::ReplaceChars(truncated_aria_label, u"\n", u"|",
                           &truncated_aria_label);

        std::u16string truncated_aria_description =
            field_data.aria_description().substr(0, kMaxLabelSize);
        base::ReplaceChars(truncated_aria_description, u"\n", u"|",
                           &truncated_aria_description);

        std::string option_labels;
        std::string option_values;
        for (size_t option_index = 0;
             option_index < field_data.options().size(); option_index++) {
          const SelectOption& select_option =
              field_data.options()[option_index];
          const std::string delimiter = option_index > 0 ? "|" : "";
          option_labels =
              option_labels + delimiter + base::UTF16ToUTF8(select_option.text);
          option_values = option_values + delimiter +
                          base::UTF16ToUTF8(select_option.value);
        }

        title = base::StrCat({
            title,
            "\naria label: ",
            base::UTF16ToUTF8(truncated_aria_label),
            "\naria description: ",
            base::UTF16ToUTF8(truncated_aria_description),
            "\nplaceholder: ",
            base::UTF16ToUTF8(field_data.placeholder()),
            "\noption labels: ",
            option_labels,
            "\noption values: ",
            option_values,
        });
      }

      WebString kAutocomplete = WebString::FromASCII("autocomplete");
      if (element.HasAttribute(kAutocomplete)) {
        title += "\nautocomplete: " +
                 element.GetAttribute(kAutocomplete).Utf8().substr(0, 100);
      }

      // Set the same debug string to an attribute that does not get mangled if
      // Google Translate is triggered for the site. This is useful for
      // automated processing of the data.
      element.SetAttribute("autofill-information", WebString::FromUTF8(title));

      //  If the field has password manager's annotation, add it as well.
      if (element.HasAttribute("pm_parser_annotation")) {
        title =
            base::StrCat({title, "\npm_parser_annotation: ",
                          element.GetAttribute("pm_parser_annotation").Utf8()});
      }

      // Set this debug string to the title so that a developer can easily debug
      // by hovering the mouse over the input field.
      element.SetAttribute("title", WebString::FromUTF8(title));

      element.SetAttribute("autofill-prediction",
                           WebString::FromUTF8(field.overall_type));
    }
  }
  return true;
}

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
  // TODO(crbug.com/40947225): Internationalize this normalization.
  return old_value == new_value;
}

gfx::Rect GetCaretBounds(content::RenderFrame& frame) {
  if (!base::FeatureList::IsEnabled(features::kAutofillCaretExtraction)) {
    return gfx::Rect();
  }
  if (auto* frame_widget = frame.GetWebFrame()->LocalRoot()->FrameWidget()) {
    gfx::Rect anchor;
    gfx::Rect focus;
    frame_widget->CalculateSelectionBounds(anchor, focus);
    return frame.ConvertViewportToWindow(focus);
  }
  return gfx::Rect();
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
  void CaretMovedInFormField(const FormData& form,
                             FieldRendererId field_id,
                             const gfx::Rect& caret_bounds) override {
    DeferMsg(&mojom::AutofillDriver::CaretMovedInFormField, form, field_id,
             caret_bounds);
  }
  void TextFieldDidChange(const FormData& form,
                          FieldRendererId field_id,
                          base::TimeTicks timestamp) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldDidChange, form, field_id,
             timestamp);
  }
  void TextFieldDidScroll(const FormData& form,
                          FieldRendererId field_id) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldDidScroll, form, field_id);
  }
  void SelectControlDidChange(const FormData& form,
                              FieldRendererId field_id) override {
    DeferMsg(&mojom::AutofillDriver::SelectControlDidChange, form, field_id);
  }
  void SelectFieldOptionsDidChange(const FormData& form) override {
    DeferMsg(&mojom::AutofillDriver::SelectFieldOptionsDidChange, form);
  }
  void AskForValuesToFill(
      const FormData& form,
      FieldRendererId field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override {
    DeferMsg(&mojom::AutofillDriver::AskForValuesToFill, form, field_id,
             caret_bounds, trigger_source);
  }
  void HidePopup() override { DeferMsg(&mojom::AutofillDriver::HidePopup); }
  void FocusOnNonFormField() override {
    DeferMsg(&mojom::AutofillDriver::FocusOnNonFormField);
  }
  void FocusOnFormField(const FormData& form,
                        FieldRendererId field_id) override {
    DeferMsg(&mojom::AutofillDriver::FocusOnFormField, form, field_id);
  }
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {
    DeferMsg(&mojom::AutofillDriver::DidFillAutofillFormData, form, timestamp);
  }
  void DidEndTextFieldEditing() override {
    DeferMsg(&mojom::AutofillDriver::DidEndTextFieldEditing);
  }
  void JavaScriptChangedAutofilledValue(const FormData& form,
                                        FieldRendererId field_id,
                                        const std::u16string& old_value,
                                        bool formatting_only) override {
    DeferMsg(&mojom::AutofillDriver::JavaScriptChangedAutofilledValue, form,
             field_id, old_value, formatting_only);
  }

  const raw_ref<AutofillAgent> agent_;
  base::WeakPtrFactory<DeferringAutofillDriver> weak_ptr_factory_{this};
};

AutofillAgent::AutofillAgent(
    content::RenderFrame* render_frame,
    Config config,
    std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
    std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      config_(config),
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

WebDocument AutofillAgent::GetDocument() const {
  return unsafe_render_frame()
             ? unsafe_render_frame()->GetWebFrame()->GetDocument()
             : WebDocument();
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
  form_cache_.Reset();
  is_dom_content_loaded_ = false;
  select_option_change_batch_timer_.Stop();
  datalist_option_change_batch_timer_.Stop();
  process_forms_after_dynamic_change_timer_.Stop();
  process_forms_form_extraction_timer_.Stop();
  process_forms_form_extraction_with_response_timer_.Stop();
  ResetLastInteractedElements();
  OnFormNoLongerSubmittable();
  timing_ = {};
}

void AutofillAgent::DidDispatchDOMContentLoadedEvent() {
  base::UmaHistogramBoolean("Autofill.DOMContentLoadedInOutermostMainFrame",
                            unsafe_render_frame()->IsMainFrame() &&
                                !unsafe_render_frame()->IsInFencedFrameTree());
  is_dom_content_loaded_ = true;
  timing_.last_dom_content_loaded = base::TimeTicks::Now();
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
  if (element != last_queried_element_.GetField() || !element ||
      config_.focus_requires_scroll || !is_popup_possibly_visible_ ||
      !element.Focused()) {
    return;
  }

  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (std::optional<FormAndField> form_and_field =
          FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              GetCallTimerState(kDidChangeScrollOffsetImpl),
              MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidScroll(form, field->renderer_id());
    }
  }

  // Ignore subsequent scroll offset changes.
  HidePopup();
}

CallTimerState AutofillAgent::GetCallTimerState(
    CallTimerState::CallSite call_site) const {
  return {.call_site = call_site,
          .last_autofill_agent_reset = timing_.last_autofill_agent_reset,
          .last_dom_content_loaded = timing_.last_dom_content_loaded};
}

void AutofillAgent::FocusedElementChanged(
    const WebElement& new_focused_element) {
  ObserveCaret(new_focused_element);

  HidePopup();

  // This behavior was introduced for to fix http://crbug.com/1105254. It's
  // unclear if this is still needed.
  auto handle_focus_change = [&]() {
    if ((config_.uses_keyboard_accessory_for_suggestions ||
         !config_.focus_requires_scroll) &&
        new_focused_element && unsafe_render_frame() &&
        unsafe_render_frame()->GetWebFrame()->HasTransientUserActivation()) {
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

  if (auto control = new_focused_element.DynamicTo<WebFormControlElement>()) {
    if (std::optional<FormAndField> form_and_field =
            FindFormAndFieldForFormControlElement(
                control, field_data_manager(),
                GetCallTimerState(kFocusedElementChanged),
                MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
      auto& [form, field] = *form_and_field;
      if (auto* autofill_driver = unsafe_autofill_driver()) {
        last_queried_element_ = FieldRef(control);
        autofill_driver->FocusOnFormField(form, field->renderer_id());
        handle_focus_change();
        return;
      }
    }
  }

  if (new_focused_element && new_focused_element.IsContentEditable()) {
    if (std::optional<FormData> form =
            form_util::FindFormForContentEditable(new_focused_element)) {
      CHECK_EQ(form->fields().size(), 1u);
      if (auto* autofill_driver = unsafe_autofill_driver()) {
        last_queried_element_ = FieldRef(new_focused_element);
        autofill_driver->FocusOnFormField(*form,
                                          form->fields().front().renderer_id());
        handle_focus_change();
        return;
      }
    }
  }

  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->FocusOnNonFormField();
    handle_focus_change();
  }
}

void AutofillAgent::ObserveCaret(WebElement element) {
  if (!base::FeatureList::IsEnabled(features::kAutofillCaretExtraction)) {
    return;
  }

  if (element && (element.IsContentEditable() ||
                  form_util::IsTextAreaElement(
                      element.DynamicTo<WebFormControlElement>()))) {
    caret_state_.remove_listener = element.GetDocument().AddEventListener(
        WebNode::EventType::kSelectionchange,
        base::BindRepeating(&AutofillAgent::HandleCaretMovedInFormField,
                            base::Unretained(this), element));
  } else {
    caret_state_.remove_listener = {};
    caret_state_.time_of_last_event = {};
    caret_state_.timer.Stop();
  }
}

void AutofillAgent::HandleCaretMovedInFormField(WebElement element,
                                                blink::WebDOMEvent) {
  auto handle_throttled_caret_change = [](AutofillAgent& self,
                                          WebElement element) {
    if (!self.unsafe_render_frame() || !element.Focused() ||
        !element.ContainsFrameSelection()) {
      return;
    }
    gfx::Rect caret_bounds = GetCaretBounds(*self.unsafe_render_frame());
    if (WebFormControlElement control =
            element.DynamicTo<WebFormControlElement>()) {
      if (std::optional<FormAndField> form_and_field =
              FindFormAndFieldForFormControlElement(
                  control, self.field_data_manager(),
                  self.GetCallTimerState(kHandleCaretMovedInFormField),
                  self.MaybeExtractDatalist(
                      {form_util::ExtractOption::kBounds}))) {
        auto& [form, field] = *form_and_field;
        if (auto* autofill_driver = self.unsafe_autofill_driver()) {
          autofill_driver->CaretMovedInFormField(form, field->renderer_id(),
                                                 caret_bounds);
          return;
        }
      }
    }
    if (element && element.IsContentEditable()) {
      if (std::optional<FormData> form =
              form_util::FindFormForContentEditable(element)) {
        CHECK_EQ(form->fields().size(), 1u);
        if (auto* autofill_driver = self.unsafe_autofill_driver()) {
          autofill_driver->CaretMovedInFormField(
              *form, form->fields().front().renderer_id(), caret_bounds);
          return;
        }
      }
    }
  };
  const base::Time now = base::Time::Now();
  const base::TimeDelta time_since_last = now - caret_state_.time_of_last_event;
  caret_state_.time_of_last_event = now;
  if (time_since_last < base::Milliseconds(100)) {
    caret_state_.timer.Start(FROM_HERE, base::Milliseconds(100),
                             base::BindOnce(handle_throttled_caret_change,
                                            std::ref(*this), element));
  } else {
    caret_state_.timer.Stop();
    handle_throttled_caret_change(*this, element);
  }
}

// AutofillAgent is deleted asynchronously because OnDestruct() may be
// triggered by JavaScript, which in turn may be triggered by AutofillAgent.
void AutofillAgent::OnDestruct() {
  receiver_.reset();
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
  const bool is_duplicate_submission_for_autofill = [&]() {
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
  const bool is_duplicate_submission_for_password_manager = [&]() {
    DenseSet<mojom::SubmissionSource> pwm_sources = sources;
    // PasswordManager doesn't consider FORM_SUBMISSION as a sufficient
    // condition for "successful" submission.
    pwm_sources.erase(mojom::SubmissionSource::FORM_SUBMISSION);
    if (base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
      // PasswordManager completely ignores PROBABLY_FORM_SUBMITTED.
      pwm_sources.erase(mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED);
    }
    return pwm_sources.size() > 1;
  }();

  if (!is_duplicate_submission_for_password_manager &&
      base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    password_autofill_agent_->FireHostSubmitEvent(form_data.renderer_id(),
                                                  source);
  }
  if (!is_duplicate_submission_for_autofill) {
    base::UmaHistogramEnumeration(kSubmissionSourceHistogram, source);
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FormSubmitted(form_data, known_success, source);
    }
  }
  // Bound the size of `submitted_forms_` to avoid possible memory leaks.
  if (submitted_forms_.size() > 200) {
    submitted_forms_.erase(--submitted_forms_.end());
  }
}

void AutofillAgent::TextFieldCleared(const WebFormControlElement& element) {
  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  CHECK(input_element || form_util::IsTextAreaElement(element));
  if (password_generation_agent_ && input_element) {
    password_generation_agent_->TextFieldCleared(input_element);
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
  password_autofill_agent_->FocusedElementChangedWithCustomSemantics(
      WebElement(), /*pass_key=*/{});
  if (password_generation_agent_) {
    password_generation_agent_->DidEndTextFieldEditing(element);
  }
}

void AutofillAgent::TextFieldDidChange(const WebFormControlElement& element) {
  form_tracker_->TextFieldDidChange(element);
}

void AutofillAgent::ContentEditableDidChange(const WebElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  // TODO(crbug.com/40286232): Add throttling to avoid sending this event for
  // rapid changes.
  if (std::optional<FormData> form =
          form_util::FindFormForContentEditable(element)) {
    CHECK_EQ(form->fields().size(), 1u);
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidChange(
          *form, form->fields().front().renderer_id(), base::TimeTicks::Now());
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
  if (password_generation_agent_ && input_element &&
      password_generation_agent_->TextDidChangeInTextField(input_element)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (input_element &&
      password_autofill_agent_->TextDidChangeInTextField(input_element)) {
    is_popup_possibly_visible_ = true;
    last_queried_element_ = FieldRef(element);
    return;
  }

  if (input_element) {
    ShowSuggestions(element,
                    AutofillSuggestionTriggerSource::kTextFieldDidChange);
  }

  if (std::optional<FormAndField> form_and_field =
          FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              GetCallTimerState(kOnTextFieldDidChange),
              MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldDidChange(form, field->renderer_id(),
                                          base::TimeTicks::Now());
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

  if (!element.GetDocument() || !is_popup_possibly_visible_ ||
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
  if (!element || !element.GetDocument()) {
    return;
  }

  OnProvisionallySaveForm(form_util::GetOwningForm(element), element,
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
  WebDocument document = GetDocument();
  if (!document) {
    return;
  }

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
      if (WebInputElement input_element =
              form_util::GetFormControlByRendererId(filled_field.GetId())
                  .DynamicTo<WebInputElement>()) {
        password_autofill_agent_->UpdatePasswordStateForTextChange(
            input_element);
      }
    }

    auto host_form_is_connected = [](const FormFieldData::FillData& fill_data) {
      return !form_util::GetFormByRendererId(fill_data.host_form_id).IsNull();
    };
    if (auto it = base::ranges::find_if(fields, host_form_is_connected);
        it != fields.end()) {
      base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking) &&
              base::FeatureList::IsEnabled(
                  features::kAutofillAcceptDomMutationAfterAutofillSubmission)
          ? TrackAutofilledElement(
                form_util::GetFormControlByRendererId(it->renderer_id))
          : UpdateLastInteractedElement(it->host_form_id);
    } else if (!base::FeatureList::IsEnabled(
                   features::kAutofillUnifyAndFixFormTracking)) {
      UpdateLastInteractedElement(FormRendererId());
    } else {
      for (const auto& [filled_field, state] : filled_fields) {
        if (WebFormControlElement control_element =
                form_util::GetFormControlByRendererId(filled_field.GetId())) {
          // `filled_fields` was populated at the same time where multiple focus
          // and blur events were dispatched. This means that many fields in the
          // list could have been removed from the DOM. Updating inside this
          // conditional ensures submission is always tracked with an element
          // currently connected to the DOM.
          base::FeatureList::IsEnabled(
              features::kAutofillAcceptDomMutationAfterAutofillSubmission)
              ? TrackAutofilledElement(control_element)
              : UpdateLastInteractedElement(
                    form_util::GetFieldRendererId(control_element));
        }
      }
    }

    formless_elements_were_autofilled_ |= std::ranges::any_of(
        filled_fields, [](const std::pair<FieldRef, WebAutofillState>& field) {
          WebFormControlElement element = field.first.GetField();
          return element && !form_util::GetOwningForm(element);
        });

    base::flat_set<FormRendererId> extracted_form_ids;
    std::vector<FormData> filled_forms;
    for (const FormFieldData::FillData& field : fields) {
      if (extracted_form_ids.insert(field.host_form_id).second) {
        std::optional<FormData> form = form_util::ExtractFormData(
            document, form_util::GetFormByRendererId(field.host_form_id),
            field_data_manager(), GetCallTimerState(kApplyFieldsAction));
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
  WebDocument document = GetDocument();
  if (!document) {
    return;
  }
  for (const auto& form : forms) {
    ShowPredictions(document, form, attach_predictions_to_dom);
  }
}

void AutofillAgent::ClearPreviewedForm() {
  // `password_generation_agent_` can be null in WebView.
  // TODO(crbug.com/326213028): Clear fields previewed by
  // `PasswordGenerationAgent` directly using `PasswordGenerationAgent`.
  if (password_generation_agent_) {
    password_generation_agent_->ClearPreviewedForm();
  }
  // TODO(crbug.com/326213028): Clear fields previewed by
  // `PasswordAutofillAgent` directly using `PasswordAutofillAgent`.
  password_autofill_agent_->ClearPreviewedForm();

  std::vector<std::pair<WebFormControlElement, WebAutofillState>>
      previewed_elements;
  for (const auto& [previewed_element, prior_autofill_state] :
       previewed_elements_) {
    if (WebFormControlElement field = previewed_element.GetField()) {
      previewed_elements.emplace_back(field, prior_autofill_state);
    }
  }
  form_util::ClearPreviewedElements(previewed_elements);
  previewed_elements_ = {};
}

void AutofillAgent::TriggerSuggestions(
    FieldRendererId field_id,
    AutofillSuggestionTriggerSource trigger_source) {
  if (WebFormControlElement control_element =
          form_util::GetFormControlByRendererId(field_id)) {
    last_queried_element_ = FieldRef(control_element);
    ShowSuggestions(control_element, trigger_source);
    return;
  }
  if (trigger_source ==
          AutofillSuggestionTriggerSource::kComposeDialogLostFocus ||
      trigger_source ==
          AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge) {
    if (WebElement content_editable =
            form_util::GetContentEditableByRendererId(field_id)) {
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
  if (form_control && form_util::IsTextAreaElementOrTextInput(form_control)) {
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
            previewed_elements_.emplace_back(form_control,
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
        // AutofillAgent::DoFillFieldWithValue dispatches many events that can
        // trigger JS and therefore disconnect `form_control` from the DOM or
        // delete the frame. Therefore we apply this GetElement(GetId(element))
        // pattern in order to ensure we're not holding a reference to a
        // disconnected element.
        form_control = form_util::GetFormControlByRendererId(
            form_util::GetFieldRendererId(form_control));
        if (form_control && base::FeatureList::IsEnabled(
                                features::kAutofillUnifyAndFixFormTracking)) {
          if (WebFormElement form_element =
                  form_util::GetOwningForm(form_control)) {
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
          form_util::GetContentEditableByRendererId(field_id)) {
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
  if (!last_queried_element ||
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
  if (!last_queried_element ||
      field_id != form_util::GetFieldRendererId(last_queried_element)) {
    return;
  }

  WebInputElement input_element =
      last_queried_element.DynamicTo<WebInputElement>();
  if (!input_element) {
    // Early return for non-input fields such as textarea.
    return;
  }
  std::u16string new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion. We intentionally use `FormControlType()` instead of
  // `FormControlTypeForAutofill()` because it does not matter here if the field
  // has ever been a password field before.
  if (input_element.IsMultiple() &&
      input_element.FormControlType() ==  // nocheck
          blink::mojom::FormControlType::kInputEmail) {
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
  if (!last_queried_element) {
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
  if (!form_util::IsTextAreaElementOrTextInput(element)) {
    return;
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
  // TODO(crbug.com/333990908): Test manual fallback on different form types.
  if (IsAddressAutofillManuallyTriggered(trigger_source) ||
      IsPaymentsAutofillManuallyTriggered(trigger_source) ||
      IsPlusAddressesManuallyTriggered(trigger_source)) {
    QueryAutofillSuggestions(element, trigger_source);
    return;
  }

  // Proceed with generating suggestions based on the field type.
  if (const WebInputElement input_element =
          element.DynamicTo<WebInputElement>()) {
    if (IsPasswordsAutofillManuallyTriggered(trigger_source)) {
      is_popup_possibly_visible_ = password_autofill_agent_->ShowSuggestions(
          input_element, trigger_source);
      return;
    }
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

    // Password field elements should only have suggestions shown by the
    // password AutofillAgent. We call `FormControlType()` instead of
    // `FormControlTypeForAutofill()` because we are interested in whether the
    // field is *currently* a password field, not whether it has ever been a
    // password field.
    if (input_element.FormControlType() ==  // nocheck
            blink::mojom::FormControlType::kInputPassword &&
        !config_.query_password_suggestions) {
      return;
    }
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
  CHECK_EQ(form->fields().size(), 1u);
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    is_popup_possibly_visible_ = true;
    if (auto* render_frame = unsafe_render_frame()) {
      autofill_driver->AskForValuesToFill(
          *form, form->fields()[0].renderer_id(), GetCaretBounds(*render_frame),
          trigger_source);
    }
  }
}

void AutofillAgent::GetPotentialLastFourCombinationsForStandaloneCvc(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  WebDocument document = GetDocument();
  if (!document) {
    std::vector<std::string> matches;
    std::move(potential_matches).Run(matches);
  } else {
    form_util::TraverseDomForFourDigitCombinations(
        document, std::move(potential_matches));
  }
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  DCHECK(element.DynamicTo<WebInputElement>() ||
         form_util::IsTextAreaElement(element));

  std::optional<FormAndField> form_and_field =
      form_util::FindFormAndFieldForFormControlElement(
          element, field_data_manager(),
          GetCallTimerState(kQueryAutofillSuggestions),
          {form_util::ExtractOption::kDatalist,
           form_util::ExtractOption::kBounds});
  if (!form_and_field) {
    return;
  }
  auto& [form, field] = *form_and_field;

  if (config_.secure_context_required &&
      !element.GetDocument().IsSecureContext()) {
    LOG(WARNING) << "Autofill suggestions are disabled because the document "
                    "isn't a secure context.";
    return;
  }

  is_popup_possibly_visible_ = true;
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    if (auto* render_frame = unsafe_render_frame()) {
      autofill_driver->AskForValuesToFill(form, field->renderer_id(),
                                          GetCaretBounds(*render_frame),
                                          trigger_source);
    }
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
  WebDocument document = GetDocument();
  if (!document) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  DenseSet<form_util::ExtractOption> extract_options =
      MaybeExtractDatalist({form_util::ExtractOption::kBounds});
  if (!form_id) {
    if (std::optional<FormData> form = form_util::ExtractFormData(
            document, WebFormElement(), field_data_manager(),
            GetCallTimerState(kExtractForm), extract_options)) {
      std::move(callback).Run(std::move(form));
      return;
    }
  }
  if (WebFormElement form_element = form_util::GetFormByRendererId(form_id)) {
    if (std::optional<FormData> form = form_util::ExtractFormData(
            document, form_element, field_data_manager(),
            GetCallTimerState(kExtractForm), extract_options)) {
      std::move(callback).Run(std::move(form));
      return;
    }
  }
  if (WebElement contenteditable = form_util::GetContentEditableByRendererId(
          FieldRendererId(*form_id))) {
    std::move(callback).Run(
        form_util::FindFormForContentEditable(contenteditable));
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
  content::RenderFrame* render_frame = unsafe_render_frame();
  if (!render_frame) {
    if (!callback.is_null()) {
      std::move(callback).Run(/*success=*/false);
    }
    return;
  }
  FormCache::UpdateFormCacheResult cache =
      form_cache_.UpdateFormCache(field_data_manager());
  form_issues::MaybeEmitFormIssuesToDevtools(*render_frame->GetWebFrame(),
                                             cache.updated_forms);
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
  WebDocument document = GetDocument();
  if (!document) {
    return;
  }
  if (WebElement focused_element = document.FocusedElement()) {
    password_autofill_agent_->FocusedElementChangedWithCustomSemantics(
        focused_element,
        /*pass_key=*/{});
    if (auto input_element = focused_element.DynamicTo<WebInputElement>()) {
      field_data_manager_->UpdateFieldDataMapWithNullValue(
          form_util::GetFieldRendererId(input_element),
          FieldPropertiesFlags::kHadFocus);
    }
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
  DCHECK(node);
  WebElement contenteditable;
  const bool is_focused =
      node.Focused() || ((contenteditable = node.RootEditableElement()) &&
                         contenteditable.Focused());
#if defined(ANDROID)
  HandleFocusChangeComplete(/*focused_node_was_last_clicked=*/is_focused);
#else
  last_left_mouse_down_or_gesture_tap_in_node_caused_focus_ = is_focused;
#endif
}

void AutofillAgent::SelectControlDidChange(
    const WebFormControlElement& element) {
  form_tracker_->SelectControlDidChange(element);
}

// Notifies the AutofillDriver about changes in the <select>
// options in batches.
//
// A batch ends if no event occurred for `kWaitTimeForOptionsChanges`. For a
// given batch, the AutofillDriver is informed only about the last FormData.
// That is, if within one batch the options of different forms changed, all but
// one of these events will be lost.
void AutofillAgent::SelectFieldOptionsChanged(
    const WebFormControlElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (!was_last_action_fill_ || !last_queried_element_.GetField()) {
    return;
  }

  if (select_option_change_batch_timer_.IsRunning()) {
    select_option_change_batch_timer_.AbandonAndStop();
  }

  select_option_change_batch_timer_.Start(
      FROM_HERE, kWaitTimeForOptionsChanges,
      base::BindRepeating(&AutofillAgent::BatchSelectOptionChange,
                          base::Unretained(this),
                          form_util::GetFieldRendererId(element)));
}

void AutofillAgent::BatchSelectOptionChange(FieldRendererId element_id) {
  WebFormControlElement element =
      form_util::GetFormControlByRendererId(element_id);
  if (!element) {
    return;
  }

  // Look for the form and field associated with the select element. If they are
  // found, notify the driver that the form was modified dynamically.
  if (std::optional<FormAndField> form_and_field =
          form_util::FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              GetCallTimerState(kBatchSelectOptionChange),
              /*extract_options=*/{})) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver();
        autofill_driver && !field->options().empty()) {
      autofill_driver->SelectFieldOptionsDidChange(form);
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
  WebDocument document = GetDocument();
  if (!document) {
    return;
  }

  // When using Talkback on Android, and possibly others, traversing to and
  // focusing a field will not register as a click. Thus, when screen readers
  // are used, treat the focused node as if it was the last clicked.
  focused_node_was_last_clicked |= is_screen_reader_enabled_;

  WebElement focused_element = document.FocusedElement();
  if (!focused_element) {
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

  // Preventing suggestions shown on contenteditable for right click or
  // non-click focus.
  // TODO(crbug.com/40284726): This seems to be redundant. Remove call to
  // ShowSuggestionsForContentEditable.
  if (focused_node_was_last_clicked) {
    ShowSuggestionsForContentEditable(
        focused_element,
        AutofillSuggestionTriggerSource::kContentEditableClicked);
  }
}

void AutofillAgent::AjaxSucceeded() {
  form_tracker_->AjaxSucceeded();
}

void AutofillAgent::JavaScriptChangedValue(WebFormControlElement element,
                                           const WebString& old_value,
                                           bool was_autofilled) {
  if (!element.IsConnected()) {
    return;
  }
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
    std::vector<FormFieldData> fields =
        provisionally_saved_form()->ExtractFields();
    if (auto it =
            base::ranges::find(fields, form_util::GetFieldRendererId(element),
                               &FormFieldData::renderer_id);
        it != fields.end()) {
      it->set_value(element.Value().Utf16());
      it->set_is_autofilled(element.IsAutofilled());
      form_util::MaybeUpdateUserInput(
          *it, form_util::GetFieldRendererId(element), field_data_manager());
    }
    provisionally_saved_form()->set_fields(std::move(fields));
  }

  const auto input_element = element.DynamicTo<WebInputElement>();
  if (input_element && !element.Value().IsEmpty() &&
      (input_element.FormControlTypeForAutofill() ==
           blink::mojom::FormControlType::kInputPassword ||
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
              GetCallTimerState(kJavaScriptChangedValue),
              /*extract_options=*/{})) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->JavaScriptChangedAutofilledValue(
          form, field->renderer_id(), old_value.Utf16(), formatting_only);
    }
  }
}

void AutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form_element,
    const WebFormControlElement& element,
    SaveFormReason source) {
  DCHECK(form_util::MaybeWasOwnedByFrame(form_element, unsafe_render_frame()));
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  WebDocument document = GetDocument();
  if (!document) {
    return;
  }

  // Updates cached data needed for submission so that we only cache the latest
  // version of the to-be-submitted form.
  auto update_submission_data_on_user_edit = [&]() {
    if (form_element) {
      UpdateLastInteractedElement(form_util::GetFormRendererId(form_element));
      return;
    }
    std::erase_if(formless_elements_user_edited_,
                  [](const FieldRendererId field_id) {
                    WebFormControlElement field =
                        form_util::GetFormControlByRendererId(field_id);
                    return field &&
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
        // TODO(crbug.com/40281981): Figure out if this is still needed, and
        // document the reason, otherwise remove.
        password_autofill_agent_->InformBrowserAboutUserInput(
            form_element, WebInputElement());
        // TODO(crbug.com/40281981): Figure out if this is still needed, and
        // document the reason, otherwise remove.
        update_submission_data_on_user_edit();
      }
      if (submitted_forms_[form_util::GetFormRendererId(form_element)].contains(
              mojom::SubmissionSource::FORM_SUBMISSION) &&
          base::FeatureList::IsEnabled(
              features::kAutofillUnifyAndFixFormTracking)) {
        // Save an extraction call since the submission will be ignored anyways
        // by the duplicate submission filtering logic.
        break;
      }
      if (std::optional<FormData> form_data = form_util::ExtractFormData(
              document, form_element, field_data_manager(),
              GetCallTimerState(kOnProvisionallySaveForm))) {
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
                  GetCallTimerState(kOnProvisionallySaveForm),
                  MaybeExtractDatalist({form_util::ExtractOption::kBounds}))) {
        auto& [form, field] = *form_and_field;
        if (auto* autofill_driver = unsafe_autofill_driver()) {
          autofill_driver->SelectControlDidChange(form, field->renderer_id());
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
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
    ResetLastInteractedElements();
    OnFormNoLongerSubmittable();
  }
}

void AutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  DCHECK(form_util::MaybeWasOwnedByFrame(form, unsafe_render_frame()));
  if (submitted_forms_[form_util::GetFormRendererId(form)].contains(
          mojom::SubmissionSource::FORM_SUBMISSION) &&
      base::FeatureList::IsEnabled(
          features::kAutofillUnifyAndFixFormTracking)) {
    // Save an extraction call since the submission will be ignored anyways
    // by the duplicate submission filtering logic.
    return;
  }
  // Fire the submission event here because WILL_SEND_SUBMIT_EVENT is skipped
  // if javascript calls submit() directly.
  if (std::optional<FormData> form_data = form_util::ExtractFormData(
          form.GetDocument(), form, field_data_manager(),
          GetCallTimerState(kOnFormSubmitted))) {
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
      NOTREACHED();
    case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      if (base::FeatureList::IsEnabled(
              features::kAutofillUnifyAndFixFormTracking)) {
        password_autofill_agent_->FireHostSubmitEvent(
            FormRendererId(),
            mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
        if (std::optional<FormData> form_data = GetSubmittedForm();
            form_data &&
            base::FeatureList::IsEnabled(
                features::kAutofillAcceptDomMutationAfterAutofillSubmission)) {
          FireHostSubmitEvents(
              *form_data, /*known_success=*/true,
              mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
        }
      }
      // `BrowserAutofillManager` ignores submissions with
      // DOM_MUTATION_AFTER_AUTOFILL as a source, therefore we early return in
      // this case as to not call `AutofillAgent::ResetLastInteractedElements()`
      // which could cause us to miss a submission that BAM actually cares
      // about.
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
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
    ResetLastInteractedElements();
    OnFormNoLongerSubmittable();
  }
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
  if (!input_element || !input_element.IsTextField()) {
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
  WebDocument document = GetDocument();
  if ((!user_autofilled_or_edited_owned_form && !user_autofilled_unowned_form &&
       !user_edited_unowned_form) ||
      !document) {
    return std::nullopt;
  }
  // Extracts the last-interacted form, with fallback to its last-saved state.
  std::optional<FormData> form = form_util::ExtractFormData(
      document, last_interacted_form().GetForm(), field_data_manager(),
      GetCallTimerState(kGetSubmittedForm));
  return !form || (user_edited_unowned_form &&
                   std::ranges::none_of(form->fields(), has_been_user_edited))
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
    WebDocument document = GetDocument();
    provisionally_saved_form() =
        document ? form_util::ExtractFormData(
                       document,
                       form_util::GetFormByRendererId(
                           absl::get<FormRendererId>(element_id)),
                       field_data_manager(),
                       GetCallTimerState(kUpdateLastInteractedElement))
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

}  // namespace autofill
