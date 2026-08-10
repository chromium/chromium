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
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "content/public/renderer/render_frame.h"
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
  if (base::IsAsciiControl(event.text[0])) {
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
    const WebFormControlElement& element) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return false;
  }
  if (element.FormControlTypeForAutofill() ==
      blink::mojom::FormControlType::kInputPassword) {
    return false;
  }
  const RendererPreferences* renderer_prefs = GetRendererPreferences();
  if (!renderer_prefs || renderer_prefs->autofill_trigger_string.empty()) {
    return false;
  }
  // TODO(crbug.com/494158096): Add WebString::EndsWith().
  const WebString trigger =
      WebString::FromUtf8(renderer_prefs->autofill_trigger_string);
  const unsigned int sel_start = element.SelectionStart();
  const unsigned int sel_end = element.SelectionEnd();
  return sel_start == sel_end && sel_start >= trigger.length() &&
         element.EditingValue()
             .Substring(sel_start - trigger.length(), trigger.length())
             .Equals(trigger);
}

bool AtMemoryHandler::ShouldTriggerAtMemorySearchForContentEditable(
    WebLocalFrame* frame,
    const WebRange& selection) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return false;
  }
  const RendererPreferences* renderer_prefs = GetRendererPreferences();
  if (!renderer_prefs || renderer_prefs->autofill_trigger_string.empty()) {
    return false;
  }
  // TODO(crbug.com/494158096): Add WebString::EndsWith().
  const WebString trigger =
      WebString::FromUtf8(renderer_prefs->autofill_trigger_string);
  const int trigger_len = std::max(static_cast<int>(trigger.length()), 0);
  const int sel_start = selection.StartOffset();
  const int sel_end = selection.EndOffset();
  return sel_start == sel_end && sel_start >= trigger_len &&
         frame->RangeAsText(WebRange(sel_start - trigger_len, trigger_len))
             .Equals(trigger);
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
  if (!agent_->unsafe_render_frame()) {
    return false;
  }
  WebLocalFrame* frame = agent_->unsafe_render_frame()->GetWebFrame();
  if (ShouldTriggerAtMemorySearchForContentEditable(
          frame, frame->GetInputMethodController()->GetSelectionOffsets())) {
    agent_->ShowSuggestionsForContentEditable(
        element, AutofillSuggestionTriggerSource::kAtMemoryTriggerString);
    return true;
  }
  return false;
}

bool AtMemoryHandler::DidReceiveKeyDown(const WebElement& element,
                                        const WebKeyboardEvent& event) {
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

bool AtMemoryHandler::PreviewReplaceSelectionForAtMemory(
    WebFormControlElement& form_control,
    const std::u16string& value) {
  const std::optional<AtMemoryHandler::AskForValuesToFillInfo> info =
      FindAskForValuesToFill(form_control,
                             /*pop=*/false);
  if (!info) {
    return false;
  }

  const blink::RendererPreferences* prefs = GetRendererPreferences();
  WebString trigger =
      WebString::FromUtf8(prefs ? prefs->autofill_trigger_string : "");
  const unsigned int sel_start = form_control.SelectionStart();
  const unsigned int sel_end = form_control.SelectionEnd();
  std::u16string preview_value = form_control.EditingValue().Utf16();
  // If there is no selection and the cursor is immediately preceded
  // by the trigger string, we replace the trigger. Otherwise (e.g. if
  // the user has already selected text or triggered via the context
  // menu), we replace the current selection or insert at the cursor.
  if (info->caused_by_trigger_string && !trigger.IsEmpty() &&
      sel_start == sel_end && sel_start >= trigger.length() &&
      form_control.EditingValue()
          .Substring(sel_start - trigger.length(), trigger.length())
          .Equals(trigger)) {
    preview_value.replace(sel_start - trigger.length(), trigger.length(),
                          value);
  } else {
    preview_value.replace(sel_start, sel_end - sel_start, value);
  }
  form_control.SetSuggestedValue(WebString::FromUtf16(preview_value));
  return true;
}

void AtMemoryHandler::ReplaceSelectionForAtMemory(
    WebFormControlElement& form_control,
    const std::u16string& value) {
  const std::optional<AskForValuesToFillInfo> info =
      FindAskForValuesToFill(form_control, /*pop=*/true);
  if (!info) {
    return;
  }

  const RendererPreferences* prefs = GetRendererPreferences();
  WebString trigger =
      WebString::FromUtf8(prefs ? prefs->autofill_trigger_string : "");
  const unsigned int sel_start = form_control.SelectionStart();
  const unsigned int sel_end = form_control.SelectionEnd();
  // If there is no selection and the cursor is immediately preceded
  // by the trigger string, we select the trigger so it gets replaced
  // by `PasteText` below. Otherwise (e.g. if the user has already
  // selected text or triggered via context menu), we just perform
  // a regular insertion/replacement at the current position.
  if (info->caused_by_trigger_string && !trigger.IsEmpty() &&
      sel_start == sel_end && sel_start >= trigger.length() &&
      form_control.EditingValue()
          .Substring(sel_start - trigger.length(), trigger.length())
          .Equals(trigger)) {
    form_control.SetSelectionRange(sel_start - trigger.length(), sel_start);
  }
  form_control.PasteText(WebString::FromUtf16(value),
                         /*replace_all=*/false,
                         /*smart_replace=*/true);
}

void AtMemoryHandler::ReplaceSelectionForAtMemoryForContentEditable(
    WebElement& content_editable,
    const std::u16string& value) {
  const std::optional<AskForValuesToFillInfo> info =
      FindAskForValuesToFill(content_editable, /*pop=*/true);
  if (!info) {
    return;
  }

  if (info->caused_by_trigger_string) {
    if (auto* frame = agent_->unsafe_render_frame()) {
      WebRange selection = frame->GetWebFrame()
                               ->GetInputMethodController()
                               ->GetSelectionOffsets();
      if (ShouldTriggerAtMemorySearchForContentEditable(frame->GetWebFrame(),
                                                        selection)) {
        const RendererPreferences* prefs = GetRendererPreferences();
        WebString trigger =
            WebString::FromUtf8(prefs ? prefs->autofill_trigger_string : "");
        int offset = selection.StartOffset();
        int trigger_len = std::max(static_cast<int>(trigger.length()), 0);
        frame->GetWebFrame()->SetEditableSelectionOffsets(offset - trigger_len,
                                                          offset);
      }
    }
  }
  content_editable.PasteText(WebString::FromUtf16(value),
                             /*replace_all=*/false,
                             /*smart_replace=*/true);
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

  WebString value = [&] {
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

  WebString value = [&] {
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

}  // namespace autofill
