// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/at_memory_handler.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/synchronous_form_cache.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/signatures.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

namespace {

using ::blink::RendererPreferences;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebFormElement;
using ::blink::WebKeyboardEvent;
using ::blink::WebLocalFrame;
using ::blink::WebRange;
using ::blink::WebString;

// Returns true if `event` may produce a character.
bool IsPrintable(const WebKeyboardEvent& event) {
  if (base::IsAsciiControl(event.text[0]) || event.text[1] != 0) {
    return false;
  }
  if constexpr (BUILDFLAG(IS_MAC)) {
    // On Mac, Meta+X is not printable but leads to `event.text[0] != 'X'`.
    return !(event.GetModifiers() & blink::WebInputEvent::kMetaKey);
  }
  return true;
}

}  // namespace

AtMemoryHandler::AtMemoryHandler(AutofillAgent* agent)
    : agent_(CHECK_DEREF(agent)) {}

AtMemoryHandler::~AtMemoryHandler() = default;

// AtMemory should be triggered if the field is not a password field, no text is
// selected and the cursor is located behind the trigger string.
bool AtMemoryHandler::ShouldTriggerAtMemorySearch(
    const WebElement& element) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return false;
  }

  const WebString trigger = WebString::FromUtf8(GetTriggerString());
  if (trigger.IsEmpty()) {
    return false;
  }

  const auto form_control = element.DynamicTo<WebFormControlElement>();
  if (form_util::GetAutofillFormControlType(form_control) ==
          FormControlType::kInputPassword ||
      element.DynamicTo<WebFormElement>()) {
    return false;
  }

  const int trigger_len = std::max(static_cast<int>(trigger.length()), 0);

  if (form_control) {
    const unsigned int sel_start = form_control.SelectionStart();
    const unsigned int sel_end = form_control.SelectionEnd();
    return sel_start == sel_end && sel_start >= trigger.length() &&
           form_control.EditingValue()
               .Substring(sel_start - trigger.length(), trigger.length())
               .Equals(trigger);
  }

  if (auto* frame = agent_->unsafe_render_frame()) {
    const WebRange selection =
        frame->GetWebFrame()->GetInputMethodController()->GetSelectionOffsets();
    const int sel_start = selection.StartOffset();
    const int sel_end = selection.EndOffset();
    return sel_start == sel_end && sel_start >= trigger_len &&
           frame->GetWebFrame()
               ->RangeAsText(WebRange(sel_start - trigger_len, trigger_len))
               .Equals(trigger);
  }

  return false;
}

bool AtMemoryHandler::OnTextFieldValueChanged(
    const WebFormControlElement& element,
    const SynchronousFormCache& form_cache) {
  if (ShouldTriggerAtMemorySearch(element)) {
    agent_->ShowSuggestions(
        element, AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        form_cache, std::nullopt);
    return true;
  }
  return false;
}

bool AtMemoryHandler::ContentEditableDidChange(const WebElement& element) {
  if (ShouldTriggerAtMemorySearch(element)) {
    agent_->ShowSuggestionsForContentEditable(
        element, AutofillSuggestionTriggerSource::kAtMemoryTriggerString);
    return true;
  }
  return false;
}

bool AtMemoryHandler::DidReceiveKeyDown(const WebElement& element,
                                        const WebKeyboardEvent& event) {
  MaybeRecordAtAt(
      element, event, agent_->field_data_manager(),
      agent_->GetCallTimerState(CallTimerState::CallSite::kDidReceiveKeyDown),
      agent_->button_titles_cache());

  const RendererPreferences* prefs = GetRendererPreferences();
  if (!prefs || prefs->autofill_shortcut_key_code == ui::VKEY_UNKNOWN ||
      !base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryTriggerShortcut)) {
    return false;
  }

  // The configured keyboard shortcut opens the Autofill AtMemory popup.
  const ui::Accelerator expected_accelerator(
      prefs->autofill_shortcut_key_code, prefs->autofill_shortcut_modifiers);
  const ui::Accelerator actual_accelerator(
      static_cast<ui::KeyboardCode>(event.windows_key_code),
      ui::WebEventModifiersToEventFlags(event.GetModifiers()));

  if (expected_accelerator == actual_accelerator && !IsPrintable(event)) {
    if (auto control = element.DynamicTo<WebFormControlElement>();
        control && form_util::IsTextAreaElementOrTextInput(control) &&
        control.FormControlTypeForAutofill() !=
            blink::mojom::FormControlType::kInputPassword) {
      if (!actual_accelerator.IsRepeat()) {
        agent_->ShowSuggestions(
            control, AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut,
            SynchronousFormCache(), std::nullopt);
      }
      return true;  // Prevent default.
    } else if (element.IsContentEditable()) {
      if (!actual_accelerator.IsRepeat()) {
        agent_->ShowSuggestionsForContentEditable(
            element,
            AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut);
      }
      return true;  // Prevent default.
    }
  }
  return false;
}

void AtMemoryHandler::ReplaceSelectionForAtMemory(WebElement& element,
                                                  const std::u16string& value) {
  const std::optional<AskForValuesToFillInfo> info =
      FindAskForValuesToFill(element, /*pop=*/true);
  if (!info) {
    return;
  }

  if (info->caused_by_trigger_string && ShouldTriggerAtMemorySearch(element)) {
    // TODO(crbug.com/538102446): Instead of adjusting the selection, eliminate
    // the trigger string.
    const WebString trigger = WebString::FromUtf8(GetTriggerString());
    const int trigger_len = std::max(static_cast<int>(trigger.length()), 0);
    if (auto form_control = element.DynamicTo<WebFormControlElement>()) {
      const unsigned int offset = form_control.SelectionStart();
      form_control.SetSelectionRange(offset - trigger_len, offset);
    } else if (auto* frame = agent_->unsafe_render_frame()) {
      const WebRange selection = frame->GetWebFrame()
                                     ->GetInputMethodController()
                                     ->GetSelectionOffsets();
      const int offset = selection.StartOffset();
      frame->GetWebFrame()->SetEditableSelectionOffsets(offset - trigger_len,
                                                        offset);
    }
  }

  element.PasteText(WebString::FromUtf16(value),
                    /*replace_all=*/false,
                    /*smart_replace=*/false);
}

std::optional<AtMemoryHandler::AskForValuesToFillInfo>
AtMemoryHandler::FindAskForValuesToFill(const WebElement& element, bool pop) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!element.DynamicTo<WebFormElement>());
  auto it = std::ranges::find(last_at_memory_ask_for_values_to_fills_,
                              form_util::GetFieldRendererId(element),
                              &AskForValuesToFillInfo::field_id);
  if (it == last_at_memory_ask_for_values_to_fills_.end()) {
    return std::nullopt;
  }
  AskForValuesToFillInfo info = *it;
  if (pop) {
    last_at_memory_ask_for_values_to_fills_.erase(it);
  }

  const WebString value = [&] {
    if (auto form_control = element.DynamicTo<WebFormControlElement>()) {
      return form_control.Value();
    }
    return element.TextContent();
  }();
  if (info.value_hash != base::FastHash(base::as_byte_span(value.Utf16()))) {
    return std::nullopt;
  }

  return info;
}

void AtMemoryHandler::MaybeUpdateAskForValuesToFill(
    const WebElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!element.DynamicTo<WebFormElement>());
  if (!IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }

  FindAskForValuesToFill(element, /*pop=*/true);

  static constexpr size_t kMaxSize = 10;
  while (last_at_memory_ask_for_values_to_fills_.size() >= kMaxSize) {
    last_at_memory_ask_for_values_to_fills_.pop_front();
  }

  const WebString value = [&] {
    if (auto form_control = element.DynamicTo<WebFormControlElement>()) {
      return form_control.Value();
    }
    return element.TextContent();
  }();

  last_at_memory_ask_for_values_to_fills_.push_back(AskForValuesToFillInfo{
      .field_id = form_util::GetFieldRendererId(element),
      .caused_by_trigger_string =
          trigger_source ==
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
      .value_hash = base::FastHash(base::as_byte_span(value.Utf16()))});
}

ukm::UkmRecorder* AtMemoryHandler::GetUkmRecorder() {
  if (!ukm_recorder_) {
    mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
    content::RenderThread::Get()->BindHostReceiver(
        factory.BindNewPipeAndPassReceiver());
    ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
  }
  return ukm_recorder_.get();
}

void AtMemoryHandler::MaybeRecordAtAt(
    const WebElement& element,
    const WebKeyboardEvent& event,
    const FieldDataManager& field_data_manager,
    const CallTimerState& timer_state,
    form_util::ButtonTitlesCache* button_titles_cache) {
  constexpr base::TimeDelta kAtAtThreshold = base::Milliseconds(500);

  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  if (element.DynamicTo<WebFormElement>()) {
    return;
  }

  if (base::IsAsciiControl(event.text[0])) {
    return;
  }

  if (event.text[0] != u'@' || event.text[1] != 0 ||
      (event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat)) {
    last_at_key_press_ = {};
    return;
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  if (last_at_key_press_.time.is_null() ||
      now - last_at_key_press_.time > kAtAtThreshold ||
      last_at_key_press_.field != form_util::GetFieldRendererId(element)) {
    last_at_key_press_ = {now, form_util::GetFieldRendererId(element)};
    return;
  }
  last_at_key_press_ = {};

  const ukm::SourceId source_id = element && element.GetDocument()
                                      ? element.GetDocument().GetUkmSourceId()
                                      : ukm::kInvalidSourceId;
  ukm::UkmRecorder* recorder = GetUkmRecorder();
  if (!recorder || source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::Autofill_AtAtPressed builder(source_id);

  auto set_metrics = [&](const FormData& form, const FormFieldData& field) {
    builder.SetFormSignature(HashFormSignature(CalculateFormSignature(form)));
    builder.SetFieldSignature(
        HashFieldSignature(CalculateFieldSignatureForField(field)));
    builder.SetFormControlType(std::to_underlying(field.form_control_type()));
    if (WebLocalFrame* frame = element.GetDocument().GetFrame()) {
      const FieldRendererId field_id = field.renderer_id();
      const blink::LocalFrameToken frame_token = frame->GetLocalFrameToken();
      builder.SetFieldSessionIdentifier(StrToHash64Bit(
          base::NumberToString(field_id.value()) + frame_token.ToString()));
    }
  };

  if (WebFormControlElement control =
          element.DynamicTo<WebFormControlElement>()) {
    if (std::optional<form_util::FormAndField> form_and_field =
            form_util::FindFormAndFieldForFormControlElement(
                control, field_data_manager, timer_state, button_titles_cache,
                /*form_cache=*/{})) {
      const auto& [form, field] = *form_and_field;
      set_metrics(form, field);
    }
  } else if (element && element.IsContentEditable()) {
    if (std::optional<FormData> form =
            form_util::FindFormForContentEditable(element)) {
      if (!form->fields().empty()) {
        set_metrics(*form, form->fields().front());
      }
    }
  }

  builder.Record(recorder);
}

const RendererPreferences* AtMemoryHandler::GetRendererPreferences() const {
  if (auto* frame = agent_->unsafe_render_frame()) {
    if (auto* web_frame = frame->GetWebFrame()) {
      if (auto* view = web_frame->View()) {
        return &view->GetRendererPreferences();
      }
    }
  }
  return nullptr;
}

const std::string& AtMemoryHandler::GetTriggerString() const {
  const blink::RendererPreferences* prefs = GetRendererPreferences();
  if (!prefs) {
    return base::EmptyString();
  }
  return prefs->autofill_trigger_string;
}

}  // namespace autofill
