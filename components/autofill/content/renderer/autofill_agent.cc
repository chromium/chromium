// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/json/string_escape.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/optional_ref.h"
#include "base/types/zip.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/a11y_utils.h"
#include "components/autofill/content/renderer/form_autofill_issues.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/suggestion_properties.h"
#include "components/autofill/content/renderer/synchronous_form_cache.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_debug_features.h"
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
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
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

// Time to wait in ms to ensure that only a single select or datalist change
// will be acted upon, instead of multiple in close succession (debounce time).
constexpr base::TimeDelta kWaitTimeForOptionsChanges = base::Milliseconds(50);

using FormAndField = std::pair<FormData, raw_ref<const FormFieldData>>;

void LogRendererExtractLabeledTextNodeValueLatency(base::TimeDelta latency,
                                                   bool is_successful) {
  base::UmaHistogramTimes(
      base::StrCat({"Autofill.RendererLabeledAmountExtractionLatency.",
                    is_successful ? "Success" : "Failure"}),
      latency);
}

// For each field in the |form| sets the title to include the field's heuristic
// type, server type, and signature; as well as the form's signature and the
// experiment id for the server predictions.
bool ShowPredictions(const WebDocument& document,
                     const FormDataPredictions& form) {
  CHECK(base::FeatureList::IsEnabled(
      features::debug::kAutofillShowTypePredictions));
  CHECK_EQ(form.data.fields().size(), form.fields.size());

  WebFormElement form_element =
      form_util::GetFormByRendererId(form.data.renderer_id());
  std::vector<WebFormControlElement> control_elements =
      form_util::GetOwnedAutofillableFormControls(document, form_element);
  if (control_elements.size() != form.fields.size()) {
    // Keep things simple.  Don't show predictions for forms that were modified
    // between page load and the server's response to our query.
    return false;
  }

  for (auto [element, field_data, field] :
       base::zip(control_elements, form.data.fields(), form.fields)) {
    if (form_util::GetFieldRendererId(element) != field_data.renderer_id()) {
      continue;
    }

    // If the flag is enabled, attach the prediction to the field.
    constexpr size_t kMaxLabelSize = 100;
    std::string label =
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForParsingWithSharedLabels)
            ? field.parseable_label
            : base::UTF16ToUTF8(field_data.label());
    std::string truncated_label = label.substr(0, kMaxLabelSize);
    // The label may be derived from the placeholder attribute and may contain
    // line wraps which are normalized here.
    base::ReplaceChars(truncated_label, "\n", "|", &truncated_label);

    std::string form_id = base::NumberToString(form.data.renderer_id().value());
    std::string field_id_str =
        base::NumberToString(field_data.renderer_id().value());

    blink::LocalFrameToken frame_token;
    if (auto* frame = element.GetDocument().GetFrame()) {
      frame_token = frame->GetLocalFrameToken();
    }

    std::string autofill_info = base::StrCat({
        "overall type: ",
        field.overall_type,
        "\nhtml type: ",
        field.html_type,
        "\nserver type: ",
        field.server_type.has_value() ? field.server_type.value()
                                      : "SERVER_RESPONSE_PENDING",
        "\nheuristic type: ",
        field.heuristic_type,
        "\npwm ml type: ",
        field.pwm_ml_type,
        (!field.attribute_types.empty() ? "\nautofill ai attribute types: "
                                        : ""),
        (!field.attribute_types.empty() ? field.attribute_types : ""),
        (!field.format_string.empty() ? "\nformat string: " : ""),
        (!field.format_string.empty() ? field.format_string : ""),
        "\nlabel: ",
        truncated_label,
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
        "\nstructural form signature: ",
        form.structural_form_signature,
        "\nform name: ",
        base::UTF16ToUTF8(form.data.name_attribute()),
        "\nform id: ",
        base::UTF16ToUTF8(form.data.id_attribute()),
        "\nform button titles: ",
        base::UTF16ToUTF8(GetButtonTitlesString(form_util::GetButtonTitles(
            form_element, /*button_titles_cache=*/nullptr))),
        "\nfield frame token: ",
        frame_token.ToString(),
        "\nform renderer id: ",
        form_id,
        "\nfield renderer id: ",
        field_id_str,
        "\nvisible: ",
        base::ToString(field_data.is_visible()),
        "\nfocusable: ",
        base::ToString(field_data.IsFocusable()),
        "\nfield rank: ",
        base::NumberToString(field.rank),
        "\nfield rank in signature group: ",
        base::NumberToString(field.rank_in_signature_group),
        "\nfield rank in host form: ",
        base::NumberToString(field.rank_in_host_form),
        "\nfield rank in host form signature group: ",
        base::NumberToString(field.rank_in_host_form_signature_group),
    });

    if (features::debug::kAutofillShowTypePredictionsVerboseParam.Get()) {
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
      for (size_t option_index = 0; option_index < field_data.options().size();
           option_index++) {
        const SelectOption& select_option = field_data.options()[option_index];
        const std::string delimiter = option_index > 0 ? "|" : "";
        option_labels =
            option_labels + delimiter + base::UTF16ToUTF8(select_option.text);
        option_values =
            option_values + delimiter + base::UTF16ToUTF8(select_option.value);
      }

      autofill_info = base::StrCat({
          autofill_info,
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
          "\nax node id: ",
          base::NumberToString(field_data.form_control_ax_id()),
      });
    }

    WebString kAutocomplete = WebString::FromASCII("autocomplete");
    if (element.HasAttribute(kAutocomplete)) {
      autofill_info +=
          "\nautocomplete: " +
          element.GetAttribute(kAutocomplete).Utf8().substr(0, 100);
    }

    // Set the same debug string to an attribute that does not get mangled if
    // Google Translate is triggered for the site. This is useful for
    // automated processing of the data.
    element.SetAttribute("autofill-information",
                         WebString::FromUTF8(autofill_info));

    //  If the field has password manager's annotation, add it as well.
    if (element.HasAttribute("pm_parser_annotation")) {
      autofill_info =
          base::StrCat({autofill_info, "\npm_parser_annotation: ",
                        element.GetAttribute("pm_parser_annotation").Utf8()});
    }

    // Set this debug string so that a developer can easily debug the element.
    // If the flag is on with parameter :as-title, information will be found as
    // 'title' in the DOM of the element.
    bool title_parameter_on =
        features::debug::kAutofillShowTypePredictionsAsTitleParam.Get();
    if (title_parameter_on) {
      element.SetAttribute("title", WebString::FromUTF8(autofill_info));
    }

    element.SetAttribute("autofill-prediction",
                         WebString::FromUTF8(field.overall_type));
  }
  return true;
}

// TODO(crbug.com/402071086): Remove when AutofillIgnoreCheckableElements is
// removed.
bool IsCheckableElement(const WebFormControlElement& element) {
  using enum blink::mojom::FormControlType;
  return element && (element.FormControlTypeForAutofill() == kInputCheckbox ||
                     element.FormControlTypeForAutofill() == kInputRadio);
}

gfx::Rect GetCaretBounds(content::RenderFrame& frame) {
  if (auto* frame_widget = frame.GetWebFrame()->LocalRoot()->FrameWidget()) {
    gfx::Rect anchor;
    gfx::Rect focus;
    frame_widget->CalculateSelectionBounds(anchor, focus);
    return frame.ConvertViewportToWindow(focus);
  }
  return gfx::Rect();
}

AutofillAgent::Config CreateConfig(bool uses_platform_autofill) {
  if (uses_platform_autofill) {
    return {
        AutofillAgent::ExtractAllDatalists(true),
        AutofillAgent::FocusRequiresScroll(false),
        AutofillAgent::QueryPasswordSuggestions(true),
        AutofillAgent::SecureContextRequired(true),
        AutofillAgent::UserGestureRequired(false),
        AutofillAgent::UsesKeyboardAccessoryForSuggestions(false),
    };
  }
  return {
      AutofillAgent::ExtractAllDatalists(false),
      AutofillAgent::FocusRequiresScroll(true),
      AutofillAgent::QueryPasswordSuggestions(false),
      AutofillAgent::SecureContextRequired(false),
      AutofillAgent::UserGestureRequired(true),
      AutofillAgent::UsesKeyboardAccessoryForSuggestions(BUILDFLAG(IS_ANDROID)),
  };
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
                     mojom::SubmissionSource source) override {
    DeferMsg(&mojom::AutofillDriver::FormSubmitted, form, source);
  }
  void CaretMovedInFormField(const FormData& form,
                             FieldRendererId field_id,
                             const gfx::Rect& caret_bounds) override {
    DeferMsg(&mojom::AutofillDriver::CaretMovedInFormField, form, field_id,
             caret_bounds);
  }
  void TextFieldValueChanged(const FormData& form,
                             FieldRendererId field_id,
                             base::TimeTicks timestamp) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldValueChanged, form, field_id,
             timestamp);
  }
  void TextFieldDidScroll(const FormData& form,
                          FieldRendererId field_id) override {
    DeferMsg(&mojom::AutofillDriver::TextFieldDidScroll, form, field_id);
  }
  void SelectControlSelectionChanged(const FormData& form,
                                     FieldRendererId field_id) override {
    DeferMsg(&mojom::AutofillDriver::SelectControlSelectionChanged, form,
             field_id);
  }
  void SelectFieldOptionsDidChange(const FormData& form,
                                   FieldRendererId field_id) override {
    DeferMsg(&mojom::AutofillDriver::SelectFieldOptionsDidChange, form,
             field_id);
  }
  void AskForValuesToFill(const FormData& form,
                          FieldRendererId field_id,
                          const gfx::Rect& caret_bounds,
                          AutofillSuggestionTriggerSource trigger_source,
                          const std::optional<PasswordSuggestionRequest>&
                              password_request) override {
    DeferMsg(&mojom::AutofillDriver::AskForValuesToFill, form, field_id,
             caret_bounds, trigger_source,
             base::FeatureList::IsEnabled(
                 features::kAutofillAndPasswordsInSameSurface)
                 ? password_request
                 : std::nullopt);
  }
  void HidePopup() override { DeferMsg(&mojom::AutofillDriver::HidePopup); }
  void FocusOnNonFormField() override {
    DeferMsg(&mojom::AutofillDriver::FocusOnNonFormField);
  }
  void FocusOnFormField(const FormData& form,
                        FieldRendererId field_id) override {
    DeferMsg(&mojom::AutofillDriver::FocusOnFormField, form, field_id);
  }
  void DidAutofillForm(const FormData& form) override {
    DeferMsg(&mojom::AutofillDriver::DidAutofillForm, form);
  }
  void DidEndTextFieldEditing() override {
    DeferMsg(&mojom::AutofillDriver::DidEndTextFieldEditing);
  }
  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      FieldRendererId field_id,
      const std::u16string& old_value) override {
    DeferMsg(&mojom::AutofillDriver::JavaScriptChangedAutofilledValue, form,
             field_id, old_value);
  }

  const raw_ref<AutofillAgent> agent_;
  base::WeakPtrFactory<DeferringAutofillDriver> weak_ptr_factory_{this};
};

AutofillAgent::AutofillAgent(
    content::RenderFrame* render_frame,
    std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
    std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      config_(CreateConfig(render_frame->GetWebView()
                               ->GetRendererPreferences()
                               .uses_platform_autofill)),
      password_autofill_agent_(std::move(password_autofill_agent)),
      password_generation_agent_(std::move(password_generation_agent)),
      replace_form_element_observer_(base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
  render_frame->GetWebFrame()->SetAutofillClient(this);
  password_autofill_agent_->Init(this);
  form_tracker_ = std::make_unique<FormTracker>(unsafe_render_frame(), *this,
                                                *password_autofill_agent_);
  form_tracker_->SetUserGestureRequired(config_.user_gesture_required);
  registry->AddInterface<mojom::AutofillAgent>(base::BindRepeating(
      &AutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

// The destructor is not guaranteed to be called. Destruction happens (only)
// through the OnDestruct() event, which posts a task to delete this object.
// The process may be killed before this deletion can happen.
AutofillAgent::~AutofillAgent() = default;

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
  select_option_change_batch_timer_.clear();
  datalist_option_change_batch_timer_.Stop();
  process_forms_after_dynamic_change_timer_.Stop();
  process_forms_form_extraction_timer_.Stop();
  process_forms_form_extraction_with_response_timer_.Stop();
  form_tracker_->ResetLastInteractedElements();
  form_tracker_->OnFormNoLongerSubmittable();
  timing_ = {};
}

void AutofillAgent::DidDispatchDOMContentLoadedEvent() {
  base::UmaHistogramBoolean("Autofill.DOMContentLoadedInOutermostMainFrame",
                            unsafe_render_frame()->IsMainFrame() &&
                                !unsafe_render_frame()->IsInFencedFrameTree());
  is_dom_content_loaded_ = true;
  timing_.last_dom_content_loaded = base::TimeTicks::Now();
  ExtractFormsUnthrottled(/*callback=*/{},
                          GetCallTimerState(kDidDispatchDomContentLoadedEvent));
  password_autofill_agent_->DispatchedDOMContentLoadedEvent(
      SynchronousFormCache(form_cache_.extracted_forms()));
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
          form_util::FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              GetCallTimerState(kDidChangeScrollOffsetImpl),
              button_titles_cache(),
              /*form_cache=*/{})) {
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
  auto handle_focus_change = [&](base::optional_ref<FormData> extracted_form =
                                     std::nullopt) {
    if (((config_.uses_keyboard_accessory_for_suggestions &&
          !base::FeatureList::IsEnabled(
              features::kAutofillAndroidKeyboardAccessoryDynamicPositioning)) ||
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
          /*focused_node_was_last_clicked=*/focused_node_was_last_clicked,
          extracted_form && GetDocument() &&
                  // Sometimes Autofill receives FocusedElementChanged signals
                  // where `new_focused_element` is different from
                  // `document.FocusedElement()`. In those cases we shouldn't
                  // cache the form because it might be different from the one
                  // that will be used later.
                  // TODO(crbug.com/40947729): Figure out why this happens and
                  // document the reason.
                  new_focused_element == GetDocument().FocusedElement()
              ? SynchronousFormCache(*extracted_form)
              : SynchronousFormCache());
    }
  };

  if (auto control = new_focused_element.DynamicTo<WebFormControlElement>()) {
    if (std::optional<FormAndField> form_and_field =
            form_util::FindFormAndFieldForFormControlElement(
                control, field_data_manager(),
                GetCallTimerState(kFocusedElementChanged),
                button_titles_cache(),
                /*form_cache=*/{})) {
      auto& [form, field] = *form_and_field;
      if (auto* autofill_driver = unsafe_autofill_driver()) {
        last_queried_element_ = FieldRef(control);
        autofill_driver->FocusOnFormField(form, field->renderer_id());
        handle_focus_change(form);
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
              form_util::FindFormAndFieldForFormControlElement(
                  control, self.field_data_manager(),
                  self.GetCallTimerState(kHandleCaretMovedInFormField),
                  self.button_titles_cache(),
                  /*form_cache=*/{})) {
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
                                         mojom::SubmissionSource source) {
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->FormSubmitted(form_data, source);
  }
}

bool AutofillAgent::TryShowPasswordSuggestions(
    const WebInputElement& input,
    IsPasswordRequestManuallyTriggered manually_triggered_password_request,
    base::optional_ref<const PasswordSuggestionRequest> password_request) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillAndPasswordsInSameSurface)) {
    // No update to `is_popup_possibly_visible_` yet: it could still be open.
    return false;
  }

  const bool is_field_empty_or_autofilled =
      input.IsAutofilled() || input.Value().IsEmpty();
  const bool is_password_field = input.FormControlTypeForAutofill() ==
                                 blink::mojom::FormControlType::kInputPassword;

  // Only show password suggestions on password-type fields if the field is
  // either empty or autofilled. This will effectively close the popup on a
  // password-type field once the user starts typing.
  if (is_password_field && !is_field_empty_or_autofilled) {
    HidePopup();
    return false;
  }

  if (password_request) {
    password_autofill_agent_->ShowSuggestions(password_request.value());
    is_popup_possibly_visible_ = true;
    return true;
  }
  // Beyond this point, the renderer won't be called. Earlier renderer calls may
  // have shown/suppressed popups, so update visibility & success of this call.

  // Treat the popup as (still) visible if
  //  - a suggestion was accepted on another field, or if
  //  - it was already open and no manual request force-closes the popup.
  is_popup_possibly_visible_ =
      password_autofill_agent_->HasAcceptedSuggestionOnOtherField(input) ||
      (is_popup_possibly_visible_ && !manually_triggered_password_request);

  // Call `FormControlType()` instead of `FormControlTypeForAutofill()` to
  // determine whether the focsed field is *currently* a password field, not
  // whether it has ever been a password field.
  bool is_password_field_now = input.FormControlType() ==  // nocheck
                               blink::mojom::FormControlType::kInputPassword;

  // Return whether the password autofill agent has handled this request. Above,
  // we already returned true if suggestions were shown. But there are several
  // cases were the AutofillAgent should not show non-password Autofill:
  //   a) when the user request password explicitly.
  //   b) when the focused field is a password field (right now).
  // Special condition for b: if the autofill agent handles all requests, don't
  // defer to the password agent either.
  // TODO: crbug.com/410753794 - Check if an early return works better here.
  return manually_triggered_password_request        // --> case a.
         || (is_password_field_now &&               // --> case b.
             !config_.query_password_suggestions);  // --> case b without PWM.
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

void AutofillAgent::TextFieldValueChanged(
    const WebFormControlElement& element) {
  field_data_manager_->UpdateFieldDataMap(
      form_util::GetFieldRendererId(element), element.Value().Utf16(),
      FieldPropertiesFlags::kUserTyped);
  form_tracker_->TextFieldValueChanged(element);
}

void AutofillAgent::ContentEditableDidChange(const WebElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  // TODO(crbug.com/40286232): Add throttling to avoid sending this event for
  // rapid changes.
  if (std::optional<FormData> form =
          form_util::FindFormForContentEditable(element)) {
    CHECK_EQ(form->fields().size(), 1u);
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldValueChanged(
          *form, form->fields().front().renderer_id(), base::TimeTicks::Now());
    }
  }
}

void AutofillAgent::OnTextFieldValueChanged(
    const WebFormControlElement& element,
    const SynchronousFormCache& form_cache) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  // TODO(crbug.com/40286232): Add throttling to avoid sending this event for
  // rapid changes.

  // The field might have changed while the user was hovering on a suggestion,
  // the preview in that case should be cleared since new suggestions will be
  // showing up.
  ClearPreviewedForm();

  const auto input_element = element.DynamicTo<WebInputElement>();
  if (input_element && input_element.IsTextField()) {
    password_autofill_agent_->UpdatePasswordStateForTextChange(input_element,
                                                               form_cache);
  }

  if (password_generation_agent_ && input_element &&
      password_generation_agent_->TextDidChangeInTextField(input_element,
                                                           form_cache)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (input_element) {
    std::optional<PasswordSuggestionRequest> password_request =
        password_autofill_agent_->CreateRequestForChangeInTextField(
            input_element, form_cache);
    if (password_request &&
        TryShowPasswordSuggestions(input_element,
                                   IsPasswordRequestManuallyTriggered(false),
                                   password_request.value())) {
      last_queried_element_ = FieldRef(element);
      return;
    }

    ShowSuggestions(element,
                    AutofillSuggestionTriggerSource::kTextFieldValueChanged,
                    form_cache, password_request);
  }

  if (std::optional<FormAndField> form_and_field =
          form_util::FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              GetCallTimerState(kOnTextFieldValueChanged),
              button_titles_cache(), form_cache)) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->TextFieldValueChanged(form, field->renderer_id(),
                                             base::TimeTicks::Now());
    }
  }
}

void AutofillAgent::OnSelectControlSelectionChanged(
    const WebFormControlElement& element,
    const SynchronousFormCache& form_cache) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  if (std::optional<FormAndField> form_and_field =
          form_util::FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              GetCallTimerState(kOnProvisionallySaveForm),
              button_titles_cache(), form_cache)) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->SelectControlSelectionChanged(form,
                                                     field->renderer_id());
    }
  }
}

void AutofillAgent::TextFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));

  if (event.windows_key_code == ui::VKEY_DOWN ||
      event.windows_key_code == ui::VKEY_UP) {
    ShowSuggestions(
        element, AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown,
        /*form_cache=*/{},
        password_autofill_agent_->CreateRequestForDomain(
            element,
            AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown,
            /*form_cache=*/{}));
  }
}

void AutofillAgent::OpenTextDataListChooser(const WebInputElement& element) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  ShowSuggestions(
      element, AutofillSuggestionTriggerSource::kOpenTextDataListChooser,
      /*form_cache=*/{},
      password_autofill_agent_->CreateRequestForDomain(
          element, AutofillSuggestionTriggerSource::kOpenTextDataListChooser,
          /*form_cache=*/{}));
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
    datalist_option_change_batch_timer_.Stop();
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
  OnTextFieldValueChanged(element, /*form_cache=*/{});
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

    std::vector<FieldRendererId> filled_element_ids = base::ToVector(
        form_util::ApplyFieldsAction(document, fields, action_type,
                                     action_persistence, field_data_manager()),
        &std::pair<FieldRendererId, WebAutofillState>::first);
    std::erase_if(filled_element_ids, [](FieldRendererId filled_element_id) {
      return !form_util::GetFormControlByRendererId(filled_element_id);
    });

    // This map contains for each filled field (returned by
    // `form_util::ApplyFieldsAction()`) the corresponding current owning form.
    // This information cannot be inferred from
    // `FormFieldData::FillData::host_form_id` because after calling the filling
    // function dynamic changes can occur to the DOM.
    auto filled_fields_and_forms =
        base::MakeFlatMap<FieldRendererId, FormRendererId>(
            filled_element_ids, {},
            [](FieldRendererId filled_element_id)
                -> std::pair<FieldRendererId, FormRendererId> {
              WebFormControlElement element =
                  form_util::GetFormControlByRendererId(filled_element_id);
              CHECK(element);
              return {filled_element_id,
                      form_util::GetFormRendererId(
                          element.GetOwningFormForAutofill())};
            });

    form_tracker_->TrackAutofilledElement(filled_fields_and_forms);

    base::flat_set<FormRendererId> extracted_form_ids;
    std::vector<FormData> filled_forms;
    for (const auto& [filled_field_id, filled_form_id] :
         filled_fields_and_forms) {
      // Inform the browser about all forms that were autofilled.
      if (extracted_form_ids.insert(filled_form_id).second) {
        std::optional<FormData> form = form_util::ExtractFormData(
            document, form_util::GetFormByRendererId(filled_form_id),
            field_data_manager(), GetCallTimerState(kApplyFieldsAction),
            button_titles_cache());
        if (!form) {
          continue;
        }
        filled_forms.push_back(*form);
        if (auto* autofill_driver = unsafe_autofill_driver()) {
          CHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);
          autofill_driver->DidAutofillForm(*form);
        }
      }
    }

    // Notify Password Manager of filled fields.
    for (const auto& [filled_field_id, filled_form_id] :
         filled_fields_and_forms) {
      if (WebInputElement input_element =
              form_util::GetFormControlByRendererId(filled_field_id)
                  .DynamicTo<WebInputElement>();
          input_element && input_element.IsTextField()) {
        if (auto form_it = std::ranges::find(filled_forms, filled_form_id,
                                             &FormData::renderer_id);
            form_it != filled_forms.end()) {
          password_autofill_agent_->UpdatePasswordStateForTextChange(
              input_element, SynchronousFormCache(*form_it));
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
  CHECK(base::FeatureList::IsEnabled(
      features::debug::kAutofillShowTypePredictions));
  WebDocument document = GetDocument();
  if (!document) {
    return;
  }
  for (const auto& form : forms) {
    ShowPredictions(document, form);
  }
}

// For all elements the DOM Node ID will be exposed on the DOM
// as attribute "dom-node-id". This is done for data collection purposes.
void AutofillAgent::ExposeDomNodeIds() {
  CHECK(base::FeatureList::IsEnabled(features::debug::kShowDomNodeIDs));
  WebDocument document = GetDocument();
  if (!document) {
    return;
  }
  blink::WebElementCollection all = document.All();
  for (blink::WebElement element = all.FirstItem(); !element.IsNull();
       element = all.NextItem()) {
    element.SetAttribute(
        "dom-node-id",
        WebString::FromUTF8(base::NumberToString(element.GetDomNodeId())));
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
  for (const auto& [previewed_element_id, prior_autofill_state] :
       previewed_elements_) {
    if (WebFormControlElement field =
            form_util::GetFormControlByRendererId(previewed_element_id)) {
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
    std::optional<PasswordSuggestionRequest> password_request;
    if (auto input_element = control_element.DynamicTo<WebInputElement>()) {
      password_request =
          IsPasswordsAutofillManuallyTriggered(trigger_source)
              ? password_autofill_agent_->CreateManualFallbackRequest(
                    input_element, /*form_cache=*/{})
              : password_autofill_agent_->CreateRequestForDomain(
                    input_element, trigger_source, /*form_cache=*/{});
    }
    ShowSuggestions(control_element, trigger_source, /*form_cache=*/{},
                    password_request);
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
            previewed_elements_.emplace_back(field_id,
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
        if (form_control) {
          if (WebFormElement form_element =
                  form_control.GetOwningFormForAutofill()) {
            form_tracker_->UpdateLastInteractedElement(
                form_util::GetFormRendererId(form_element));
          } else {
            form_tracker_->UpdateLastInteractedElement(
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

bool AutofillAgent::ShouldThrottleAskForValuesToFill(FieldRendererId field) {
  static constexpr base::TimeDelta kThrottle = base::Milliseconds(100);
  base::TimeTicks now = base::TimeTicks::Now();
  if (field == last_ask_for_values_to_fill_.field &&
      now - last_ask_for_values_to_fill_.time < kThrottle) {
    return true;
  }
  last_ask_for_values_to_fill_ = {now, field};
  return false;
}

void AutofillAgent::ShowSuggestions(
    const WebFormControlElement& element,
    AutofillSuggestionTriggerSource trigger_source,
    const SynchronousFormCache& form_cache,
    const std::optional<PasswordSuggestionRequest>& password_request) {
  // TODO(crbug.com/40068004): Make this a CHECK.
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  CHECK_NE(trigger_source, AutofillSuggestionTriggerSource::kUnspecified);

  if (!element.IsEnabled() || element.IsReadOnly()) {
    return;
  }
  // Proactive password recovery can be triggered on forms that have just been
  // re-rendered and autofilled because it happens after a failed login
  // attempt. In this case the autofilled username/password fields will have
  // suggested value set.
  if (!element.SuggestedValue().IsEmpty() &&
      trigger_source !=
          AutofillSuggestionTriggerSource::kProactivePasswordRecovery) {
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

  // Password manager takes precedence over Autofill, but not about manual
  // fallbacks.
  // TODO(crbug.com/333990908): Test manual fallback on different form types.
  if (auto input_element = element.DynamicTo<WebInputElement>();
      input_element && !IsPlusAddressesManuallyTriggered(trigger_source)) {
    // Only manually triggered requests override generation requests.
    if (!IsPasswordsAutofillManuallyTriggered(trigger_source) &&
        password_generation_agent_ &&
        password_generation_agent_->ShowPasswordGenerationSuggestions(
            input_element, form_cache)) {
      is_popup_possibly_visible_ = true;
      return;
    }
    bool password_agent_handled_request = TryShowPasswordSuggestions(
        input_element, IsPasswordsAutofillManuallyTriggered(trigger_source),
        password_request);
    if (password_agent_handled_request) {
      return;
    }
  }

  if (config_.secure_context_required &&
      !element.GetDocument().IsSecureContext()) {
    LOG(WARNING) << "Autofill suggestions are disabled because the document "
                    "isn't a secure context.";
    return;
  }

  std::optional<FormAndField> form_and_field =
      form_util::FindFormAndFieldForFormControlElement(
          element, field_data_manager(),
          GetCallTimerState(kQueryAutofillSuggestions), button_titles_cache(),
          form_cache);
  if (!form_and_field) {
    return;
  }
  auto& [form, field] = *form_and_field;

  if (ShouldThrottleAskForValuesToFill(field->renderer_id())) {
    return;
  }

  is_popup_possibly_visible_ = true;
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    if (auto* render_frame = unsafe_render_frame()) {
      autofill_driver->AskForValuesToFill(form, field->renderer_id(),
                                          GetCaretBounds(*render_frame),
                                          trigger_source, password_request);
    }
  }
}

void AutofillAgent::ShowSuggestionsForContentEditable(
    const blink::WebElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  std::optional<FormData> form = form_util::FindFormForContentEditable(element);
  if (!form) {
    return;
  }

  CHECK_EQ(form->fields().size(), 1u);
  const FormFieldData& field = form->fields()[0];

  if (ShouldThrottleAskForValuesToFill(field.renderer_id())) {
    return;
  }

  if (auto* autofill_driver = unsafe_autofill_driver()) {
    is_popup_possibly_visible_ = true;
    if (auto* render_frame = unsafe_render_frame()) {
      autofill_driver->AskForValuesToFill(*form, field.renderer_id(),
                                          GetCaretBounds(*render_frame),
                                          trigger_source, std::nullopt);
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

void AutofillAgent::DispatchEmailVerifiedEvent(
    FieldRendererId field_id,
    const std::string& presentation_token) {
  if (WebFormControlElement element =
          form_util::GetFormControlByRendererId(field_id)) {
    element.DispatchEmailVerifiedEvent(WebString::FromUTF8(presentation_token));
  }
}

void AutofillAgent::DoFillFieldWithValue(std::u16string_view value,
                                         WebFormControlElement& element,
                                         WebAutofillState autofill_state) {
  DCHECK(form_util::MaybeWasOwnedByFrame(element, unsafe_render_frame()));
  element.SetAutofillValue(WebString::FromUTF16(value), autofill_state);
  UpdateStateForTextChange(element,
                           autofill_state == WebAutofillState::kAutofilled
                               ? FieldPropertiesFlags::kAutofilledOnUserTrigger
                               : FieldPropertiesFlags::kUserTyped,
                           /*form_cache=*/{});
}

void AutofillAgent::TriggerFormExtraction() {
  ExtractForms(process_forms_form_extraction_timer_, /*callback=*/{});
}

void AutofillAgent::TriggerFormExtractionWithResponse(
    base::OnceCallback<void(bool)> callback) {
  ExtractForms(process_forms_form_extraction_with_response_timer_,
               std::move(callback));
}

void AutofillAgent::ExtractFormWithField(
    FieldRendererId field_id,
    base::OnceCallback<void(const std::optional<FormData>&)> callback) {
  WebDocument document = GetDocument();
  if (!document) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (WebFormControlElement form_control =
          form_util::GetFormControlByRendererId(field_id)) {
    if (std::optional<FormData> form = form_util::ExtractFormData(
            document, form_control.GetOwningFormForAutofill(),
            field_data_manager(), GetCallTimerState(kExtractForm),
            button_titles_cache())) {
      std::move(callback).Run(std::move(form));
      return;
    }
  }
  if (WebElement contenteditable =
          form_util::GetContentEditableByRendererId(field_id)) {
    std::move(callback).Run(
        form_util::FindFormForContentEditable(contenteditable));
    return;
  }
  std::move(callback).Run(std::nullopt);
}

void AutofillAgent::ExtractLabeledTextNodeValue(
    const std::u16string& value_regex,
    const std::u16string& label_regex,
    uint32_t number_of_ancestor_levels_to_search,
    base::OnceCallback<void(const std::string&)> callback) {
  WebDocument document = GetDocument();
  if (!document) {
    std::move(callback).Run(std::string());
    return;
  }
  base::TimeTicks search_start_time = base::TimeTicks::Now();
  std::string result = form_util::ExtractFinalCheckoutAmountFromDom(
      document, base::UTF16ToUTF8(value_regex), base::UTF16ToUTF8(label_regex),
      number_of_ancestor_levels_to_search);

  base::TimeTicks search_end_time = base::TimeTicks::Now();
  base::TimeDelta renderer_search_latency = search_end_time - search_start_time;

  LogRendererExtractLabeledTextNodeValueLatency(renderer_search_latency,
                                                !result.empty());

  std::move(callback).Run(result);
}

void AutofillAgent::OnDevToolsSessionConnectionChanged(bool attached) {
  if (!attached) {
    return;
  }
  if (is_dom_content_loaded_) {
    // Re-extract all forms. Otherwise, ExtractFormsUnthrottled() would emit
    // DevTools issues only for the updated forms.
    form_cache_.Reset();
    ExtractFormsUnthrottled(
        /*callback=*/{},
        GetCallTimerState(kOnDevToolsSessionConnectionChanged));
  }
}

void AutofillAgent::EmitFormIssuesToDevtools() {
  // TODO(crbug.com/1399414,crbug.com/1444566): Throttle this call if possible.
  ExtractFormsUnthrottled(/*callback=*/{},
                          GetCallTimerState(kEmitFormIssuesToDevtools));
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
                             base::Unretained(this), std::move(callback),
                             GetCallTimerState(kExtractForms)));
}

void AutofillAgent::ExtractFormsAndNotifyPasswordAutofillAgent(
    base::OneShotTimer& timer,
    const WebElement& element) {
  if (!is_dom_content_loaded_ || timer.IsRunning()) {
    return;
  }

  timer.Start(
      FROM_HERE, kFormsSeenThrottle,
      base::BindOnce(
          &AutofillAgent::ExtractFormsUnthrottled, base::Unretained(this),
          base::BindOnce(
              [](PasswordAutofillAgent* password_autofill_agent,
                 FormCache* form_cache, bool success) {
                if (success) {
                  password_autofill_agent->OnDynamicFormsSeen(
                      SynchronousFormCache(form_cache->extracted_forms()));
                }
              },
              base::Unretained(password_autofill_agent_.get()),
              base::Unretained(&form_cache_)),
          GetCallTimerState(kExtractFormsAndNotifyPasswordAutofillAgent)));
}

void AutofillAgent::ExtractFormsUnthrottled(
    base::OnceCallback<void(bool)> callback,
    const CallTimerState& timer_state) {
  content::RenderFrame* render_frame = unsafe_render_frame();
  if (!render_frame) {
    if (!callback.is_null()) {
      std::move(callback).Run(/*success=*/false);
    }
    return;
  }

  FormCache::UpdateFormCacheResult cache =
      form_cache_.UpdateFormCache(field_data_manager(), timer_state);
  if (!cache.updated_forms.empty() || !cache.removed_forms.empty()) {
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->FormsSeen(cache.updated_forms,
                                 std::move(cache.removed_forms).extract());
    }
  }
  if (!callback.is_null()) {
    std::move(callback).Run(/*success=*/true);
  }

  if (WebLocalFrame& frame = *render_frame->GetWebFrame();
      frame.IsInspectorConnected()) {
    form_issues::EmitFormIssues(frame.GetDocument(), cache.updated_forms,
                                &form_issues::EmitToDevTools);
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

  // The browser code also calls `ClearPreviewedForm` on hiding the popup.
  // However, there can be instances in which the render frame is already
  // switched out by the time this call happens. If that render frame is later
  // retrieved from BFCache, the preview is still active. Clearing preview here
  // prevents that.
  ClearPreviewedForm();
  if (auto* autofill_driver = unsafe_autofill_driver()) {
    autofill_driver->HidePopup();
  }
}

void AutofillAgent::DidChangeFormRelatedElementDynamically(
    const WebElement& element,
    WebFormRelatedChangeType form_related_change) {
  auto should_handle_event = [&] {
    if (!is_dom_content_loaded_) {
      // When the agent receives the DomContentLoaded signal, it will extract
      // all forms and notify PasswordAutofillAgent by default, so we do not
      // need to run this function as this would be redundant.
      return false;
    }
    // Early bailout for node removal.
    if (form_related_change == blink::WebFormRelatedChangeType::kRemove &&
        !replace_form_element_observer_) {
      return false;
    }
    auto maybe_control_element = element.DynamicTo<WebFormControlElement>();
    const bool is_autofillable_element =
        element.DynamicTo<WebFormElement>() ||
        (maybe_control_element &&
         form_util::IsAutofillableElement(maybe_control_element) &&
         !IsCheckableElement(maybe_control_element));
    switch (form_related_change) {
      case blink::WebFormRelatedChangeType::kAdd:
      case blink::WebFormRelatedChangeType::kRemove:
      case blink::WebFormRelatedChangeType::kReassociate:
        // If the element dynamically added is not a form element or
        // autofillable control element (see condition above), it will probably
        // not have any influence on Autofill at all, and therefore there's no
        // need to trigger DOM re-extraction on any other case.
        return is_autofillable_element;
      case blink::WebFormRelatedChangeType::kHide:
        // Hidden elements have a slightly different behavior, since they don't
        // lead to form extraction. Here, we are also interested in input
        // elements that have type 'hidden', which are not autofillable, but are
        // one way to hide previously autofillable elements.
        return is_autofillable_element ||
               (maybe_control_element &&
                maybe_control_element.FormControlTypeForAutofill() ==
                    blink::mojom::FormControlType::kInputHidden);
    }
  };
  if (!should_handle_event()) {
    return;
  }

  switch (form_related_change) {
    case blink::WebFormRelatedChangeType::kAdd:
    case blink::WebFormRelatedChangeType::kReassociate:
      ExtractFormsAndNotifyPasswordAutofillAgent(
          process_forms_after_dynamic_change_timer_, element);
      break;
    case blink::WebFormRelatedChangeType::kRemove:
      // Autofill currently notifies the browser of additions but not of
      // deletions, see crbug.com/356236098#comment10 for further details.
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

  if ((!config_.uses_keyboard_accessory_for_suggestions ||
       base::FeatureList::IsEnabled(
           features::kAutofillAndroidKeyboardAccessoryDynamicPositioning)) &&
      config_.focus_requires_scroll) {
    HandleFocusChangeComplete(
        /*focused_node_was_last_clicked=*/
        last_left_mouse_down_or_gesture_tap_in_node_caused_focus_,
        /*form_cache=*/{});
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
  if (base::FeatureList::IsEnabled(
          features::kAutofillAndroidKeyboardAccessoryDynamicPositioning)) {
    last_left_mouse_down_or_gesture_tap_in_node_caused_focus_ = is_focused;
  } else {
    HandleFocusChangeComplete(/*focused_node_was_last_clicked=*/is_focused,
                              /*form_cache=*/{});
  }
#else
  last_left_mouse_down_or_gesture_tap_in_node_caused_focus_ = is_focused;
#endif
}

void AutofillAgent::SelectControlSelectionChanged(
    const WebFormControlElement& element) {
  if (WebDocument document = GetDocument();
      !document || !document.GetFrame() ||
      !document.GetFrame()->HasTransientUserActivation() ||
      element.IsAutofilled()) {
    // This filtering is an approximation of "the user manually edited the
    // field". The reason is that some JS value-change events could be the
    // result of a user edit to a custom select field tied to a hidden select
    // element.
    return;
  }
  form_tracker_->SelectControlSelectionChanged(element);
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

  FieldRendererId element_id = form_util::GetFieldRendererId(element);
  base::OneShotTimer& timer =
      base::FeatureList::IsEnabled(
          features::kAutofillSplitTimersForSelectOptionChanges)
          ? select_option_change_batch_timer_[element_id]
          : select_option_change_batch_timer_[FieldRendererId()];

  if (timer.IsRunning()) {
    timer.Stop();
  }
  timer.Start(FROM_HERE, kWaitTimeForOptionsChanges,
              base::BindRepeating(&AutofillAgent::BatchSelectOptionChange,
                                  base::Unretained(this), element_id));
}

void AutofillAgent::BatchSelectOptionChange(FieldRendererId element_id) {
  select_option_change_batch_timer_.erase(element_id);
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
              button_titles_cache(), /*form_cache=*/{})) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver();
        autofill_driver && !field->options().empty()) {
      autofill_driver->SelectFieldOptionsDidChange(form, field->renderer_id());
    }
  }
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
    bool focused_node_was_last_clicked,
    const SynchronousFormCache& form_cache) {
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
    std::optional<PasswordSuggestionRequest> password_request;
    if (auto input_element = focused_control.DynamicTo<WebInputElement>()) {
      password_request = password_autofill_agent_->CreateRequestForDomain(
          input_element,
          focused_node_was_last_clicked
              ? AutofillSuggestionTriggerSource::kFormControlElementClicked
              : AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick,
          form_cache);
    }
    if (focused_node_was_last_clicked) {
      was_last_action_fill_ = false;
      ShowSuggestions(
          focused_control,
          AutofillSuggestionTriggerSource::kFormControlElementClicked,
          form_cache, password_request);
    } else if (form_util::IsTextAreaElement(focused_control)) {
#if !BUILDFLAG(IS_ANDROID)
      // Compose reacts to tab area focus even when not triggered by a click -
      // therefore call `ShowSuggestions` with a separate trigger source.
      ShowSuggestions(
          focused_control,
          AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick,
          form_cache, password_request);
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
  form_tracker_->OnJavaScriptChangedValue(element);

  if (!was_autofilled) {
    return;
  }
  if (std::optional<FormAndField> form_and_field =
          form_util::FindFormAndFieldForFormControlElement(
              element, field_data_manager(),
              GetCallTimerState(kJavaScriptChangedValue), button_titles_cache(),
              /*form_cache=*/{})) {
    auto& [form, field] = *form_and_field;
    if (auto* autofill_driver = unsafe_autofill_driver()) {
      autofill_driver->JavaScriptChangedAutofilledValue(
          form, field->renderer_id(), old_value.Utf16());
    }
  }
}

void AutofillAgent::TrackAutofilledElement(
    const WebFormControlElement& element) {
  form_tracker_->TrackAutofilledElement(element);
}

void AutofillAgent::UpdateStateForTextChange(
    const WebFormControlElement& element,
    FieldPropertiesFlags flag,
    const SynchronousFormCache& form_cache) {
  const auto input_element = element.DynamicTo<WebInputElement>();
  if (!input_element || !input_element.IsTextField()) {
    return;
  }

  field_data_manager_->UpdateFieldDataMap(
      form_util::GetFieldRendererId(element), element.Value().Utf16(), flag);

  password_autofill_agent_->UpdatePasswordStateForTextChange(input_element,
                                                             form_cache);
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
