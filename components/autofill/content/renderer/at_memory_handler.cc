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
#include "base/strings/utf_string_conversion_utils.h"
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
using ::blink::WebNode;
using ::blink::WebRange;
using ::blink::WebString;

// If more time than this happens between two keystrokes, they're not considered
// as belonging to the same coherent input (e.g., trigger string).
constexpr base::TimeDelta kCoherentKeyDownThreshold = base::Milliseconds(500);

// Returns true if `event` may produce a character.
bool IsPrintable(const WebKeyboardEvent& event) {
  if (base::IsAsciiControl(event.text[0]) || event.text[1] != 0) {
    return false;
  }
  if constexpr (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)) {
    // On Linux and Windows, Alt+X is not printable.
    return !(event.GetModifiers() & blink::WebInputEvent::kAltKey);
  }
  if constexpr (BUILDFLAG(IS_MAC)) {
    // On Mac, Meta+X is not printable but leads to `event.text[0] != 'X'`.
    return !(event.GetModifiers() & blink::WebInputEvent::kMetaKey);
  }
  return true;
}

bool IsModifierKey(const WebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    case ui::VKEY_SHIFT:
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
    case ui::VKEY_MENU:
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
    case ui::VKEY_ALTGR:
    case ui::VKEY_LWIN:  // VKEY_LWIN is an alias Mac's VKEY_COMMAND.
    case ui::VKEY_RWIN:
    case ui::VKEY_CAPITAL:
    case ui::VKEY_NUMLOCK:
    case ui::VKEY_SCROLL:
      return true;
    default:
      return false;
  }
}

}  // namespace

AtMemoryHandler::AtMemoryHandler(AutofillAgent* agent)
    : agent_(CHECK_DEREF(agent)) {}

AtMemoryHandler::~AtMemoryHandler() = default;

std::optional<AtMemoryHandler::CaretInfo> AtMemoryHandler::GetCaretInfo(
    const WebElement& element) const {
  // TODO(crbug.com/545987198): Consider re-adding `element.Focused()`.
  if (!element || !element.ContainsFrameSelection() ||
      element.DynamicTo<WebFormElement>() ||
      !form_util::IsAccessible(element)) {
    return std::nullopt;
  }

  if (const auto form_control = element.DynamicTo<WebFormControlElement>();
      form_util::IsTextAreaElementOrTextInput(form_control) &&
      form_util::GetAutofillFormControlType(form_control) !=
          FormControlType::kInputPassword &&
      form_control.IsEnabled() && !form_control.IsReadOnly()) {
    unsigned int begin = form_control.SelectionStart();
    unsigned int end = form_control.SelectionEnd();
    if (begin == end) {
      return CaretInfo{FieldType::kTextTypeFormControl, begin};
    }
  } else if (element.IsContentEditable()) {
    if (auto* render_frame = agent_->unsafe_render_frame()) {
      const WebRange selection = render_frame->GetWebFrame()
                                     ->GetInputMethodController()
                                     ->GetSelectionOffsets();
      int begin = selection.StartOffset();
      int end = selection.EndOffset();
      if (begin == end && begin >= 0) {
        return CaretInfo{FieldType::kContentEditable,
                         static_cast<size_t>(begin)};
      }
    }
  }
  return std::nullopt;
}

bool AtMemoryHandler::ShouldTriggerAtMemorySearch(
    const WebElement& element) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return false;
  }

  const WebString trigger = WebString::FromUtf8(GetTriggerString());
  if (trigger.IsEmpty()) {
    return false;
  }

  std::optional<CaretInfo> info = GetCaretInfo(element);
  if (!info) {
    return false;
  }

  switch (info->field_type) {
    case FieldType::kTextTypeFormControl:
      return info->offset >= trigger.length() &&
             element.DynamicTo<WebFormControlElement>()
                 .EditingValue()
                 .Substring(info->offset - trigger.length(), trigger.length())
                 .Equals(trigger);
    case FieldType::kContentEditable:
      if (auto* frame = agent_->unsafe_render_frame()) {
        return info->offset >= trigger.length() &&
               frame->GetWebFrame()
                   ->RangeAsText(WebRange(info->offset - trigger.length(),
                                          trigger.length()))
                   .Equals(trigger);
      }
      break;
  }
  return false;
}

bool AtMemoryHandler::DidReceiveKeyDown(const WebElement& element,
                                        const WebKeyboardEvent& event) {
  MaybeRecordAtAt(
      element, event, agent_->field_data_manager(),
      agent_->GetCallTimerState(CallTimerState::CallSite::kDidReceiveKeyDown),
      agent_->button_titles_cache());

  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return false;
  }
  if (DidReceiveKeyDownForAtMemoryShortcut(element, event)) {
    return true;
  }
  DidReceiveKeyDownForAtMemoryTriggerString(element, event);
  return false;
}

bool AtMemoryHandler::DidReceiveKeyDownForAtMemoryShortcut(
    const WebElement& element,
    const WebKeyboardEvent& event) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryTriggerShortcut)) {
    return false;
  }

  const RendererPreferences* prefs = GetRendererPreferences();
  if (!prefs || prefs->autofill_shortcut_key_code == ui::VKEY_UNKNOWN) {
    return false;
  }

  // The configured keyboard shortcut opens the Autofill AtMemory popup.
  const ui::Accelerator expected_accelerator(
      prefs->autofill_shortcut_key_code, prefs->autofill_shortcut_modifiers);
  const ui::Accelerator actual_accelerator(
      static_cast<ui::KeyboardCode>(event.windows_key_code),
      ui::WebEventModifiersToEventFlags(event.GetModifiers()));
  if (expected_accelerator != actual_accelerator || IsPrintable(event)) {
    return false;
  }

  if (auto control = element.DynamicTo<WebFormControlElement>();
      control && form_util::IsTextAreaElementOrTextInput(control) &&
      control.FormControlTypeForAutofill() !=
          blink::mojom::FormControlType::kInputPassword &&
      control.IsEnabled() && !control.IsReadOnly()) {
    if (!actual_accelerator.IsRepeat()) {
      agent_->ShowSuggestions(
          control, AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut,
          SynchronousFormCache(), std::nullopt);
    }
    return true;  // Prevent default.
  } else if (element.IsContentEditable()) {
    if (!actual_accelerator.IsRepeat()) {
      agent_->ShowSuggestionsForContentEditable(
          element, AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut);
    }
    return true;  // Prevent default.
  }
  return false;
}

void AtMemoryHandler::DidReceiveKeyDownForAtMemoryTriggerString(
    const WebElement& element,
    const WebKeyboardEvent& event) {
  if (IsModifierKey(event)) {
    return;
  }

  if (!IsPrintable(event) ||
      (event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat)) {
    trigger_state_ = {};
    return;
  }

  const std::string& trigger = GetTriggerString();
  if (trigger.empty()) {
    trigger_state_ = {};
    return;
  }

  const std::optional<CaretInfo> info = GetCaretInfo(element);
  if (!info) {
    trigger_state_ = {};
    return;
  }

  const FieldRendererId element_id = form_util::GetFieldRendererId(element);
  const base::TimeTicks now = base::TimeTicks::Now();

  auto is_plausible_offset = [](size_t last_offset, size_t current_offset) {
    // Characters are not guaranteed to occur in the field.
    // For example, non-numeric characters are suppressed in <input
    // type=number>.
    return last_offset == current_offset ||
           (last_offset + 1 == current_offset &&
            last_offset < std::string::npos);
  };

  if (trigger_state_.last_element_id != element_id ||
      !is_plausible_offset(trigger_state_.last_offset, info->offset) ||
      now - trigger_state_.last_time > kCoherentKeyDownThreshold) {
    trigger_state_ = {};
  }

  base::WriteUnicodeCharacter(event.text[0], &trigger_state_.seen_trigger);

  // Truncate the seen trigger so that it is a prefix of the expected trigger.
  while (!trigger_state_.seen_trigger.empty() &&
         !trigger.starts_with(trigger_state_.seen_trigger)) {
    trigger_state_.seen_trigger.erase(0, 1);
  }
  DCHECK(trigger.starts_with(trigger_state_.seen_trigger));

  if (trigger_state_.seen_trigger.empty()) {
    trigger_state_ = {};
    return;
  }

  trigger_state_ = {.seen_trigger = trigger_state_.seen_trigger,
                    .last_time = now,
                    .last_element_id = element_id,
                    .last_offset = info->offset};
  if (trigger != trigger_state_.seen_trigger) {
    // The trigger string isn't complete yet.
    return;
  }

  // The trigger string is complete. We trigger AtMemory suggestions.
  trigger_state_ = {};

  // The character produced by this keydown event, if there is any, has not been
  // appended to the field yet. The character is added synchronously after this
  // event.
  //
  // We call AutofillAgent::ShowSuggestions() and
  // AutofillAgent::ShowSuggestionsForContentEditable() asynchronously to give
  // Blink time to add the character to the field. This is important because
  // AutofillAgent calls MaybeUpdateAskForValuesToFill(), which takes a hash of
  // the field value.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AtMemoryHandler> self,
             const FieldRendererId element_id) {
            if (!self) {
              return;
            }
            WebElement element =
                WebNode::FromDomNodeId(*element_id).DynamicTo<WebElement>();
            std::optional<CaretInfo> info = self->GetCaretInfo(element);
            if (!info) {
              return;
            }
            switch (info->field_type) {
              case FieldType::kTextTypeFormControl:
                self->agent_->ShowSuggestions(
                    element.DynamicTo<WebFormControlElement>(),
                    AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
                    SynchronousFormCache(), std::nullopt);
                break;
              case FieldType::kContentEditable:
                self->agent_->ShowSuggestionsForContentEditable(
                    element,
                    AutofillSuggestionTriggerSource::kAtMemoryTriggerString);
                break;
            }
          },
          weak_ptr_factory_.GetWeakPtr(), element_id));
}

void AtMemoryHandler::FocusedElementChanged(
    const WebElement& new_focused_element) {
  trigger_state_ = {};
}

void AtMemoryHandler::DidReceiveLeftMouseDownOrGestureTapInNode(
    const blink::WebNode& node) {
  trigger_state_ = {};
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
    if (auto form_control = element.DynamicTo<WebFormControlElement>()) {
      const size_t offset = form_control.SelectionStart();
      if (offset >= trigger.length()) {
        form_control.SetSelectionRange(offset - trigger.length(), offset);
      }
    } else if (auto* frame = agent_->unsafe_render_frame()) {
      const WebRange selection = frame->GetWebFrame()
                                     ->GetInputMethodController()
                                     ->GetSelectionOffsets();
      const size_t offset =
          base::saturated_cast<size_t>(selection.StartOffset());
      if (offset >= trigger.length()) {
        frame->GetWebFrame()->SetEditableSelectionOffsets(
            base::saturated_cast<int>(offset - trigger.length()), offset);
      }
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
      now - last_at_key_press_.time > kCoherentKeyDownThreshold ||
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
