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
    case ui::VKEY_RIGHT_COMMAND:
    case ui::VKEY_CAPITAL:
    case ui::VKEY_NUMLOCK:
    case ui::VKEY_SCROLL:
      return true;
    default:
      return false;
  }
}

bool IsSingleCtrlKey(const WebKeyboardEvent& event) {
  switch (event.windows_key_code) {
#if BUILDFLAG(IS_MAC)
    // On Mac, we use Command instead of Ctrl.
    case ui::VKEY_COMMAND:
    case ui::VKEY_RIGHT_COMMAND:
      return (event.GetModifiers() & WebKeyboardEvent::kKeyModifiers) ==
             WebKeyboardEvent::kMetaKey;
#else
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
      return (event.GetModifiers() & WebKeyboardEvent::kKeyModifiers) ==
             WebKeyboardEvent::kControlKey;
#endif
    default:
      return false;
  }
}

// Returns true if `field` is fillable by AtMemory.
bool IsSupportedField(const WebElement& field) {
  if (!field || !form_util::IsAccessible(field)) {
    return false;
  }
  if (const auto form_control = field.DynamicTo<WebFormControlElement>()) {
    return form_util::IsTextAreaElementOrTextInput(form_control) &&
           form_util::GetAutofillFormControlType(form_control) !=
               FormControlType::kInputPassword &&
           form_control.IsEnabled() && !form_control.IsReadOnly();
  }
  return field.IsContentEditable() && !field.DynamicTo<WebFormElement>();
}

// Returns the offset of the caret in `field`.
// Returns std::string::npos if `field` is not fillable by AtMemory: if it
// not focused, not a text-type form control or contenteditable, or there is a
// non-empty text selection.
size_t GetCaretOffset(const WebElement& field) {
  if (!IsSupportedField(field) || !field.ContainsFrameSelection()) {
    return std::string::npos;
  }

  WebLocalFrame* frame = field.GetDocument().GetFrame();
  if (!frame) {
    return std::string::npos;
  }

  const WebRange selection =
      frame->GetInputMethodController()->GetSelectionOffsets();
  const int begin = selection.StartOffset();
  const int end = selection.EndOffset();
  if (begin != end || begin < 0) {
    return std::string::npos;
  }
  return static_cast<size_t>(begin);
}

size_t HashFieldValue(const WebElement& field) {
  const WebString value = [&] {
    if (auto form_control = field.DynamicTo<WebFormControlElement>()) {
      return form_control.Value();
    }
    return field.TextContent();
  }();
  return base::FastHash(base::as_byte_span(value.Utf16()));
}

}  // namespace

AtMemoryHandler::AtMemoryHandler(AutofillAgent* agent)
    : agent_(CHECK_DEREF(agent)) {}

AtMemoryHandler::~AtMemoryHandler() = default;

// Returns true if the trigger string occurs before the caret in `field`.
bool AtMemoryHandler::HasTriggerStringNextToCaret(
    const WebElement& field) const {
  const WebString trigger = WebString(GetTriggerString());
  if (trigger.IsEmpty()) {
    return false;
  }
  const size_t offset = GetCaretOffset(field);
  if (offset == std::string::npos) {
    return false;
  }
  WebLocalFrame* frame = field.GetDocument().GetFrame();
  if (!frame) {
    return false;
  }
  return offset >= trigger.length() &&
         frame
             ->RangeAsText(
                 WebRange(base::saturated_cast<int>(offset - trigger.length()),
                          base::saturated_cast<int>(trigger.length())))
             .Equals(trigger);
}

bool AtMemoryHandler::DidReceiveKeyDown(const WebElement& field,
                                        const WebKeyboardEvent& event) {
  MaybeRecordAtAt(
      field, event, agent_->field_data_manager(),
      agent_->GetCallTimerState(CallTimerState::CallSite::kDidReceiveKeyDown),
      agent_->button_titles_cache());

  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return false;
  }
  if (DidReceiveKeyDownForTriggerShortcut(field, event)) {
    return true;
  }
  DidReceiveKeyDownForTriggerString(field, event);
  DidReceiveKeyDownForDoubleCtrl(field, event);
  return false;
}

bool AtMemoryHandler::DidReceiveKeyDownForTriggerShortcut(
    const WebElement& field,
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

  if (auto form_control = field.DynamicTo<WebFormControlElement>();
      form_control && form_util::IsTextAreaElementOrTextInput(form_control) &&
      form_control.FormControlTypeForAutofill() !=
          blink::mojom::FormControlType::kInputPassword &&
      form_control.IsEnabled() && !form_control.IsReadOnly()) {
    if (!actual_accelerator.IsRepeat()) {
      agent_->ShowSuggestions(
          form_control,
          AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut,
          SynchronousFormCache(), std::nullopt);
    }
    return true;  // Prevent default.
  } else if (field.IsContentEditable()) {
    if (!actual_accelerator.IsRepeat()) {
      agent_->ShowSuggestionsForContentEditable(
          field, AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut);
    }
    return true;  // Prevent default.
  }
  return false;
}

void AtMemoryHandler::DidReceiveKeyDownForTriggerString(
    const WebElement& field,
    const WebKeyboardEvent& event) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemoryTriggerString)) {
    return;
  }

  if (IsModifierKey(event)) {
    return;
  }

  if (!IsPrintable(event) ||
      (event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat)) {
    trigger_state_ = {};
    return;
  }

  const std::u16string& trigger = GetTriggerString();
  if (trigger.empty()) {
    trigger_state_ = {};
    return;
  }

  const size_t offset = GetCaretOffset(field);
  if (offset == std::string::npos) {
    trigger_state_ = {};
    return;
  }

  const FieldRendererId field_id = form_util::GetFieldRendererId(field);
  const base::TimeTicks now = base::TimeTicks::Now();

  auto is_plausible_offset = [](size_t last_offset, size_t current_offset) {
    // Characters are not guaranteed to occur in the field.
    // For example, non-numeric characters are suppressed in <input
    // type=number>.
    return last_offset == current_offset ||
           (last_offset + 1 == current_offset &&
            last_offset < std::string::npos);
  };

  if (trigger_state_.last_field_id != field_id ||
      !is_plausible_offset(trigger_state_.last_offset, offset) ||
      now - trigger_state_.last_time > kCoherentKeyDownThreshold) {
    trigger_state_ = {};
  }

  trigger_state_.seen_trigger.push_back(event.text[0]);

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
                    .last_field_id = field_id,
                    .last_offset = offset};
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
             const FieldRendererId field_id) {
            if (!self) {
              return;
            }
            const WebElement field =
                WebNode::FromDomNodeId(*field_id).DynamicTo<WebElement>();
            if (!IsSupportedField(field)) {
              return;
            }
            if (auto form_control = field.DynamicTo<WebFormControlElement>()) {
              self->agent_->ShowSuggestions(
                  form_control,
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
                  SynchronousFormCache(), std::nullopt);
            } else {
              DCHECK(field.IsContentEditable());
              self->agent_->ShowSuggestionsForContentEditable(
                  field,
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), field_id));
}

void AtMemoryHandler::DidReceiveKeyDownForDoubleCtrl(
    const WebElement& field,
    const WebKeyboardEvent& event) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemoryDoubleCtrl)) {
    return;
  }

  if (!IsSingleCtrlKey(event) ||
      (event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat)) {
    ctrl_state_ = {};
    return;
  }

  if (const RendererPreferences* prefs = GetRendererPreferences();
      !prefs || prefs->autofill_shortcut_key_code != ui::VKEY_UNKNOWN) {
    // The double Ctrl trigger is mutually exclusive with the configurable
    // keyboard shortcut.
    return;
  }

  if (!IsSupportedField(field) || !field.ContainsFrameSelection()) {
    ctrl_state_ = {};
    return;
  }

  const FieldRendererId field_id = form_util::GetFieldRendererId(field);
  const base::TimeTicks now = base::TimeTicks::Now();

  if (ctrl_state_.last_ctrl_dom_code != event.dom_code ||
      ctrl_state_.last_field_id != field_id ||
      now - ctrl_state_.last_time > kCoherentKeyDownThreshold) {
    ctrl_state_ = {.last_ctrl_dom_code = event.dom_code,
                   .last_time = now,
                   .last_field_id = field_id};
    // The double Ctrl sequence isn't complete yet.
    return;
  }

  // The double Ctrl sequence is complete. We trigger AtMemory suggestions.
  ctrl_state_ = {};

  if (auto form_control = field.DynamicTo<WebFormControlElement>()) {
    agent_->ShowSuggestions(
        form_control, AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl,
        SynchronousFormCache(), std::nullopt);
  } else {
    DCHECK(field.IsContentEditable());
    agent_->ShowSuggestionsForContentEditable(
        field, AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl);
  }
}

void AtMemoryHandler::FocusedElementChanged(
    const WebElement& new_focused_element) {
  trigger_state_ = {};
  ctrl_state_ = {};
}

void AtMemoryHandler::DidReceiveLeftMouseDownOrGestureTapInNode(
    const blink::WebNode& node) {
  trigger_state_ = {};
  ctrl_state_ = {};
}

void AtMemoryHandler::ReplaceSelectionForAtMemory(WebElement field,
                                                  const std::u16string& value) {
  const std::optional<AskForValuesToFillInfo> info =
      ExtractAskForValuesToFill(field);
  if (!info) {
    return;
  }
  WaitForFocusAndReplaceSelectionForAtMemory(*std::move(info), value,
                                             /*num_try=*/0);
}

void AtMemoryHandler::WaitForFocusAndReplaceSelectionForAtMemory(
    AskForValuesToFillInfo info,
    std::u16string value,
    int num_try) {
  constexpr int kMaxRetries = 5;
  constexpr base::TimeDelta kDelayBeforeRetry = base::Milliseconds(20);

  WebElement field =
      WebNode::FromDomNodeId(*info.field_id).DynamicTo<WebElement>();
  if (!field || info.value_hash != HashFieldValue(field)) {
    return;
  }

  // Ensures that `field.GetDocument().FocusedElement() == field` and that
  // SetEditableSelectionOffsets() and ExtendSelectionAndReplace() operate on
  // that field -- assuming that no JavaScript `focus` listener or similar
  // moved the focus elsewhere.
  field.Focus();

  // While `field.Focus()` sets the page-level focus, the window-level focus may
  // still be with the AtMemory popup in the browser process. In that case,
  // `field.Focused()` is false. If so, we wait `kMaxRetries - num_try` times
  // for it to become true. If the focus still hasn't arrived afterwards, we
  // just try to fill the field without window focus.
  if (!field.Focused() && num_try < kMaxRetries) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AtMemoryHandler::WaitForFocusAndReplaceSelectionForAtMemory,
            weak_ptr_factory_.GetWeakPtr(), std::move(info), std::move(value),
            num_try + 1),
        kDelayBeforeRetry);
    return;
  }

  WebLocalFrame* frame = field.GetDocument().GetFrame();
  if (!frame) {
    return;
  }

  if (!info.selection_range.IsNull()) {
    // Restores the text selection at the time of AskForValuesToFill().
    // When AtMemory was triggered with the trigger string, the selection is
    // normally empty (but JavaScript may have interfered).
    // When AtMemory was triggered by the context menu or keyboard shortcut, it
    // may be non-empty and filling should replace the selected text.
    frame->SetEditableSelectionOffsets(info.selection_range.StartOffset(),
                                       info.selection_range.EndOffset());
  }

  int offset = 0;
  if (info.caused_by_trigger_string && HasTriggerStringNextToCaret(field)) {
    offset = GetTriggerString().size();
  }

  frame->ExtendSelectionAndReplace(/*before=*/offset,
                                   /*after=*/0, WebString::FromUtf16(value));
}

std::optional<AtMemoryHandler::AskForValuesToFillInfo>
AtMemoryHandler::ExtractAskForValuesToFill(const WebElement& field) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!field.DynamicTo<WebFormElement>());
  auto it = std::ranges::find(last_at_memory_ask_for_values_to_fills_,
                              form_util::GetFieldRendererId(field),
                              &AskForValuesToFillInfo::field_id);
  if (it == last_at_memory_ask_for_values_to_fills_.end()) {
    return std::nullopt;
  }
  AskForValuesToFillInfo info = *it;
  last_at_memory_ask_for_values_to_fills_.erase(it);
  return info;
}

void AtMemoryHandler::MaybeUpdateAskForValuesToFill(
    const WebElement& field,
    AutofillSuggestionTriggerSource trigger_source) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!field.DynamicTo<WebFormElement>());
  if (!IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }

  ExtractAskForValuesToFill(field);

  constexpr size_t kMaxSize = 10;
  while (last_at_memory_ask_for_values_to_fills_.size() >= kMaxSize) {
    last_at_memory_ask_for_values_to_fills_.pop_front();
  }

  WebLocalFrame* frame = field.GetDocument().GetFrame();

  last_at_memory_ask_for_values_to_fills_.push_back(AskForValuesToFillInfo{
      .field_id = form_util::GetFieldRendererId(field),
      .caused_by_trigger_string =
          trigger_source ==
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
      .value_hash = HashFieldValue(field),
      .selection_range =
          frame ? frame->GetInputMethodController()->GetSelectionOffsets()
                : WebRange()});
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
    const WebElement& field,
    const WebKeyboardEvent& event,
    const FieldDataManager& field_data_manager,
    const CallTimerState& timer_state,
    form_util::ButtonTitlesCache* button_titles_cache) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  if (field.DynamicTo<WebFormElement>()) {
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
      last_at_key_press_.field != form_util::GetFieldRendererId(field)) {
    last_at_key_press_ = {now, form_util::GetFieldRendererId(field)};
    return;
  }
  last_at_key_press_ = {};

  const ukm::SourceId source_id = field && field.GetDocument()
                                      ? field.GetDocument().GetUkmSourceId()
                                      : ukm::kInvalidSourceId;
  ukm::UkmRecorder* recorder = GetUkmRecorder();
  if (!recorder || source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::Autofill_AtAtPressed builder(source_id);

  auto set_metrics = [&](const FormData& form_data,
                         const FormFieldData& field_data) {
    builder.SetFormSignature(
        HashFormSignature(CalculateFormSignature(form_data)));
    builder.SetFieldSignature(
        HashFieldSignature(CalculateFieldSignatureForField(field_data)));
    builder.SetFormControlType(
        std::to_underlying(field_data.form_control_type()));
    if (WebLocalFrame* frame = field.GetDocument().GetFrame()) {
      const FieldRendererId field_id = field_data.renderer_id();
      const blink::LocalFrameToken frame_token = frame->GetLocalFrameToken();
      builder.SetFieldSessionIdentifier(StrToHash64Bit(
          base::NumberToString(field_id.value()) + frame_token.ToString()));
    }
  };

  if (WebFormControlElement form_control =
          field.DynamicTo<WebFormControlElement>()) {
    if (std::optional<form_util::FormAndField> form_and_field =
            form_util::FindFormAndFieldForFormControlElement(
                form_control, field_data_manager, timer_state,
                button_titles_cache,
                /*form_cache=*/{})) {
      set_metrics(form_and_field->form, form_and_field->field);
    }
  } else if (field && field.IsContentEditable()) {
    if (std::optional<FormData> form_data =
            form_util::FindFormForContentEditable(field)) {
      if (!form_data->fields().empty()) {
        set_metrics(*form_data, form_data->fields().front());
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

const std::u16string& AtMemoryHandler::GetTriggerString() const {
  const blink::RendererPreferences* prefs = GetRendererPreferences();
  if (!prefs) {
    return base::EmptyString16();
  }
  return prefs->autofill_trigger_string;
}

}  // namespace autofill
